// SPDX-License-Identifier: MIT
#include "keyboard_decoder.h"
#include "linux_input.h"
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <map>
#include <vector>

#ifdef MESH_DEVICE_INPUT_TESTS
#include <linux/input.h>
#include <fcntl.h>
#include <unistd.h>
namespace platform { lv_indev_t* make_test_evdev(int fd, bool cardputer); }
#endif

#define REQUIRE(x) do { if (!(x)) { std::cerr << "keyboard failure at " << __LINE__ << ": " << #x << '\n'; std::abort(); } } while (false)
using Decoder=platform::KeyboardDecoder;

static std::string fixture_path=MESH_KEYMAP_FIXTURE;

static std::string fixture() {
    std::ifstream file(fixture_path);
    REQUIRE(file.good());
    return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};
}

static const std::map<std::string,char>& expected_symbols() {
    static const std::map<std::string,char> names={
        {"exclam",'!'},{"at",'@'},{"numbersign",'#'},{"dollar",'$'},{"percent",'%'},
        {"asciicircum",'^'},{"ampersand",'&'},{"asterisk",'*'},{"parenleft",'('},
        {"parenright",')'},{"asciitilde",'~'},{"grave",'`'},{"underscore",'_'},
        {"minus",'-'},{"plus",'+'},{"equal",'='},{"bracketleft",'['},{"bracketright",']'},
        {"braceleft",'{'},{"braceright",'}'},{"semicolon",';'},{"colon",':'},
        {"apostrophe",'\''},{"quotedbl",'"'},{"less",'<'},{"greater",'>'},
        {"backslash",'\\'},{"bar",'|'},{"comma",','},{"period",'.'},{"slash",'/'},{"question",'?'}
    };
    return names;
}

