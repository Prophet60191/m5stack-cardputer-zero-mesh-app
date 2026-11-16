/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "app.h"
#include <cmath>
#include <chrono>

#include "application_config.h"
#include "asset_manager.h"
#include "help_popup.h"
#include "logger.h"
#include "linux_input.h"
#include "screenshot_service.h"
#include "screen_manager.h"
#include "base_viewmodel.h"
#include "theme.h"
#include "toast.h"
#include "supervisor_client.h"
#include "json.h"

#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#if USE_DESKTOP
#include "desktop_simulator_frame.h"
#endif

#if !USE_DESKTOP
#if APP_USE_DRM
#include "src/drivers/display/drm/lv_linux_drm.h"
#else
#include "src/drivers/display/fb/lv_linux_fbdev.h"
#endif
#endif

#ifndef APP_FRAMEBUFFER_DEVICE
#define APP_FRAMEBUFFER_DEVICE "/dev/fb0"
#endif

#ifndef APP_DRM_DEVICE
#define APP_DRM_DEVICE "/dev/dri/card0"
#endif

#ifndef APP_DRM_CONNECTOR_ID
#define APP_DRM_CONNECTOR_ID -1
#endif

#ifndef APP_CONFIG_FILE
#define APP_CONFIG_FILE "cardputerzero-mesh.conf"
#endif

