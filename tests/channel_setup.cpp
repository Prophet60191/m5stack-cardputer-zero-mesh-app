// SPDX-License-Identifier: MIT
// Real LVGL, synthetic socket replies; no hardware, network, framebuffer or RF.
#include "settings_screen.h"
#include "messages_screen.h"
#include "screen_manager.h"
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <png.h>

#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            std::cerr << "FAIL " << __LINE__ << ": " << #x << '\n';                                          \
            std::abort();                                                                                    \
        }                                                                                                    \
    } while (0)
static json::Value parse(const std::string &s) {
    json::Value v;
    CHECK(json::parse(s, v));
    return v;
}
static std::string labels(lv_obj_t *o) {
    std::string out = lv_obj_check_type(o, &lv_label_class) ? std::string(lv_label_get_text(o)) + "\n" : "";
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i)
        out += labels(lv_obj_get_child(o, i));
    return out;
}
static lv_obj_t *input(lv_obj_t *o) {
    if (lv_obj_check_type(o, &lv_textarea_class))
        return o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i)
        if (auto *p = input(lv_obj_get_child(o, i)))
            return p;
    return nullptr;
}
static std::string selected(lv_obj_t *o) {
    if (lv_obj_check_type(o, &lv_label_class) && lv_obj_get_style_bg_opa(o, LV_PART_MAIN) == LV_OPA_COVER &&
        lv_obj_get_style_radius(o, LV_PART_MAIN) == 6)
        return lv_label_get_text(o);
    if (lv_obj_get_style_border_width(o, LV_PART_MAIN) == 3 && lv_obj_get_child_count(o))
        return labels(o).substr(0, labels(o).find('\n'));
    for (uint32_t i = lv_obj_get_child_count(o); i > 0; --i) {
        auto s = selected(lv_obj_get_child(o, i - 1));
        if (!s.empty())
            return s;
    }
    return "";
}
static void choose(screen::SettingsScreen &s, const std::string &prefix) {
    for (int i = 0; i < 36; ++i)
        s.on_up();
    for (int i = 0; i < 45; ++i) {
        if (selected(s.root()).rfind(prefix, 0) == 0) {
            s.on_enter();
            return;
        }
        s.on_down();
    }
    std::cerr << "Missing selection " << prefix << " among " << labels(s.root()) << '\n';
    CHECK(false);
}
static void screenshot(lv_obj_t *root, const std::string &dir, const std::string &name) {
    lv_obj_update_layout(root);
    const auto check_head = [&](auto &&self, lv_obj_t *o) -> void {
        if (lv_obj_check_type(o, &lv_label_class)) {
            const std::string t = lv_label_get_text(o);
            if (t == "Confirm these changes" || t == "QR scanned (unsaved)" || t == "Link loaded (unsaved)" ||
                t == "Channel setup complete" || t == "Add channel (unsaved)") {
                lv_area_t r;
                lv_obj_get_coords(o, &r);
                std::cerr << "chrome " << name << " heading_y=" << r.y1 << ".." << r.y2
                          << " parent_y=" << lv_obj_get_y(lv_obj_get_parent(o))
                          << " scroll=" << lv_obj_get_scroll_y(lv_obj_get_parent(o)) << '\n';
                CHECK(r.y1 >= 30 && r.y2 < 155);
            }
        }
        for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i)
            self(self, lv_obj_get_child(o, i));
    };
    check_head(check_head, root);
    if (dir.empty())
        return;
    auto *shot = lv_snapshot_take(root, LV_COLOR_FORMAT_ARGB8888);
    CHECK(shot);
    png_image im{};
    im.version = PNG_IMAGE_VERSION;
    im.width = shot->header.w;
    im.height = shot->header.h;
    im.format = PNG_FORMAT_BGRA;
    CHECK(png_image_write_to_file(&im, (std::filesystem::path(dir) / name).c_str(), false, shot->data,
                                  shot->header.stride, nullptr));
    lv_draw_buf_destroy(shot);
}
static const std::string context =
    R"("event":"channel.setup.context","base":"synthetic-base","radio":"online","tx":false,"tuning":{"region":"US","preset":"LONG_FAST","use_preset":true,"slot":20,"hop":3})";
