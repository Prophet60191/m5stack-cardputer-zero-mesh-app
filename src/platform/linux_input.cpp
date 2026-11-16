#include "linux_input.h"

#include <array>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <optional>

#if !USE_DESKTOP || defined(MESH_INPUT_TESTING)
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#ifndef APP_KEY_INPUT_DEVICE
#define APP_KEY_INPUT_DEVICE ""
#endif

namespace platform {
namespace {

std::array<lv_obj_t*, kNavKeyCount> nav_buttons{};
uint32_t last_key = 0;
bool last_key_pressed = false;
bool nav_shortcut_mode = false;
// Mode is queried at each key, including after an Enter starts composing in
// the same evdev batch. The setter remains useful for simple standalone apps.
bool text_entry_mode = false;
TextEntryQuery text_entry_query = nullptr;
void* text_entry_query_data = nullptr;
uint32_t pressed_key = 0;
uint32_t press_started_at = 0;
bool long_press_sent = false;
bool pressed_key_consumed = false;
lv_timer_t* long_press_timer = nullptr;
KeyReleaseListener key_release_listener = nullptr;
void* key_release_listener_user_data = nullptr;
GlobalKeyListener global_key_listener = nullptr;
void* global_key_listener_user_data = nullptr;

constexpr uint32_t kLongPressMs = 700;

#if !USE_DESKTOP || defined(MESH_INPUT_TESTING)
struct EvdevKeypad {
    int fd{-1};
    lv_indev_state_t state{LV_INDEV_STATE_RELEASED};
    uint32_t key{0};
    KeyboardDecoder decoder;
    std::optional<KeyboardDecoder::Event> pending;
    bool dropped{false};
    explicit EvdevKeypad(KeyboardDecoder::Profile profile) : decoder(profile) {}
};
#endif

size_t nav_key_to_index(uint32_t key) {
    if (nav_shortcut_mode) {
        switch (key) {
            case LV_KEY_ESC:
                return 4;
            case 'z':
            case 'Z':
            case LV_KEY_LEFT:
                return 1;
            case 'c':
            case 'C':
            case LV_KEY_RIGHT:
                return 3;
            default:
                return kNavKeyCount;
        }
    }

    switch (key) {
        case '4':
        case LV_KEY_ESC:
            return 0;
        case '5':
            return 1;
        case '6':
            return 2;
        case '7':
            return 3;
        case '8':
            return 4;
        default:
            return kNavKeyCount;
    }
}

void dispatch_nav_key(uint32_t key) {
    const auto index = nav_key_to_index(key);
    if (index >= nav_buttons.size()) {
        return;
    }

    auto* button = nav_buttons[index];
    if (!button || !lv_obj_is_valid(button) || !lv_obj_has_flag(button, LV_OBJ_FLAG_CLICKABLE)) {
        return;
    }

    lv_obj_send_event(button, LV_EVENT_CLICKED, nullptr);
}

bool emit_global_key(uint32_t key, bool long_pressed) {
    return global_key_listener &&
           global_key_listener(key, describe_key(key), long_pressed, global_key_listener_user_data);
}

void long_press_timer_cb(lv_timer_t*) {
    if (!pressed_key || long_press_sent || lv_tick_elaps(press_started_at) < kLongPressMs) {
        return;
    }

    long_press_sent = true;
    if (emit_global_key(pressed_key, true)) {
        pressed_key_consumed = true;
    }
}

void ensure_long_press_timer() {
    if (!long_press_timer) {
        long_press_timer = lv_timer_create(long_press_timer_cb, 40, nullptr);
    }
    if (long_press_timer) {
        lv_timer_resume(long_press_timer);
        lv_timer_reset(long_press_timer);
    }
}

bool route_key_state(uint32_t key, bool pressed) {
    // Releasing an older rollover key must not cancel a newer key's hold.
    if (!pressed && pressed_key != key) return false;
    const bool consumed_release = !pressed && pressed_key_consumed;
    const bool new_hold = pressed && (!last_key_pressed || pressed_key != key);
    if (new_hold) {
        pressed_key = key;
        press_started_at = lv_tick_get();
        long_press_sent = false;
        pressed_key_consumed = emit_global_key(key, false);
        ensure_long_press_timer();
        if (!pressed_key_consumed) {
            dispatch_nav_key(key);
        }
    }
    else if (!pressed && last_key_pressed && pressed_key == key) {
        if (key_release_listener) {
            key_release_listener(key, describe_key(key), key_release_listener_user_data);
        }
        pressed_key = 0;
        pressed_key_consumed = false;
        if (long_press_timer) {
            lv_timer_pause(long_press_timer);
        }
    }

    last_key = key;
    last_key_pressed = pressed;
    return pressed ? pressed_key_consumed : consumed_release;
}

#if !USE_DESKTOP || defined(MESH_INPUT_TESTING)
lv_group_t* keypad_group = nullptr;

lv_group_t* ensure_keypad_group() {
    if (!keypad_group) {
        keypad_group = lv_group_create();
        lv_group_set_default(keypad_group);
    }
    return keypad_group;
}

bool key_down(const unsigned long* bits, unsigned code) {
    constexpr auto width=sizeof(unsigned long)*8;
    return (bits[code/width] & (1UL<<(code%width)))!=0;
}

void restore_modifier_state(EvdevKeypad& keypad) {
    unsigned long bits[(KEY_MAX/(sizeof(unsigned long)*8))+1]{};
    if (ioctl(keypad.fd,EVIOCGKEY(sizeof(bits)),bits)>=0)
        keypad.decoder.restore_modifiers(key_down(bits,KEY_LEFTSHIFT),key_down(bits,KEY_RIGHTSHIFT),
            key_down(bits,KEY_LEFTCTRL),key_down(bits,KEY_RIGHTCTRL),key_down(bits,KEY_LEFTALT),key_down(bits,KEY_RIGHTALT));
}

void cancel_hold() {
    pressed_key=last_key=0;
    last_key_pressed=pressed_key_consumed=long_press_sent=false;
    if (long_press_timer) lv_timer_pause(long_press_timer);
}

void load_system_map(KeyboardDecoder& decoder) {
    // Fixed root-owned system source, never an intent/config-supplied path.
    const int fd=open("/usr/share/keymaps/tca8418_keypad_m5stack_keymap.map", O_RDONLY|O_CLOEXEC|O_NOFOLLOW);
    if (fd<0) return; // The complete pinned official map is compiled in.
    struct stat st{};
    char buffer[8193];
    const bool trusted=fstat(fd,&st)==0 && S_ISREG(st.st_mode) && st.st_uid==0 && !(st.st_mode&0022);
    const ssize_t length=trusted ? read(fd,buffer,sizeof(buffer)) : -1;
    close(fd);
    std::string error;
    if (length<=0 || length>8192 || !decoder.load_symbol_map(std::string_view(buffer,static_cast<size_t>(length)),error))
        LV_LOG_WARN("unavailable/invalid system keyboard map; using complete built-in map");
}

bool has_nav_keys(int fd) {
    unsigned long key_bits[(KEY_MAX / (sizeof(unsigned long) * 8)) + 1] = {};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(key_bits)), key_bits) < 0) {
        return false;
    }

    auto has_key = [&](int code) {
        const auto bits_per_word = static_cast<int>(sizeof(unsigned long) * 8);
        return (key_bits[code / bits_per_word] & (1UL << (code % bits_per_word))) != 0;
    };

    return has_key(KEY_ESC) || has_key(KEY_LEFT) || has_key(KEY_RIGHT) || has_key(KEY_Z) ||
           has_key(KEY_C) || has_key(KEY_4) || has_key(KEY_5) || has_key(KEY_6) ||
           has_key(KEY_7) || has_key(KEY_8) || has_key(KEY_SYSRQ) || has_key(KEY_HELP);
}