namespace app {
namespace {

void quit_requested_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* running = static_cast<bool*>(lv_observer_get_user_data(observer));
    if (running && lv_subject_get_int(subject)) {
        *running = false;
    }
}

viewmodel::MessageInfo parse_message(const json::Value& m) {
    viewmodel::MessageInfo info;
    info.from = m.str_at("from");
    info.fromName = m.str_at("fromName");
    info.to = m.str_at("to", "broadcast");
    info.text = m.str_at("text");
    info.id = m.str_at("id");
    info.channel = static_cast<int>(m.int_at("channel"));
    info.time = m.int_at("time");
    if (const json::Value* o = m.find("outgoing")) {
        info.outgoing = o->as_bool();
    }
    if (const json::Value* s = m.find("snr")) {
        if (s->type == json::Value::Num) { info.snr = s->num; info.hasSnr = true; }
    }
    if (const json::Value* h = m.find("hops")) {
        if (h->type == json::Value::Num) info.hops = h->as_int32(-1);
    }
    info.relay = m.str_at("relay");
    info.channelIdentity = m.str_at("channel_id");
    info.channelName = m.str_at("channel_name");
    if (const auto* v = m.find("channel_private")) info.channelPrivate = v->as_bool();
    info.reason = m.str_at("reason");
    const std::string st = m.str_at("status");
    if (st == "sending") info.status = viewmodel::kMsgSending;
    else if (st == "delivered") info.status = viewmodel::kMsgDelivered;
    else if (st == "failed") info.status = viewmodel::kMsgFailed;
    else if (st == "mesh") info.status = viewmodel::kMsgMesh;
    else if (st == "sent") info.status = viewmodel::kMsgSent;
    return info;
}

// Translate one supervisor event into view-model updates. Runs on the UI thread
// (called from SupervisorClient::poll()).
void apply_supervisor_event(viewmodel::BaseViewModel& vm,
                            platform::SupervisorClient& client,
                            const json::Value& ev) {
    const std::string type = ev.str_at("event");
    if (type == "threads") {
        vm.apply_threads(ev);
    } else if (type == "setup") {
        vm.apply_setup(ev);
        const auto* ready = ev.find("ready");
        if (ready && !ready->as_bool()) vm.show_settings_page();
    } else if (type == "profiles" || type == "profile.preview" || type == "diagnostics" || type == "channel.slots" || type == "channel.preview" || type == "channel.imported") {
        vm.apply_service(ev);
    } else if (type == "_connected") {
        vm.set_connected(true);
        client.send_line("{\"intent\":\"status\"}");
        client.send_line("{\"intent\":\"node.list\"}");
        client.send_line("{\"intent\":\"channel.list\"}");
        // Honour "Auto-start radio" once per app launch.
        static bool auto_started = false;
        if (!auto_started && vm.auto_start()) {
            auto_started = true;
            vm.radio_start();
        }
    } else if (type == "_disconnected") {
        vm.set_connected(false);
    } else if (type == "status") {
        if (ev.str_at("radio") == "error" && !ev.str_at("detail").empty() && vm.radio_detail() != ev.str_at("detail"))
            vm.apply_notice(ev.str_at("detail"));
        vm.apply_health(ev.str_at("mode"), ev.str_at("detail"));
        vm.apply_status(ev.str_at("radio", "off"), ev.str_at("region", "UNSET"), ev.str_at("owner"));
    } else if (type == "nodes") {
        std::vector<viewmodel::NodeInfo> nodes;
        if (const json::Value* a = ev.find("nodes")) {
            for (const auto& n : a->arr) {
                viewmodel::NodeInfo info;
                info.id = n.str_at("id");
                info.name = n.str_at("name");
                info.shortName = n.str_at("short");
                if (const json::Value* s = n.find("snr")) {
                    if (s->type == json::Value::Num) { info.snr = s->num; info.hasSnr = true; }
                }
                info.lastHeard = n.int_at("lastHeard");
                info.positionTime = n.int_at("position_time");
                info.satellites = n.int_at("satellites", -1);
                if (const auto* v = n.find("channel_utilization"); v && v->type == json::Value::Num) info.channelUtilization = v->num;
                if (const auto* v = n.find("air_utilization"); v && v->type == json::Value::Num) info.airUtilization = v->num;
                if (const json::Value* h = n.find("hops")) {
                    if (h->type == json::Value::Num) info.hops = h->as_int32(-1);
                }
                if (const json::Value* s = n.find("self")) {
                    info.isSelf = s->as_bool();
                }
                if (const json::Value* b = n.find("battery")) {
                    if (b->type == json::Value::Num) { info.battery = b->as_int32(); info.hasBattery = true; }
                }
                if (const json::Value* v = n.find("voltage")) {
                    if (v->type == json::Value::Num) info.voltage = v->num;
                }
                const json::Value* la = n.find("lat");
                const json::Value* lo = n.find("lon");
                if (la && lo && la->type == json::Value::Num && lo->type == json::Value::Num) {
                    info.lat = la->num; info.lon = lo->num; info.hasPos = true;
                }
                nodes.push_back(std::move(info));
            }
        }
        vm.set_nodes(std::move(nodes));
    } else if (type == "channels") {
        std::vector<viewmodel::ChannelInfo> chans;
        if (const json::Value* a = ev.find("channels")) {
            for (const auto& c : a->arr) {
                viewmodel::ChannelInfo info;
                info.index = static_cast<int>(c.int_at("index"));
                info.name = c.str_at("name");
                info.role = c.str_at("role");
                info.identity = c.str_at("identity");
                if (const auto* v = c.find("private")) info.isPrivate = v->as_bool();
                info.positionPrecision = static_cast<int>(c.int_at("position_precision"));
                chans.push_back(std::move(info));
            }
        }
        vm.set_channels(std::move(chans));
    } else if (type == "messages") {
        std::vector<viewmodel::MessageInfo> msgs;
        if (const json::Value* a = ev.find("messages")) {
            for (const auto& m : a->arr) {
                msgs.push_back(parse_message(m));
            }
        }
        vm.set_messages(std::move(msgs));
    } else if (type == "message") {
        if (const json::Value* m = ev.find("message")) {
            vm.add_message(parse_message(*m));
        }
    } else if (type == "delivery") {
        const std::string cid = ev.str_at("cid");
        const std::string st = ev.str_at("status");
        int hops = -1;
        if (const json::Value* h = ev.find("hops")) {
            if (h->type == json::Value::Num) hops = h->as_int32(-1);
        }
        vm.set_delivery(cid, st == "failed" ? viewmodel::kMsgFailed : (st == "delivered" ? viewmodel::kMsgDelivered : (st == "mesh" ? viewmodel::kMsgMesh : (st == "sent" ? viewmodel::kMsgSent : 0))), hops, ev.str_at("reason"));
    } else if (type == "traceroute") {
        bool ok = false;
        if (const json::Value* o = ev.find("ok")) ok = o->as_bool();
        std::string text;
        if (ok) {
            if (const json::Value* p = ev.find("path")) {
                for (const auto& n : p->arr) {
                    if (!text.empty()) text += "\n  \xE2\x86\x93\n";   // down-arrow between hops
                    text += n.str;
                }
            }
        } else {
            text = ev.str_at("text");
        }
        vm.apply_traceroute(ok, text, ev.str_at("node"), ev.str_at("id"));
    } else if (type == "channelshare") {
        vm.apply_channel_share(static_cast<int>(ev.int_at("channel", -1)), ev.str_at("name"), ev.str_at("url"), ev.str_at("id"), ev.str_at("channel_id"));
    } else if (type == "notice") {
        vm.apply_notice(ev.str_at("text"));
    } else if (type == "error") {
        vm.apply_service(ev);
        if (!ev.str_at("cid").empty()) vm.set_delivery(ev.str_at("cid"), viewmodel::kMsgFailed, -1, ev.str_at("text"));
        if (ev.str_at("intent").rfind("config.", 0) == 0) vm.config_unknown();
        vm.apply_notice(ev.str_at("text"));
    } else if (type == "device") {
        vm.apply_device(ev.str_at("node_id"), ev.str_at("firmware"), ev.str_at("model"));
    } else if (type == "config.unknown") {
        vm.config_unknown();
    } else if (type == "config") {
        const auto* confirmed = ev.find("confirmed");
        const auto* position = ev.find("position");
        const auto* mqtt = ev.find("mqtt");
        const auto* preset = ev.find("preset");
        const auto* hop = ev.find("hop");
        const auto* slot = ev.find("frequency_slot");
        if (!confirmed || confirmed->type != json::Value::Bool || !confirmed->b ||
            !position || position->type != json::Value::Bool ||
            !mqtt || mqtt->type != json::Value::Bool ||
            !preset || preset->type != json::Value::Str || preset->str.empty() ||
            !hop || hop->type != json::Value::Num || hop->num < 0 || hop->num > 7 ||
            std::floor(hop->num) != hop->num ||
            (slot && (slot->type != json::Value::Num || slot->num < 0 || slot->num > 255 || std::floor(slot->num) != slot->num))) {
            vm.config_unknown();
            return;
        }
        vm.apply_config(preset->str, hop->as_int32(), position->b, mqtt->b, ev.str_at("id"), static_cast<int>(ev.int_at("frequency_slot")), ev.str_at("gps_mode", "UNKNOWN"));
    }
    // "notice" / "error": surfaced via toast during polish (step 5).
}

std::string writable_config_path() {
#if USE_DESKTOP
    return APP_CONFIG_FILE;
#else
    if (const char* xdg_config_home = std::getenv("XDG_CONFIG_HOME")) {
        const std::filesystem::path root(xdg_config_home);
        if (!root.empty() && root.is_absolute()) {
            return (root / "cardputerzero-mesh" / "cardputerzero-mesh.conf").string();
        }
    }
    if (const char* home = std::getenv("HOME")) {
        const std::filesystem::path root(home);
        if (!root.empty() && root.is_absolute()) {
            return (root / ".config" / "cardputerzero-mesh" / "cardputerzero-mesh.conf").string();
        }
    }
    return APP_CONFIG_FILE;
#endif
}

class ShortcutController {
public:
    ShortcutController(viewmodel::BaseViewModel& view_model, AssetManager& assets)
        : view_model_(view_model),
          toast_(std::make_unique<view::widgets::Toast>(lv_layer_top(), view_model, assets)),
          help_popup_(std::make_unique<view::widgets::HelpPopup>(lv_layer_top(), view_model, assets)) {
        toast_->build();
        help_popup_->build();
        platform::set_global_key_listener(global_key_listener, this);
        platform::set_key_release_listener(key_release_listener, this);
        incoming_observer_ = lv_subject_add_observer(view_model_.incoming_message_subject(),
                                                     incoming_cb, this);
        notice_observer_ = lv_subject_add_observer(view_model_.notice_subject(), notice_cb, this);
    }