static void result(viewmodel::BaseViewModel &vm, const json::Value &sent, const std::string &data) {
    vm.apply_service(parse("{\"id\":\"" + sent.str_at("id") + "\"," + data + "}"));
}
static void run(const std::string &dir, bool dark) {
    viewmodel::BaseViewModel vm;
    app::AssetManager assets;
    vm.set_dark_mode(dark);
    vm.set_connected(true);
    vm.apply_status("online", "US", "Synthetic");
    vm.apply_health("receive-only", "");
    vm.apply_setup(parse(R"({"event":"setup","ready":true})"));
    std::vector<json::Value> sent;
    bool accept = true;
    vm.set_command_sink([&](const std::string &s) {
        sent.push_back(parse(s));
        return accept;
    });
    screen::SettingsScreen s(vm, assets);
    lv_screen_load(s.root());
    choose(s, "Add a channel");
    CHECK(sent.back().str_at("intent") == "channel.setup.context");
    auto ctx = sent.back();
    result(vm, ctx, context);
    choose(s, "Create private");
    auto n = sent.size();
    choose(s, "Channel name:");
    CHECK(s.is_composing());
    CHECK(!s.on_key('1'));
    lv_textarea_set_text(input(s.root()), "Field");
    s.on_enter();
    CHECK(!s.is_composing());
    CHECK(sent.size() == n);
    choose(s, "Frequency slot:");
    lv_textarea_set_text(input(s.root()), "999");
    s.on_enter();
    CHECK(s.is_composing());
    lv_textarea_set_text(input(s.root()), "25");
    s.on_enter();
    choose(s, "Profile name:");
    lv_textarea_set_text(input(s.root()), "Field profile");
    s.on_enter();
    CHECK(labels(s.root()).find("Channel name: Field") != std::string::npos);
    CHECK(labels(s.root()).find("Frequency slot: 25") != std::string::npos);
    // Esc in the form defaults to preserving the complete local draft.
    s.on_back();
    CHECK(selected(s.root()) == "Keep editing");
    s.on_enter();
    CHECK(labels(s.root()).find("Field profile") != std::string::npos);
    CHECK(sent.size() == n);
    CHECK(labels(s.root()).find("Radio connection - all channels") != std::string::npos);
    CHECK(labels(s.root()).find("Sharing - new channel") != std::string::npos);
    CHECK(labels(s.root()).find("Save destination") != std::string::npos);
    for (int i = 0; i < 40; ++i) {
        const auto text = selected(s.root());
        CHECK(text != "Channel" && text != "Radio connection - all channels" &&
              text != "Sharing - new channel" && text != "Save destination");
        s.on_up();
    }
    for (int i = 0; i < 40 && selected(s.root()).rfind("Channel name:", 0) != 0; ++i) s.on_down();
    screenshot(s.root(), dir, dark ? "channel-setup-form-dark.png" : "channel-setup-form.png");
    // Footer stays in bounds even with long error content and deep selection.
    for (int i = 0; i < 25; ++i)
        s.on_down();
    lv_obj_update_layout(s.root());
    const auto find_hint = [&](auto &&self, lv_obj_t *o) -> lv_obj_t * {
        if (lv_obj_check_type(o, &lv_label_class) &&
            std::string(lv_label_get_text(o)).rfind("1: Save", 0) == 0)
            return o;
        for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i)
            if (auto *x = self(self, lv_obj_get_child(o, i)))
                return x;
        return nullptr;
    };
    auto *hint = find_hint(find_hint, s.root());
    CHECK(hint);
    lv_area_t a;
    lv_obj_get_coords(hint, &a);
    CHECK(a.x1 >= 0 && a.x2 < 320 && a.y1 >= 30 && a.y2 < 170);
    CHECK(s.on_key('1'));
    CHECK(sent.back().str_at("intent") == "channel.setup.preview");
    auto preview = sent.back();
    CHECK(preview.str_at("name") == "Field");
    CHECK(preview.find("tuning")->int_at("slot") == 25);
    CHECK(preview.str_at("profile_name") == "Field profile");
    CHECK(preview.str_at("base") == "synthetic-base");
    const auto during_prepare = sent.size();
    CHECK(s.on_key('1'));
    CHECK(sent.size() == during_prepare);
    s.on_enter();
    CHECK(sent.back().str_at("id") == preview.str_at("id"));
    result(vm, parse(R"({"id":"wrong"})"), R"("event":"channel.setup.preview","token":"wrong")");
    CHECK(labels(s.root()).find("Checking your entries") != std::string::npos);
    result(
        vm, preview,
        R"("event":"channel.setup.preview","token":"plan","channels":[{"name":"Field","private":true}],"tuning_changed":true,"warnings":["Radio tuning changes affect every channel on this radio."])");
    CHECK(selected(s.root()) == "Back to form (keep entries)");
    screenshot(s.root(), dir,
               dark ? "channel-setup-radio-confirm-dark.png" : "channel-setup-radio-confirm.png");
    choose(s, "Back to form");
    CHECK(labels(s.root()).find("Channel name: Field") != std::string::npos);
    choose(s, "Add by:");
    choose(s, "Create public");
    choose(s, "Transmit mode:");
    choose(s, "TX/RX");
    choose(s, "Automatic location:");
    choose(s, "Internet forwarding:");
    CHECK(s.on_key('1'));
    result(
        vm, sent.back(),
        R"("event":"channel.setup.preview","token":"plan2","channels":[{"name":"Field","private":false}],"tx_confirmation":true,"public_position":true,"mqtt":true,"tuning_changed":true)");
    auto before = sent.size();
    CHECK(s.on_key('1'));
    CHECK(sent.size() == before); // unchecked explicit consents
    choose(s, "[ ] Apply radio");
    choose(s, "[ ] Antenna");
    choose(s, "[ ] Allow public");
    choose(s, "[ ] Allow configured MQTT");
    screenshot(s.root(), dir, dark ? "channel-setup-confirm-dark.png" : "channel-setup-confirm.png");
    CHECK(s.on_key('1'));
    CHECK(sent.back().str_at("intent") == "channel.setup.commit");
    CHECK(sent.back().str_at("token") == "plan2");
    for (const auto *key :
         {"tuning_confirmed", "antenna_confirmed", "public_position_confirmed", "mqtt_confirmed"})
        CHECK(sent.back().find(key)->as_bool());
    auto commit = sent.back();
    n = sent.size();
    s.on_enter();
    CHECK(s.on_back());
    CHECK(sent.size() == n); // never resubmit or cancel midway through a save
    result(
        vm, commit,
        R"("event":"channel.setup.saved","added":1,"verified":"radio","channels":[{"index":1,"identity":"new-group"}],"profile":"synthetic-profile")");
    CHECK(labels(s.root()).find("verified by the radio") != std::string::npos);
    vm.apply_health("transmit", "");
    screenshot(s.root(), dir, dark ? "channel-setup-complete-dark.png" : "channel-setup-complete.png");
    vm.set_channels({{1, "Field", false, "SECONDARY", 15, "new-group"}});
    choose(s, "Open conversation");
    choose(s, "Open channel 1");
    CHECK(vm.current_page() == model::AppPage::Messages);
    {
        screen::MessagesScreen messages(vm, assets);
        CHECK(labels(messages.root()).find("1: Field [Public]") != std::string::npos);
    }
    CHECK(vm.take_group_target().empty());
    // New method/destination remain local, profile-only forces safe defaults.
    choose(s, "Add a channel");
    result(vm, sent.back(), context);
    choose(s, "Create public");
    choose(s, "Transmit mode:");
    choose(s, "TX/RX");
    choose(s, "Save to:");
    choose(s, "Separate profile");
    CHECK(labels(s.root()).find("Transmit mode: Receive only") != std::string::npos);
    choose(s, "Profile name:");
    lv_textarea_set_text(input(s.root()), "Public profile");
    s.on_enter();
    CHECK(s.on_key('1'));
    CHECK(sent.back().find("profile_only")->as_bool());
    CHECK(!sent.back().find("tx")->as_bool());
    auto request = sent.back();
    result(vm, request, R"("event":"error","text":"Synthetic validation error")");
    CHECK(labels(s.root()).find("Public profile") != std::string::npos);
    CHECK(labels(s.root()).find("Synthetic validation error") != std::string::npos);
    // Cancellation cannot reopen the form on a late reply.
    CHECK(s.on_key('1'));
    request = sent.back();
    s.on_back();
    s.on_back();
    choose(s, "Discard and close");
    result(vm, request, R"("event":"channel.setup.preview","token":"late")");
    CHECK(labels(s.root()).find("late") == std::string::npos);
    choose(s, "Add a channel");
    result(vm, sent.back(), context);
    choose(s, "Paste share URL");
    CHECK(lv_textarea_get_password_mode(input(s.root())));
    lv_textarea_set_text(input(s.root()), "https://meshtastic.org/e/#synthetic");
    s.on_enter();
    CHECK(sent.back().str_at("intent") == "channel.preview" && sent.back().find("setup")->as_bool());
    result(
        vm, sent.back(),
        R"("event":"channel.preview","new":1,"token":"import","channels":[{"name":"Imported","private":false}],"suggested_tuning":{"region":"US","preset":"MEDIUM_FAST","slot":35,"hop":3,"use_preset":true})");
    CHECK(labels(s.root()).find("Imported [Public]") != std::string::npos);
    CHECK(labels(s.root()).find("Review setup") == std::string::npos);
    CHECK(labels(s.root()).find("F/X:") == std::string::npos);
    CHECK(labels(s.root()).find("Frequency slot: 20") != std::string::npos);
    choose(s, "Use radio settings from QR");
    CHECK(labels(s.root()).find("Frequency slot: 35") != std::string::npos);
    CHECK(s.on_key('1'));
    CHECK(sent.back().str_at("import_token") == "import");
    CHECK(sent.back().find("tuning")->str_at("preset") == "MEDIUM_FAST");
    auto imported_review = sent.back();
    result(vm, imported_review, R"("event":"error","text":"Synthetic tuning validation error")");
    CHECK(s.on_key('1'));
    CHECK(sent.back().str_at("import_token") == "import");
    imported_review = sent.back();
    result(
        vm, imported_review,
        R"("event":"channel.setup.preview","token":"review-import","channels":[{"name":"Imported","private":false}],"tuning_changed":true)");
    const auto before_back = sent.size();
    s.on_back();
    for (size_t i = before_back; i < sent.size(); ++i)
        CHECK(sent[i].str_at("intent") != "channel.import.cancel");
    CHECK(s.on_key('1'));
    CHECK(sent.back().str_at("import_token") == "import");

    vm.set_connected(false);
    CHECK(labels(s.root()).find("Connection lost") != std::string::npos);
    CHECK(labels(s.root()).find("35") != std::string::npos);
    // Destruction and malformed/late replies must not touch deleted objects.
    s.on_back();
    choose(s, "Discard and close");
    vm.set_connected(true);
    choose(s, "Add a channel");
    result(vm, sent.back(), context);
    choose(s, "Create private");
    choose(s, "Channel name:");
    lv_textarea_set_text(input(s.root()), "Routine");
    s.on_enter();
    CHECK(s.on_key('1'));
    result(vm, sent.back(), R"("event":"channel.setup.preview","token":"malformed")");
    CHECK(labels(s.root()).find("Invalid channel validation") != std::string::npos);
    CHECK(sent.back().str_at("intent") != "channel.setup.commit");
    CHECK(s.on_key('1'));
    result(
        vm, sent.back(),
        R"("event":"channel.setup.preview","token":"routine","channels":[{"name":"Routine","private":true}])");
    CHECK(sent.back().str_at("intent") == "channel.setup.commit");
    CHECK(!sent.back().find("antenna_confirmed")->as_bool());
    CHECK(!sent.back().find("tuning_confirmed")->as_bool());
    const auto routine_commit = sent.back();
    const auto commit_count = sent.size();
    CHECK(s.on_key('1'));
    s.on_enter();
    CHECK(sent.size() == commit_count);
    result(
        vm, routine_commit,
        R"("event":"channel.setup.saved","added":1,"verified":"radio","channels":[{"index":1,"identity":"routine"}])");
    CHECK(labels(s.root()).find("Added: Routine") != std::string::npos);
    CHECK(s.on_key('1'));
    CHECK(sent.size() == commit_count);
    s.on_back();
    vm.set_connected(true);
    choose(s, "Add a channel");
    result(vm, sent.back(), context);
    choose(s, "Paste share URL");
    lv_textarea_set_text(input(s.root()), "https://meshtastic.org/e/#synthetic");
    s.on_enter();
    result(
        vm, sent.back(),
        R"("event":"channel.preview","new":2,"token":"multi","channels":[{"name":"Private","private":true},{"name":"Public","private":false}])");
    choose(s, "Automatic location:");
    choose(s, "Share: Public");
    CHECK(s.on_key('1'));
    CHECK(sent.back().find("position")->as_bool());
    CHECK(sent.back().int_at("position_target") == 1);
    s.on_back();
    choose(s, "Cancel setup");
    choose(s, "Discard and close");
    // Separate-profile Save is also one action and never requires live-radio consent.
    choose(s, "Add a channel");
    result(vm, sent.back(), context);
    choose(s, "Create private");
    choose(s, "Channel name:");
    lv_textarea_set_text(input(s.root()), "Profile1");
    s.on_enter();
    choose(s, "Save to:");
    choose(s, "Separate profile");
    choose(s, "Profile name:");
    CHECK(!s.on_key('1'));
    lv_textarea_set_text(input(s.root()), "Stored trip");
    s.on_enter();
    CHECK(labels(s.root()).find("1: Save profile") != std::string::npos);
    CHECK(s.on_key('1'));
    result(
        vm, sent.back(),
        R"("event":"channel.setup.preview","token":"profile","channels":[{"name":"Profile1","private":true}],"profile_only":true,"tuning_changed":true)");
    CHECK(sent.back().str_at("intent") == "channel.setup.commit");
    CHECK(sent.back().find("tuning_confirmed")->as_bool());
    CHECK(!sent.back().find("antenna_confirmed")->as_bool());
    result(
        vm, sent.back(),
        R"("event":"channel.setup.saved","added":1,"verified":"stored","profile_only":true,"profile":"synthetic-stored","channels":[])");
    CHECK(labels(s.root()).find("Includes: Profile1") != std::string::npos);
    CHECK(labels(s.root()).find("Open conversation") == std::string::npos);
    s.on_back();
}
int main(int argc, char **argv) {
    const std::string dir = argc == 2 ? argv[1] : "";
    if (!dir.empty())
        std::filesystem::create_directories(dir);
    lv_init();
    auto *display = lv_display_create(320, 170);
    run(dir, false);
    run(dir, true);
    lv_display_delete(display);
    std::cout << "Unified channel setup UI, privacy, cancellation and result regressions passed\n";
}