void evdev_read_cb(lv_indev_t* indev, lv_indev_data_t* data) {
    auto* keypad=static_cast<EvdevKeypad*>(lv_indev_get_driver_data(indev));
    data->continue_reading=false;
    if (!keypad || keypad->fd<0) { data->state=LV_INDEV_STATE_RELEASED; return; }
    while (true) {
        std::optional<KeyboardDecoder::Event> decoded;
        if (keypad->pending) { decoded=keypad->pending; keypad->pending.reset(); }
        else {
            input_event input{};
            const auto size=read(keypad->fd,&input,sizeof(input));
            if (size<0 && errno==EINTR) continue;
            if (size<0 && (errno==EAGAIN || errno==EWOULDBLOCK)) break;
            if (size!=sizeof(input)) {
                keypad->decoder.reset(); keypad->state=LV_INDEV_STATE_RELEASED;
                cancel_hold();
                if (keypad->fd>=0) { close(keypad->fd); keypad->fd=-1; }
                break;
            }
            if (input.type==EV_SYN && input.code==SYN_DROPPED) {
                keypad->dropped=true; keypad->decoder.reset(); cancel_hold();
                keypad->state=LV_INDEV_STATE_RELEASED;
                break; // release LVGL too; never treat a lost Esc as a click
            }
            if (keypad->dropped) {
                if (input.type==EV_SYN && input.code==SYN_REPORT) {
                    keypad->dropped=false; restore_modifier_state(*keypad);
                }
                continue;
            }
            if (input.type!=EV_KEY) continue;
            const bool editing=text_entry_query ? text_entry_query(text_entry_query_data) : text_entry_mode;
            decoded=keypad->decoder.feed(input.code,input.value,editing);
        }
        if (!decoded || decoded->repeat) continue; // LVGL owns repeat timing.
        if (!decoded->pressed && decoded->key!=keypad->key) {
            route_key_state(decoded->key,false); // old rollover key; not an LVGL release of the newer key
            continue;
        }
        if (decoded->pressed && keypad->state==LV_INDEV_STATE_PRESSED && decoded->key!=keypad->key) {
            // LVGL is a single-key indev. Insert a logical release before another
            // physical key's press; do not synthesize an application Esc release.
            keypad->pending=decoded; keypad->state=LV_INDEV_STATE_RELEASED;
            data->continue_reading=true;
            break;
        }
        keypad->key=decoded->key;
        data->key=decoded->key;
        data->state=decoded->pressed ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
        route_key_input(data);
        // Unbound non-text function keys never become invalid Unicode in a draft.
        if (is_action_key(data->key)) data->state=LV_INDEV_STATE_RELEASED;
        keypad->state=data->state;
        data->continue_reading=true;
        return;
    }
    data->key=keypad->key; data->state=keypad->state;
}