    ~ShortcutController() {
        if (incoming_observer_) {
            lv_observer_remove(incoming_observer_);
        }
        if (notice_observer_) {
            lv_observer_remove(notice_observer_);
        }
        platform::clear_key_release_listener(key_release_listener, this);
        platform::clear_global_key_listener(global_key_listener, this);
    }

    void set_screen_manager(ScreenManager* screen_manager) {
        screen_manager_ = screen_manager;
    }

private:
    bool handle_key(uint32_t key, bool long_pressed) {
        // While a text field is capturing input, only intercept Enter (commit)
        // and Esc (cancel); let every other key reach the text box.
        if (screen_manager_ && screen_manager_->is_composing()) {
            if (long_pressed) return true;
            if (key == LV_KEY_ENTER) { screen_manager_->key_enter(); return true; }
            if (key == LV_KEY_ESC)   { screen_manager_->key_back();  return true; }
            return false;
        }
        if (key == platform::kKeyPrintScreen) {
            if (!long_pressed) {
                const auto result = platform::screenshot::capture_active_screen();
                if (result.success) {
                    LOG_INFO("screenshot saved: {}", result.path);
                    toast_->show_highlighted("Saved to ",
                                             result.path,
                                             "",
                                             view::widgets::ToastTone::Success,
                                             5200,
                                             true);
                }
                else {
                    LOG_WARN("screenshot failed: {}", result.error);
                    toast_->show("Screenshot failed: " + result.error,
                                 view::widgets::ToastTone::Error,
                                 3200);
                }
            }
            return true;
        }

        if (key == platform::kKeyHelp) {
            if (!long_pressed) {
                toast_->hide();
                if (help_popup_->visible()) {
                    help_popup_->hide();
                }
                else {
                    help_popup_->show(view_model_.current_page());
                }
            }
            return true;
        }

        if (help_popup_->visible()) {
            if (key == LV_KEY_ESC && !long_pressed) {
                help_popup_->hide();
            }
            return true;
        }

        const bool on_menu = view_model_.current_page() == model::AppPage::Menu;

        // Home carousel: Left/Right roll the selection, Enter opens it.
        if (on_menu && !long_pressed) {
            if (key == LV_KEY_LEFT)  { view_model_.menu_prev(); return true; }
            if (key == LV_KEY_RIGHT) { view_model_.menu_next(); return true; }
            if (key == LV_KEY_ENTER) { view_model_.open_menu_selection(); return true; }
        }

        // On a feature screen, Up/Down/Enter drive the screen's own selection.
        if (!on_menu && !long_pressed && screen_manager_) {
            if (key == LV_KEY_UP)    { screen_manager_->key_up();    return true; }
            if (key == LV_KEY_DOWN)  { screen_manager_->key_down();  return true; }
            if (key == LV_KEY_ENTER) { screen_manager_->key_enter(); return true; }
        }

        // Other keys (e.g. 'f' to favourite, Tab to filter) go to the screen.
        if (!on_menu && !long_pressed && key != LV_KEY_ESC && screen_manager_ &&
            screen_manager_->key_char(key)) {
            return true;
        }

        if (key != LV_KEY_ESC) {
            return false;
        }

        // Esc on a feature screen goes back to the menu (on release). Esc on the
        // menu is hold-to-quit.
        if (!on_menu) {
            exit_key_pressed_ = key;
            exit_armed_ = false;
            return true;
        }
        if (long_pressed) {
            if (exit_key_pressed_ == key && exit_armed_) {
                toast_->hide();
                view_model_.request_quit();
            }
        }
        else {
            exit_key_pressed_ = key;
            exit_armed_ = true;
            toast_->show_persistent_highlighted("Hold ", "ESC", " to exit",
                                                view::widgets::ToastTone::Warning);
        }
        return true;
    }