static void decoder_checks() {
    // Independent names/expected characters; keycode/name assignments come from
    // the original vendor fixture, rather than another copy of our code table.
    const auto& names=expected_symbols();
    Decoder decoder;
    int count=0;
    std::istringstream input(fixture());
    for (std::string line;std::getline(input,line);) {
        std::istringstream row(line);std::string keyword,equal,name;unsigned code;
        if (!(row>>keyword>>code>>equal>>name) || keyword!="keycode") continue;
        REQUIRE(names.count(name));
        // Hold, released-before-character (one-shot), locked, and app startup
        // after Sym was already activated all use the driver-selected symbol.
        for (int mode=0;mode<4;++mode) {
            decoder.reset();
            if (mode==0) decoder.feed(383,1,true);
            if (mode==1 || mode==2) {decoder.feed(383,1,true);decoder.feed(383,0,true);}
            if (mode==2) {decoder.feed(383,1,true);decoder.feed(383,0,true);}
            auto down=decoder.feed(code,1,true);REQUIRE(down && down->key==static_cast<uint32_t>(names.at(name)));
            decoder.feed(383,0,true);
            auto repeat=decoder.feed(code,2,true);REQUIRE(repeat && repeat->repeat && repeat->key==down->key);
            auto up=decoder.feed(code,0,false);REQUIRE(up && up->key==down->key && !up->pressed);
        }
        ++count;
    }
    REQUIRE(count==32);
    std::string error;
    REQUIRE(decoder.load_symbol_map(fixture(),error));
    auto malformed=fixture()+"\nkeycode 92 = slash\n";
    REQUIRE(!decoder.load_symbol_map(malformed,error));
    REQUIRE(!decoder.load_symbol_map("keycode 92 = slash\n",error));
    REQUIRE(!decoder.load_symbol_map(std::string(8193,'x'),error));
    REQUIRE(!decoder.load_symbol_map(std::string("bad\0map",7),error));
    auto invalid=fixture();auto at=invalid.find("slash");REQUIRE(at!=invalid.npos);invalid.replace(at,5,"Return");
    REQUIRE(!decoder.load_symbol_map(invalid,error));
    // A rejected map never replaces the last complete one.
    REQUIRE(decoder.feed(92,1,true)->key=='/');decoder.feed(92,0,true);
    auto custom=fixture();at=custom.find("keycode 92 = slash");REQUIRE(at!=custom.npos);custom.replace(at,18,"keycode 92 = colon");
    REQUIRE(decoder.load_symbol_map(custom,error));REQUIRE(decoder.feed(92,1,true)->key==':');decoder.feed(92,0,true);

    Decoder usb(Decoder::Profile::UsKeyboard),other;
    REQUIRE(usb.feed(26,1,true)->key=='[');usb.feed(26,0,true);
    REQUIRE(other.feed(26,1,true)->key=='!');other.feed(26,0,true);
    REQUIRE(!usb.feed(383,1,true));REQUIRE(usb.feed(53,1,true)->key=='/');usb.feed(53,0,true);
    REQUIRE(!usb.feed(42,1,true));REQUIRE(usb.feed(30,1,true)->key=='A');
    usb.feed(42,0,true);REQUIRE(usb.feed(30,0,true)->key=='A');
    REQUIRE(other.feed(30,1,true)->key=='a');other.feed(30,0,true); // per-device state
    usb.feed(42,1,true);usb.feed(54,1,true);usb.feed(42,0,true);
    REQUIRE(usb.feed(30,1,true)->key=='A');usb.feed(30,0,true);usb.feed(54,0,true);
    REQUIRE(usb.feed(30,1,true)->key=='a');usb.feed(30,0,true);
    REQUIRE(usb.feed(33,1,false)->key==LV_KEY_UP);REQUIRE(usb.feed(33,0,true)->key==LV_KEY_UP);
    REQUIRE(usb.feed(33,1,true)->key=='f');usb.feed(33,0,true);
    for (auto item : std::vector<std::pair<uint16_t,uint32_t>>{{103,LV_KEY_UP},{108,LV_KEY_DOWN},{105,LV_KEY_LEFT},
          {106,LV_KEY_RIGHT},{102,LV_KEY_HOME},{107,LV_KEY_END},{111,LV_KEY_DEL},
          {104,platform::kKeyPageUp},{109,platform::kKeyPageDown},{110,platform::kKeyInsert},
          {138,platform::kKeyHelp},{99,platform::kKeyPrintScreen},{59,platform::kKeyFunction1},
          {88,platform::kKeyFunction1+11}}) {
        auto down=other.feed(item.first,1,true);REQUIRE(down && down->key==item.second);
        REQUIRE(other.feed(item.first,0,false)->key==item.second);
    }
    other.restore_modifiers(true,false);REQUIRE(other.feed(30,1,true)->key=='A');other.feed(30,0,true);other.reset();
    REQUIRE(!other.feed(767,1,true));REQUIRE(!other.feed(65535,1,true));REQUIRE(!other.feed(30,7,true));
    other.feed(42,1,true);other.reset();REQUIRE(other.feed(30,1,true)->key=='a');
    REQUIRE(Decoder::is_cardputer("tca8418c"));REQUIRE(!Decoder::is_cardputer("USB Keyboard"));
    REQUIRE(platform::is_action_key(platform::kKeyHelp));
    REQUIRE(!platform::is_action_key(0x8d939ff0U)); // packed UTF-8 GPS pin from SDL
    REQUIRE(!platform::is_action_key(0x00ac82e2U)); // packed UTF-8 euro sign
}