void evdev_delete_cb(lv_event_t* event) {
    auto* indev = static_cast<lv_indev_t*>(lv_event_get_target(event));
    auto* keypad = static_cast<EvdevKeypad*>(lv_indev_get_driver_data(indev));
    if (!keypad) {
        return;
    }

    if (keypad->fd >= 0) {
        close(keypad->fd);
    }
    delete keypad;
}

lv_indev_t* create_keypad_from_fd(int fd, KeyboardDecoder::Profile profile) {
    auto* keypad = new EvdevKeypad(profile);
    keypad->fd = fd;
    if (profile==KeyboardDecoder::Profile::CardputerZero) load_system_map(keypad->decoder);
    restore_modifier_state(*keypad);

    auto* indev = lv_indev_create();
    if (!indev) {
        delete keypad;
        close(fd);
        return nullptr;
    }

    lv_indev_set_type(indev, LV_INDEV_TYPE_KEYPAD);
    lv_indev_set_read_cb(indev, evdev_read_cb);
    lv_indev_set_driver_data(indev, keypad);
    lv_indev_set_group(indev, ensure_keypad_group());
    lv_indev_add_event_cb(indev, evdev_delete_cb, LV_EVENT_DELETE, nullptr);
    return indev;
}

lv_indev_t* try_create_keypad(const char* path) {
    if (!path || path[0] == '\0') {
        return nullptr;
    }

    const int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        LV_LOG_WARN("failed to open input device %s: %s", path, strerror(errno));
        return nullptr;
    }

    if (!has_nav_keys(fd)) {
        close(fd);
        return nullptr;
    }

    LV_LOG_INFO("using evdev key input %s", path);
    char name[128]{};
    ioctl(fd,EVIOCGNAME(sizeof(name)),name);
    name[sizeof(name)-1]='\0';
    const auto profile=KeyboardDecoder::is_cardputer(name) ? KeyboardDecoder::Profile::CardputerZero : KeyboardDecoder::Profile::UsKeyboard;
    return create_keypad_from_fd(fd,profile);
}