    void handle_release(uint32_t key) {
        if (key != exit_key_pressed_) {
            return;
        }
        const bool on_menu = view_model_.current_page() == model::AppPage::Menu;
        if (on_menu) {
            if (exit_armed_) {
                toast_->hide();   // released before the hold completed
            }
        } else if (key == LV_KEY_ESC) {
            // Let the screen consume Esc (e.g. close a detail overlay) first;
            // otherwise leave to the menu.
            if (!(screen_manager_ && screen_manager_->key_back())) {
                view_model_.show_menu_page();
            }
        }
        exit_key_pressed_ = 0;
        exit_armed_ = false;
    }

    static bool global_key_listener(uint32_t key,
                                    const char*,
                                    bool long_pressed,
                                    void* user_data) {
        auto* controller = static_cast<ShortcutController*>(user_data);
        return controller && controller->handle_key(key, long_pressed);
    }

    static void key_release_listener(uint32_t key, const char*, void* user_data) {
        auto* controller = static_cast<ShortcutController*>(user_data);
        if (controller) {
            controller->handle_release(key);
        }
    }

    // Ring a toast for an incoming message when we're not already on Messages.
    // (Muted threads never bump this subject, so they stay quiet.)
    static void incoming_cb(lv_observer_t* observer, lv_subject_t*) {
        auto* controller = static_cast<ShortcutController*>(lv_observer_get_user_data(observer));
        if (!controller) return;
        std::string from = controller->view_model_.last_incoming_from();
        std::string text = controller->view_model_.last_incoming_text();
        if (text.empty()) return;   // ignore the subscribe-time call (no message yet)
        if (controller->view_model_.current_page() == model::AppPage::Messages) return;
        if (text.size() > 40) text = text.substr(0, 39) + "\xE2\x80\xA6";
        controller->toast_->show_highlighted(from + ": ", text, "",
                                             view::widgets::ToastTone::Default, 3200, true);
    }