#ifdef MESH_DEVICE_INPUT_TESTS
struct UiState {
    bool editing{false},help{false};int commits{0},screenshots{0},holds{0},releases{0};
    static bool query(void* data) {return static_cast<UiState*>(data)->editing;}
    static void release(uint32_t,const char*,void* data) { ++static_cast<UiState*>(data)->releases; }
    static bool route(uint32_t key,const char*,bool held,void* data) {
        auto& state=*static_cast<UiState*>(data);
        if (held) { ++state.holds; return true; }
        if (key==platform::kKeyHelp) {state.help=!state.help;return true;}
        if (key==platform::kKeyPrintScreen) {++state.screenshots;return true;}
        if (state.help) {if(key==LV_KEY_ESC)state.help=false;return true;}
        if (key==LV_KEY_ENTER) {if(state.editing)++state.commits;state.editing=true;return true;}
        return platform::is_action_key(key);
    }
};
static void device_path_checks() {
    lv_init();auto* display=lv_display_create(320,170);REQUIRE(display);
    int fds[2];REQUIRE(pipe(fds)==0);REQUIRE(fcntl(fds[0],F_SETFL,O_NONBLOCK)==0);
    auto* device=platform::make_test_evdev(fds[0],true);lv_indev_set_display(device,display);
    auto* area=lv_textarea_create(lv_screen_active());lv_textarea_set_one_line(area,true);
    auto* group=lv_group_get_default();lv_group_add_obj(group,area);lv_group_focus_obj(area);
    UiState state;platform::set_text_entry_query(UiState::query,&state);
    platform::set_global_key_listener(UiState::route,&state);
    auto raw=[&](unsigned type,unsigned code,int value) {input_event ev{};ev.type=type;ev.code=code;ev.value=value;REQUIRE(write(fds[1],&ev,sizeof(ev))==sizeof(ev));};
    auto key=[&](unsigned code) {raw(EV_KEY,code,1);raw(EV_KEY,code,0);};
    auto drain=[&] {lv_indev_read(device);};
    // Enter and a letter arriving in a SINGLE read batch must query current focus.
    key(KEY_ENTER);key(KEY_F);drain();REQUIRE(std::string(lv_textarea_get_text(area))=="f");
    lv_textarea_set_text(area,"");
    key(92);key(80);key(71);drain();REQUIRE(std::string(lv_textarea_get_text(area))=="/:-");
    // Every official symbol travels through the actual production evdev reader,
    // decoder, shortcut filter, LVGL indev and focused single-line editor.
    std::istringstream map(fixture());std::vector<std::pair<unsigned,char>> codes;
    for(std::string line;std::getline(map,line);){std::istringstream row(line);std::string word,equal,name;unsigned code;if(row>>word>>code>>equal>>name && word=="keycode")codes.push_back({code,expected_symbols().at(name)});}
    for(auto item:codes){lv_textarea_set_text(area,"");key(item.first);drain();REQUIRE(std::string(lv_textarea_get_text(area))==std::string(1,item.second));}
    lv_textarea_set_text(area,"ab");key(KEY_LEFT);key(92);drain();REQUIRE(std::string(lv_textarea_get_text(area))=="a/b");
    key(KEY_HOME);key(KEY_DELETE);key(KEY_END);key(KEY_BACKSPACE);drain();REQUIRE(std::string(lv_textarea_get_text(area))=="/");
    const auto before=std::string(lv_textarea_get_text(area));
    key(KEY_HELP);key(KEY_ENTER);key(92);drain();REQUIRE(state.help && state.commits==0 && before==lv_textarea_get_text(area));
    key(KEY_ESC);key(KEY_SYSRQ);key(KEY_PAGEUP);key(KEY_PAGEDOWN);key(KEY_INSERT);key(KEY_F1);drain();
    REQUIRE(!state.help && state.screenshots==1 && before==lv_textarea_get_text(area));REQUIRE(lv_group_get_focused(group)==area);
    lv_textarea_set_text(area,"");raw(EV_KEY,KEY_A,1);raw(EV_KEY,KEY_B,1);raw(EV_KEY,KEY_A,0);raw(EV_KEY,KEY_B,0);drain();
    REQUIRE(std::string(lv_textarea_get_text(area))=="ab");
    raw(EV_KEY,KEY_LEFTSHIFT,1);raw(EV_SYN,SYN_DROPPED,0);raw(EV_KEY,KEY_A,1);raw(EV_SYN,SYN_REPORT,0);key(KEY_B);drain();drain();
    REQUIRE(std::string(lv_textarea_get_text(area))=="abb");
    // Both ignored/modifier events and ordinary text are bounded across the
    // complete LVGL continue_reading chain, and unread events are retained.
    for(int i=0;i<140;++i) raw(EV_KEY,KEY_LEFTSHIFT,2);
    raw(EV_KEY,KEY_LEFTSHIFT,0);key(KEY_B);
    drain();REQUIRE(std::string(lv_textarea_get_text(area))=="abb");
    drain();REQUIRE(std::string(lv_textarea_get_text(area))=="abbb");
    lv_textarea_set_max_length(area,512);lv_textarea_set_text(area,"");
    for(int i=0;i<70;++i) key(KEY_A);
    drain();REQUIRE(std::string(lv_textarea_get_text(area))==std::string(64,'a'));
    drain();REQUIRE(std::string(lv_textarea_get_text(area))==std::string(70,'a'));

    // Closing the stream cancels a hold without a synthetic short-click/release.
    platform::set_key_release_listener(UiState::release,&state);
    raw(EV_KEY,KEY_ESC,1);drain();const int released_before=state.releases;
    close(fds[1]);drain();drain();
    lv_tick_inc(800);lv_timer_handler();
    REQUIRE(state.holds==0 && state.releases==released_before);
    lv_indev_delete(device);device=nullptr;

    auto pipe_device=[&](int (&pipefd)[2]) {
        REQUIRE(pipe(pipefd)==0);REQUIRE(fcntl(pipefd[0],F_SETFL,O_NONBLOCK)==0);
        auto* input=platform::make_test_evdev(pipefd[0],true);lv_indev_set_display(input,display);return input;
    };
    auto send=[&](int fd,unsigned code,int value) {input_event ev{};ev.type=EV_KEY;ev.code=code;ev.value=value;REQUIRE(write(fd,&ev,sizeof(ev))==sizeof(ev));};
    int owner_fds[2],other_fds[2];auto* owner=pipe_device(owner_fds);auto* other=pipe_device(other_fds);
    send(other_fds[1],KEY_ESC,1);lv_indev_read(other);
    send(owner_fds[1],KEY_ESC,1);lv_indev_read(owner);
    // Unrelated input activity/release and teardown must not cancel this owner.
    send(other_fds[1],KEY_ESC,0);lv_indev_read(other);
    lv_indev_delete(other);close(other_fds[1]);
    lv_tick_inc(800);lv_timer_handler();REQUIRE(state.holds==1);
    send(owner_fds[1],KEY_ESC,0);lv_indev_read(owner);
    send(owner_fds[1],KEY_ESC,1);lv_indev_read(owner);
    const int before_delete=state.releases;
    lv_indev_delete(owner);close(owner_fds[1]);
    lv_tick_inc(800);lv_timer_handler();
    REQUIRE(state.holds==1 && state.releases==before_delete);

    // The generic adapter cleanup API (also used by SDL) has the same scoped
    // behavior, without needing to delete the underlying LVGL device first.
    int adapter_fds[2];auto* adapter=pipe_device(adapter_fds);
    send(adapter_fds[1],KEY_ESC,1);lv_indev_read(adapter);
    platform::release_key_input(adapter);
    lv_tick_inc(800);lv_timer_handler();REQUIRE(state.holds==1);
    lv_indev_delete(adapter);close(adapter_fds[1]);
    platform::clear_key_release_listener(UiState::release,&state);
    platform::clear_global_key_listener(UiState::route,&state);platform::clear_text_entry_query(UiState::query,&state);
    if(device) lv_indev_delete(device);lv_obj_delete(area);lv_group_set_default(nullptr);lv_group_delete(group);lv_display_delete(display);
}
#endif

int main(int argc, char** argv) {
    if (argc==2) fixture_path=argv[1];
    else REQUIRE(argc==1);
    decoder_checks();
#ifdef MESH_DEVICE_INPUT_TESTS
    device_path_checks();
#endif
    std::cout << "Official 32-symbol map, modifiers, routing and editor event regressions passed\n";
}