void discover_keypads(lv_display_t* display) {
    const char* configured_device = APP_KEY_INPUT_DEVICE;
    if (configured_device[0] != '\0') {
        auto* indev = try_create_keypad(configured_device);
        if (indev) {
            lv_indev_set_display(indev, display);
        }
        return;
    }

    auto* dir = opendir("/dev/input");
    if (!dir) {
        LV_LOG_WARN("failed to open /dev/input: %s", strerror(errno));
        return;
    }

    while (auto* entry = readdir(dir)) {
        if (std::strncmp(entry->d_name, "event", 5) != 0) {
            continue;
        }

        std::string path = "/dev/input/";
        path += entry->d_name;
        auto* indev = try_create_keypad(path.c_str());
        if (indev) {
            lv_indev_set_display(indev, display);
        }
    }

    closedir(dir);
}
#endif

} // namespace

void init_key_input(lv_display_t* display) {
#if !USE_DESKTOP || defined(MESH_INPUT_TESTING)
    discover_keypads(display);
#else
    LV_UNUSED(display);
#endif
}

void route_key_input(lv_indev_data_t* data) {
    if (route_key_state(data->key,data->state==LV_INDEV_STATE_PRESSED))
        data->state=LV_INDEV_STATE_RELEASED;
}

void set_text_entry_query(TextEntryQuery query, void* user_data) {
    text_entry_query=query; text_entry_query_data=user_data;
}

void clear_text_entry_query(TextEntryQuery query, void* user_data) {
    if (text_entry_query==query && text_entry_query_data==user_data) {
        text_entry_query=nullptr; text_entry_query_data=nullptr;
    }
}

#if defined(MESH_INPUT_TESTING)
lv_indev_t* make_test_evdev(int fd, bool cardputer) {
    return create_keypad_from_fd(fd,cardputer ? KeyboardDecoder::Profile::CardputerZero : KeyboardDecoder::Profile::UsKeyboard);
}
#endif

void set_nav_shortcut_mode(bool enabled) {
    nav_shortcut_mode = enabled;
}

void set_text_entry_mode(bool enabled) {
    text_entry_mode = enabled;
}

void register_nav_button(size_t index, lv_obj_t* button) {
    if (index >= nav_buttons.size()) {
        return;
    }

    nav_buttons[index] = button;
}

void unregister_nav_button(size_t index, lv_obj_t* button) {
    if (index >= nav_buttons.size()) {
        return;
    }

    if (!button || nav_buttons[index] == button) {
        nav_buttons[index] = nullptr;
    }
}

void set_key_release_listener(KeyReleaseListener listener, void* user_data) {
    key_release_listener = listener;
    key_release_listener_user_data = user_data;
}

void clear_key_release_listener(KeyReleaseListener listener, void* user_data) {
    if (key_release_listener == listener && key_release_listener_user_data == user_data) {
        key_release_listener = nullptr;
        key_release_listener_user_data = nullptr;
    }
}

void set_global_key_listener(GlobalKeyListener listener, void* user_data) {
    global_key_listener = listener;
    global_key_listener_user_data = user_data;
}

void clear_global_key_listener(GlobalKeyListener listener, void* user_data) {
    if (global_key_listener == listener && global_key_listener_user_data == user_data) {
        global_key_listener = nullptr;
        global_key_listener_user_data = nullptr;
    }
}

const char* describe_key(uint32_t key) {
    switch (key) {
        case LV_KEY_ESC:
            return "Esc";
        case LV_KEY_LEFT:
            return "Left";
        case LV_KEY_RIGHT:
            return "Right";
        case LV_KEY_UP:
            return "Up";
        case LV_KEY_DOWN:
            return "Down";
        case LV_KEY_ENTER:
            return "Enter";
        case LV_KEY_BACKSPACE:
            return "Backspace";
        case LV_KEY_NEXT:
            return "Tab";
        case kKeyPrintScreen:
            return "PrintScreen";
        case kKeyHelp:
            return "Help";
        default:
            break;
    }

    static char buffer[24];
    if (key >= 32 && key <= 126) {
        std::snprintf(buffer, sizeof(buffer), "%c", static_cast<char>(key));
    }
    else {
        std::snprintf(buffer, sizeof(buffer), "Key_%u", key);
    }
    return buffer;
}

} // namespace platform