    // Show a supervisor notice/error (e.g. "no position fix yet") as a toast.
    static void notice_cb(lv_observer_t* observer, lv_subject_t*) {
        auto* controller = static_cast<ShortcutController*>(lv_observer_get_user_data(observer));
        if (!controller) return;
        const std::string text = controller->view_model_.last_notice();
        if (text.empty()) return;   // ignore the subscribe-time call
        controller->toast_->show(text, view::widgets::ToastTone::Warning, 3200);
    }

    viewmodel::BaseViewModel& view_model_;
    ScreenManager* screen_manager_{nullptr};
    std::unique_ptr<view::widgets::Toast> toast_;
    std::unique_ptr<view::widgets::HelpPopup> help_popup_;
    lv_observer_t* incoming_observer_{nullptr};
    lv_observer_t* notice_observer_{nullptr};
    uint32_t exit_key_pressed_{0};
    bool exit_armed_{false};
};

#if !USE_DESKTOP
lv_display_t* init_device_display() {
#if APP_USE_DRM
    auto* display = lv_linux_drm_create();
    if (!display) {
        return nullptr;
    }

    if (lv_linux_drm_set_file(display, APP_DRM_DEVICE, APP_DRM_CONNECTOR_ID) != LV_RESULT_OK) {
        lv_display_delete(display);
        return nullptr;
    }

    platform::init_key_input(display);
    return display;
#else
    auto* display = lv_linux_fbdev_create();
    if (!display) {
        return nullptr;
    }

    if (lv_linux_fbdev_set_file(display, APP_FRAMEBUFFER_DEVICE) != LV_RESULT_OK) {
        lv_display_delete(display);
        return nullptr;
    }

    platform::init_key_input(display);
    return display;
#endif
}
#endif

} // namespace

int Application::run() {
    logger::Logger::init();
    logger::Logger::set_tag("mesh");

    lv_init();

    AssetManager assets;
    for (const auto& root : assets.roots()) {
        LOG_INFO("asset root: {}", root.string());
    }

    viewmodel::BaseViewModel view_model;
    const std::string user_config_path = writable_config_path();
    std::string loaded_config_path = APP_CONFIG_FILE;
    bool legacy_preferences = false;
    if (user_config_path != APP_CONFIG_FILE) {
        std::error_code filesystem_error;
        if (std::filesystem::is_regular_file(user_config_path, filesystem_error)) {
            loaded_config_path = user_config_path;
        } else {
            const auto legacy = std::filesystem::path(user_config_path).parent_path().parent_path() /
                                "template-app" / "template-app.conf";
            if (std::filesystem::is_regular_file(legacy, filesystem_error)) {
                loaded_config_path = legacy.string();
                legacy_preferences = true;
            }
        }
    }

    ApplicationConfig config;
    std::string config_error;
    if (load_application_config(loaded_config_path, config, config_error)) {
        view_model.set_dark_mode(config.dark_mode);
        if (config.has_messaging) {
            view_model.load_messaging_prefs(config.show_signal, config.retention_cap,
                                            config.mute_all, config.want_ack);
            view_model.load_auto_start(config.auto_start && !legacy_preferences);
            view_model.load_quiet_hours(config.quiet_start, config.quiet_end);
            view_model.load_canned(config.canned);
            view_model.load_muted(config.muted);
            view_model.load_favorites(config.favorites);
        }
        LOG_INFO("loaded config: {} (dark_mode={})",
                 loaded_config_path,
                 config.dark_mode ? "yes" : "no");
    } else {
        LOG_WARN("failed to load config {}: {}; using defaults", loaded_config_path, config_error);
    }

    // Persist every app-local change by serialising the view-model's full state.
    view_model.set_persist_hook([&view_model, user_config_path]() {
        ApplicationConfig cfg;
        cfg.dark_mode = view_model.is_dark_mode();
        cfg.show_signal = view_model.show_signal();
        cfg.retention_cap = view_model.retention_cap();
        cfg.mute_all = view_model.mute_all();
        cfg.want_ack = view_model.want_ack();
        cfg.auto_start = view_model.auto_start();
        cfg.quiet_start = view_model.quiet_start();
        cfg.quiet_end = view_model.quiet_end();
        cfg.canned = view_model.canned_messages();
        for (const auto& k : view_model.muted_threads()) cfg.muted.push_back(k);
        cfg.favorites = view_model.favorites();
        std::string err;
        if (!save_application_config(user_config_path, cfg, err)) {
            LOG_WARN("failed to save config {}: {}", user_config_path, err);
        }
    });

#if USE_DESKTOP
    DesktopSimulatorFrame simulator_frame(assets);
    auto* display = simulator_frame.display();
#else
    auto* display = init_device_display();
#endif
    if (!display) {
        LOG_ERROR("failed to initialize display");
        return 1;
    }

    auto* standard_font = assets.load_standard_font(14);
    view::apply_lvgl_theme(display,
                           view_model.is_dark_mode(),
                           standard_font ? standard_font : LV_FONT_DEFAULT);

#if USE_DESKTOP
    simulator_frame.bind_dark_mode(view_model.dark_mode_subject());
#endif

    ScreenManager screen_manager(view_model, assets);
    screen_manager.start();
    ShortcutController shortcuts(view_model, assets);
    shortcuts.set_screen_manager(&screen_manager);

#if USE_DESKTOP
    // Synthetic sample data so the screens can be designed in the simulator (no
    // supervisor there). These identities, call-signs and coordinates are made up
    // and must never reference a real node, person, or location.
    view_model.apply_health("transmit", "");
    view_model.apply_status("online", "US", "Nomad");
    view_model.apply_device("!10000001", "2.7.26", "CardputerZero");
    view_model.set_channels({{0, "Public", false, "PRIMARY", 0, "synthetic-public"},
                             {1, "Field", true, "SECONDARY", 0, "synthetic-private"}});
    view_model.apply_config("MEDIUM_FAST", 3, true, false);
    const long node_now = static_cast<long>(std::time(nullptr));
    // id, name, short, snr, hasSnr, lastHeard, hops, isSelf, favorite, hasBattery, battery, voltage, hasPos, lat, lon
    view_model.set_nodes({
        {"!10000001", "Nomad", "NMD", 0.0, false, node_now, -1, true, false, true, 98, 4.05, true, 40.0000, -105.0000},
        {"!10000002", "Basecamp", "BASE", 9.5, true, node_now - 45, 0, false, false, true, 85, 3.95, true, 40.0150, -104.9900},
        {"!10000003", "Ridge Relay", "RDG", 10.25, true, node_now - 320, 0, false, false, true, 100, 4.10, true, 39.9870, -105.0130},
        {"!10000004", "Scout", "SCT", -0.25, true, node_now - 5400, 1, false, false, false, 0, 0.0, false, 0.0, 0.0},
    });
    // from, fromName, to, text, id, channel, time, outgoing, status
    std::vector<viewmodel::MessageInfo> samples = {
        // group channel
        {"!10000002", "Basecamp", "broadcast", "On my way to the ridge.", "", 1, 0, false, 0},
        {"self", "me", "broadcast", "Copy. I have eyes on the trailhead.", "s1", 1, 0, true, viewmodel::kMsgDelivered},
        {"!10000003", "Ridge Relay", "broadcast", "Weather closing in from the west, watch the pass.", "", 1, 0, false, 0},
        {"self", "me", "broadcast", "Understood, heading back now.", "s2", 1, 0, true, viewmodel::kMsgDelivered},
        {"!10000004", "Scout", "broadcast", "Relaying for you both - strong copy.", "", 1, 0, false, 0},
        {"self", "me", "broadcast", "Four bars holding steady.", "s3", 1, 0, true, viewmodel::kMsgSending},
        // direct messages with Basecamp
        {"!10000002", "Basecamp", "!10000001", "You around? Quick question.", "", 0, 0, false, 0},
        {"self", "me", "!10000002", "Yeah, go ahead.", "d1", 0, 0, true, viewmodel::kMsgDelivered},
        {"!10000002", "Basecamp", "!10000001", "Which repeater are you hitting?", "", 0, 0, false, 0},
        {"self", "me", "!10000002", "Ridge Relay, direct.", "d2", 0, 0, true, viewmodel::kMsgFailed},
    };
    long sample_t = static_cast<long>(std::time(nullptr)) - static_cast<long>(samples.size()) * 180;
    for (auto& m : samples) {
        if (const auto* channel = view_model.channel(m.channel)) {
            m.channelIdentity = channel->identity;
            m.channelName = channel->name;
            m.channelPrivate = channel->isPrivate;
        }
        if (m.to == "broadcast" && m.status == viewmodel::kMsgDelivered) m.status = viewmodel::kMsgMesh;
        m.time = sample_t;
        sample_t += 180;
    }
    view_model.set_messages(samples);
#endif

    // The supervisor owns the radio; the UI talks to it over a local socket.
    platform::SupervisorClient supervisor;
    bool state_synced = false;
    view_model.set_command_sink([&supervisor](const std::string& line) { return supervisor.send_line(line); });
    supervisor.start([&view_model, &supervisor, &state_synced](const json::Value& ev) {
        if (ev.str_at("event") == "state.synced" && ev.str_at("id") == "quit-sync") state_synced = true;
        apply_supervisor_event(view_model, supervisor, ev);
    });

    bool running = true;
    auto* quit_observer = lv_subject_add_observer(view_model.quit_requested_subject(),
                                                  quit_requested_observer,
                                                  &running);

    LOG_INFO("LVGL app started at {}x{}", lv_display_get_horizontal_resolution(display),
             lv_display_get_vertical_resolution(display));
    while (running
#if USE_DESKTOP
           && simulator_frame.process_events()
#endif
    ) {
        // Only require Fn for the d-pad while a text field is capturing input;
        // everywhere else the bare F/X/Z/C keys navigate on their own.
        platform::set_text_entry_mode(screen_manager.is_composing());
        supervisor.poll();
        view_model.tick();          // drain supervisor events on the UI thread
        lv_timer_handler();
        lv_delay_ms(5);
    }

    if (screen_manager.is_composing()) screen_manager.key_back();
    view_model.flush_drafts();
    if (supervisor.connected() && supervisor.send_line("{\"intent\":\"state.sync\",\"id\":\"quit-sync\"}")) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!state_synced && supervisor.connected() && std::chrono::steady_clock::now() < deadline) {
            supervisor.poll();
            lv_delay_ms(5);
        }
        if (!state_synced) LOG_WARN("Draft synchronization did not finish before exit");
    }
    // Screens can flush draft state from their destructors. Disconnect the
    // callback before the transport is destroyed (reverse local-object order).
    view_model.set_command_sink({});
    supervisor.stop();

    if (quit_observer) {
        lv_observer_remove(quit_observer);
    }

    return 0;
}

} // namespace app
