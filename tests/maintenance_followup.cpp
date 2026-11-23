// SPDX-License-Identifier: MIT
// Real LVGL with synthetic state only. No radio, framebuffer or private data.
#include "base_viewmodel.h"
#include "asset_manager.h"
#include "messages_screen.h"
#include "nodes_screen.h"
#include "menu_screen.h"
#include "map_screen.h"
#include "settings_screen.h"
#include "device_status.h"
#include "geo.h"
#include <fstream>
#include <set>
#include <cstdlib>
#include <iostream>
#include <string>

#define CHECK(x)                                                                                             \
    do {                                                                                                     \
        if (!(x)) {                                                                                          \
            std::cerr << "FAIL " << __LINE__ << ": " << #x << '\n';                                          \
            std::abort();                                                                                    \
        }                                                                                                    \
    } while (0)

static std::string labels(lv_obj_t *obj) {
    std::string out;
    if (lv_obj_check_type(obj, &lv_label_class))
        out += std::string(lv_label_get_text(obj)) + '\n';
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i)
        out += labels(lv_obj_get_child(obj, i));
    return out;
}
static lv_obj_t *label(lv_obj_t *obj, const std::string &text) {
    if (lv_obj_check_type(obj, &lv_label_class) && text == lv_label_get_text(obj))
        return obj;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i)
        if (auto *found = label(lv_obj_get_child(obj, i), text))
            return found;
    return nullptr;
}
static json::Value parse(const std::string &text) {
    json::Value v;
    CHECK(json::parse(text, v));
    return v;
}
static lv_obj_t *textarea(lv_obj_t *object) {
    if (lv_obj_check_type(object, &lv_textarea_class))
        return object;
    for (uint32_t i = 0; i < lv_obj_get_child_count(object); ++i)
        if (auto *found = textarea(lv_obj_get_child(object, i)))
            return found;
    return nullptr;
}
static std::string selected_row(lv_obj_t *object) {
    if (lv_obj_get_style_border_width(object, LV_PART_MAIN) == 3) {
        const auto text = labels(object);
        return text.substr(0, text.find('\n'));
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(object); ++i) {
        const auto text = selected_row(lv_obj_get_child(object, i));
        if (!text.empty())
            return text;
    }
    return "";
}
static void draft_loss() {
    viewmodel::BaseViewModel vm;
    vm.set_command_sink([](const std::string &) { return false; });
    vm.save_draft("ch:synthetic", "New unsaved text");
    vm.flush_drafts();
    vm.apply_threads(parse(R"({"threads":{"ch:synthetic":{"draft":"Old saved text","pinned":false}}})"));
    CHECK(vm.draft("ch:synthetic") == "New unsaved text");
}
static json::Value acknowledgment(const json::Value &request, bool durable = true) {
    return parse("{\"event\":\"thread.saved\",\"id\":\"" + request.str_at("id") + "\",\"thread\":\"" +
                 request.str_at("thread") + "\",\"durable\":" + (durable ? "true" : "false") + "}");
}
static void draft_acceptance() {
    viewmodel::BaseViewModel vm;
    std::vector<json::Value> sent;
    bool available = true;
    vm.set_command_sink([&](const std::string &text) {
        if (!available)
            return false;
        sent.push_back(parse(text));
        return true;
    });
    vm.set_connected(true);
    sent.clear();
    vm.save_draft("ch:synthetic", "one");
    vm.flush_drafts();
    const auto first = sent.back();
    vm.save_draft("ch:synthetic", "two");
    vm.apply_thread_result(acknowledgment(first));
    const auto old = parse(R"({"threads":{"ch:synthetic":{"draft":"old","pinned":false}}})");
    vm.apply_threads(old);
    CHECK(vm.draft("ch:synthetic") == "two");
    vm.flush_drafts();
    const auto second = sent.back();
    CHECK(vm.thread_state_pending());
    CHECK(first.str_at("id") != second.str_at("id"));
    vm.apply_thread_result(parse("{\"event\":\"thread.saved\",\"id\":\"" + second.str_at("id") +
                                 "\",\"thread\":\"ch:synthetic\",\"durable\":1}"));
    vm.apply_threads(old);
    CHECK(vm.draft("ch:synthetic") == "two");
    vm.apply_thread_result(acknowledgment(second, false));
    CHECK(!vm.thread_state_pending());
    vm.apply_threads(old);
    CHECK(vm.draft("ch:synthetic") == "old"); // accepted, then another client updated
    available = false;
    vm.save_draft("ch:synthetic", "rejected");
    vm.flush_drafts();
    vm.apply_threads(old);
    CHECK(vm.draft("ch:synthetic") == "rejected");
    available = true;
    vm.flush_drafts();
    const auto interrupted = sent.back();
    vm.set_connected(false);
    vm.set_connected(true);
    vm.apply_thread_result(acknowledgment(interrupted));
    vm.apply_threads(old);
    CHECK(vm.draft("ch:synthetic") == "rejected");
    vm.flush_drafts();
    const auto retry = sent.back();
    CHECK(retry.str_at("id") != interrupted.str_at("id"));
    vm.apply_thread_result(acknowledgment(retry));
    vm.toggle_pin("ch:synthetic");
    const auto pin = sent.back();
    vm.apply_threads(old);
    CHECK(vm.pinned("ch:synthetic"));
    vm.apply_thread_result(acknowledgment(pin));
    vm.apply_threads(old);
    CHECK(!vm.pinned("ch:synthetic"));
    vm.save_draft("ch:synthetic", "to delete");
    vm.flush_drafts();
    const auto deleted = sent.back();
    vm.delete_thread("ch:synthetic");
    vm.apply_thread_result(acknowledgment(deleted));
    CHECK(vm.draft("ch:synthetic").empty());
    sent.clear();
    vm.tick(std::chrono::steady_clock::now() + std::chrono::seconds(5));
    for (const auto &request : sent)
        CHECK(request.str_at("intent") != "thread.state" && request.str_at("intent") != "message.send");
}
static void node_selection() {
    viewmodel::BaseViewModel vm;
    app::AssetManager assets;
    viewmodel::NodeInfo me, a, b;
    me.id = "!10000000";
    me.isSelf = true;
    a.id = "!10000001";
    a.name = "Synthetic A";
    a.lastHeard = 20;
    b.id = "!10000002";
    b.name = "Synthetic B";
    b.lastHeard = 10;
    vm.set_nodes({me, a, b});
    screen::NodesScreen screen(vm, assets);
    screen.on_down(); // select A
    b.lastHeard = 30;
    vm.set_nodes({me, a, b});
    screen.on_key('m');
    CHECK(vm.dm_target_id() == a.id);
    vm.clear_dm_target();
    vm.set_nodes({me, b});
    screen.on_key('m');
    CHECK(vm.dm_target_id().empty());
}
static void mode_status() {
    viewmodel::BaseViewModel vm;
    app::AssetManager assets;
    vm.apply_health("receive-only", "");
    vm.apply_status("online", "US", "Synthetic");
    screen::MenuScreen screen(vm, assets);
    CHECK(labels(screen.root()).find("RX\n") != std::string::npos);
    vm.apply_health("transmit", "");
    vm.apply_status("online", "US", "Synthetic");
    CHECK(labels(screen.root()).find("TX/RX") != std::string::npos);
    vm.apply_health("receive-only", "");
    CHECK(labels(screen.root()).find("TX/RX") == std::string::npos);
    vm.apply_health("unknown", "");
    CHECK(label(screen.root(), "Online ?"));
    vm.apply_health("receive-only", "");
    vm.set_connected(false);
    CHECK(vm.radio_mode()=="unknown");
}
static void bubble_selection() {
    viewmodel::BaseViewModel vm;
    app::AssetManager assets;
    vm.set_channels({{1, "Synthetic", true, "SECONDARY", 0, "synthetic"}});
    std::vector<viewmodel::MessageInfo> messages;
    for (int i = 0; i < 3; ++i) {
        viewmodel::MessageInfo m;
        m.id = std::to_string(i);
        m.from = "!10000001";
        m.to = "broadcast";
        m.text = "Synthetic message " + m.id;
        m.channel = 1;
        m.channelIdentity = "synthetic";
        messages.push_back(m);
    }
    bool deleted = false;
    vm.set_messages(messages);
    screen::MessagesScreen screen(vm, assets);
    screen.on_down();
    screen.on_enter();
    screen.on_up();
    screen.on_up(); // middle message
    messages.erase(messages.begin());
    vm.set_messages(messages);
    auto *selected = label(screen.root(), "Synthetic message 1");
    CHECK(selected);
    CHECK(lv_obj_get_style_border_width(lv_obj_get_parent(selected), LV_PART_MAIN) == 2);
    // Selection moves by changing existing styles, without deleting a bubble.
    lv_obj_add_event_cb(
        lv_obj_get_parent(selected),
        [](lv_event_t *e) { *static_cast<bool *>(lv_event_get_user_data(e)) = true; }, LV_EVENT_DELETE,
        &deleted);
    screen.on_down();
    screen.on_up();
    CHECK(!deleted);
}
static void historical_channel() {
    viewmodel::BaseViewModel vm;
    app::AssetManager assets;
    vm.set_channels({{1, "Original", true, "SECONDARY", 0, "original"}});
    viewmodel::MessageInfo m;
    m.id = "old";
    m.from = "!10000001";
    m.to = "broadcast";
    m.text = "Synthetic history";
    m.channel = 1;
    m.channelIdentity = "original";
    m.channelName = "Original";
    m.channelPrivate = true;
    vm.set_messages({m});
    screen::MessagesScreen screen(vm, assets);
    screen.on_down();
    screen.on_enter();
    vm.set_channels({{1, "Replacement", false, "SECONDARY", 0, "replacement"}});
    screen.on_up();
    screen.on_enter();
    screen.on_enter();
    CHECK(labels(screen.root()).find("Channel: Original [Private]") != std::string::npos);
    CHECK(labels(screen.root()).find("Channel: 1: Replacement") == std::string::npos);
}
static void picker_identity() {
    viewmodel::BaseViewModel vm;
    app::AssetManager assets;
    vm.set_channels({{1, "Synthetic", true, "SECONDARY", 0, "synthetic"}});
    viewmodel::NodeInfo a, b;
    a.id = "!10000001";
    a.name = "Synthetic A";
    b.id = "!10000002";
    b.name = "Synthetic B";
    vm.set_nodes({a, b});
    screen::MessagesScreen screen(vm, assets);
    screen.on_enter();
    screen.on_down(); // New message, group first then A
    b.name = "Updated B";
    vm.set_nodes({b, a});
    screen.on_enter();
    CHECK(vm.dm_target_id() == a.id);
}
static void public_channel_flow() {
    viewmodel::BaseViewModel vm;
    app::AssetManager assets;
    json::Value sent;
    vm.set_command_sink([&](const std::string &text) {
        sent = parse(text);
        return true;
    });
    vm.set_connected(true);
    vm.apply_health("receive-only", "");
    vm.apply_status("online", "US", "Synthetic");
    screen::SettingsScreen screen(vm, assets);
    for (int i = 0; i < 26 && selected_row(screen.root()) != "Manage channels"; ++i)
        screen.on_down();
    CHECK(selected_row(screen.root()) == "Manage channels");
    screen.on_enter();
    CHECK(sent.str_at("intent") == "channel.slots");
    vm.apply_service(
        parse("{\"event\":\"channel.slots\",\"id\":\"" + sent.str_at("id") + "\",\"channels\":[]}"));
    screen.on_enter(); // unified add flow
    CHECK(sent.str_at("intent") == "channel.setup.context");
    vm.apply_service(
        parse("{\"id\":\"" + sent.str_at("id") +
              "\",\"event\":\"channel.setup.context\",\"base\":\"synthetic\",\"tx\":false,\"tuning\":{"
              "\"region\":\"US\",\"preset\":\"LONG_FAST\",\"slot\":20,\"hop\":3,\"use_preset\":true}}"));
    screen.on_down();
    screen.on_down();
    screen.on_down();
    screen.on_enter();
    CHECK(labels(screen.root()).find("Add by: public") != std::string::npos);
    CHECK(labels(screen.root()).find("Region (whole radio): US") != std::string::npos);
    CHECK(labels(screen.root()).find("Internet forwarding: Off") != std::string::npos);
}
static void freshness() {
    viewmodel::BaseViewModel vm;
    app::AssetManager assets;
    const long now = 10000;
    vm.tick(std::chrono::steady_clock::now(), now);
    viewmodel::NodeInfo me, a;
    me.id = "!10000000";
    me.isSelf = true;
    me.hasPos = true;
    me.positionTime = now;
    a = me;
    a.isSelf = false;
    a.id = "!10000001";
    a.name = "Synthetic peer";
    a.hasBattery = true;
    a.battery = 50;
    a.lastHeard = now;
    a.lat = 0.01;
    vm.set_nodes({me, a});
    {
        screen::MapScreen map(vm, assets);
        CHECK(label(map.root(), "you"));
        const auto revision = lv_subject_get_int(vm.nodes_version_subject());
        vm.tick(std::chrono::steady_clock::now(), now + 121);
        CHECK(label(map.root(), "you (last known)"));
        CHECK(lv_subject_get_int(vm.nodes_version_subject()) == revision);
    }
    {
        screen::NodesScreen nodes(vm, assets);
        nodes.on_down();
        nodes.on_enter();
        CHECK(labels(nodes.root()).find("121s (stale)") != std::string::npos);
        a.battery = 62;
        auto *detail = lv_obj_get_parent(lv_obj_get_parent(label(nodes.root(), "Fix age")));
        lv_obj_update_layout(detail);
        lv_obj_scroll_to_y(detail, 40, LV_ANIM_OFF);
        const auto scroll = lv_obj_get_scroll_y(detail);
        vm.set_nodes({me, a});
        CHECK(labels(nodes.root()).find("62%") != std::string::npos);
        lv_obj_update_layout(detail);
        CHECK(lv_obj_get_scroll_y(detail) == scroll);
        vm.tick(std::chrono::steady_clock::now(), now + 122);
        CHECK(label(nodes.root(), "122s (stale)"));
        vm.set_nodes({me});
        CHECK(label(nodes.root(), "Node unavailable"));
        nodes.on_key('m');
        CHECK(vm.dm_target_id().empty());
    }
    vm.tick(std::chrono::steady_clock::now(), now + 123); // destroyed-screen callbacks must be gone
    CHECK(std::abs(viewmodel::geo::haversine_km(0, 0, 0, 180) - 20015.0868) < 0.01);
    CHECK(viewmodel::geo::haversine_km(50, 20, 50, 20) == 0);
}
static void unchanged_nodes_settings() {
    viewmodel::BaseViewModel vm;
    app::AssetManager assets;
    viewmodel::NodeInfo n;
    n.id = "!10000000";
    vm.set_nodes({n});
    const auto revision = lv_subject_get_int(vm.nodes_version_subject());
    vm.set_nodes({n});
    CHECK(lv_subject_get_int(vm.nodes_version_subject()) == revision);
    bool deleted = false;
    screen::SettingsScreen screen(vm, assets);
    auto *radio = label(screen.root(), "Radio");
    CHECK(radio);
    lv_obj_add_event_cb(
        lv_obj_get_parent(radio),
        [](lv_event_t *e) { *static_cast<bool *>(lv_event_get_user_data(e)) = true; }, LV_EVENT_DELETE,
        &deleted);
    vm.apply_health("receive-only", "");
    vm.apply_status("online", "US", "Synthetic");
    vm.apply_config("LONG_FAST", 3, false, false);
    CHECK(!deleted);
}
static void battery() {
    const auto root = std::filesystem::temp_directory_path() /
                      ("mesh-synthetic-battery-" +
                       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    CHECK(!std::filesystem::exists(root));
    std::filesystem::create_directories(root / "BAT");
    {
        std::ofstream(root / "BAT/type") << "Battery\n";
        std::ofstream(root / "BAT/capacity") << "73\n";
        std::ofstream(root / "BAT/status") << "Charging\n";
    }
    auto status = platform::read_battery_status(root);
    CHECK(status.battery_present && status.battery_charging && status.battery_percent == 73);
    {
        std::ofstream(root / "BAT/capacity") << "invalid\n";
    }
    CHECK(!platform::read_battery_status(root).battery_present);
    std::filesystem::remove_all(root);
    CHECK(!platform::read_battery_status(root).battery_present);
}
static void retention() {
    for (int peers : {1, 32, 256, 2000})
        for (int cap : {0, 1, 3, 100}) {
            viewmodel::BaseViewModel vm;
            std::vector<viewmodel::MessageInfo> input, expected;
            for (int i = 0; i < 2000; ++i) {
                viewmodel::MessageInfo m;
                m.id = std::to_string(i);
                m.to = "broadcast";
                m.channelIdentity = "synthetic-" + std::to_string(i % peers);
                input.push_back(m);
            }
            std::map<std::string, int> counts;
            for (auto it = input.rbegin(); it != input.rend(); ++it)
                if (!cap || ++counts[viewmodel::thread_key_of(*it)] <= cap)
                    expected.push_back(*it);
            std::reverse(expected.begin(), expected.end());
            vm.load_messaging_prefs(false, cap, false, true);
            const auto revision = lv_subject_get_int(vm.messages_version_subject());
            vm.set_messages(input);
            CHECK(lv_subject_get_int(vm.messages_version_subject()) == revision + 1);
            CHECK(vm.messages().size() == expected.size());
            for (size_t i = 0; i < expected.size(); ++i)
                CHECK(vm.messages()[i].id == expected[i].id);
        }
}
int main(int argc, char **argv) {
    lv_init();
    auto *display = lv_display_create(320, 170);
    CHECK(display);
    const std::string test = argc > 1 ? argv[1] : "all";
    if (test == "draft" || test == "all")
        draft_loss();
    if (test == "node" || test == "all")
        node_selection();
    if (test == "mode" || test == "all")
        mode_status();
    if (test == "bubble" || test == "all")
        bubble_selection();
    if (test == "acceptance" || test == "all")
        draft_acceptance();
    if (test == "history" || test == "all")
        historical_channel();
    if (test == "picker" || test == "all")
        picker_identity();
    if (test == "public" || test == "all")
        public_channel_flow();
    if (test == "freshness" || test == "all")
        freshness();
    if (test == "unchanged" || test == "all")
        unchanged_nodes_settings();
    if (test == "battery" || test == "all")
        battery();
    if (test == "retention" || test == "all")
        retention();
    lv_display_delete(display);
    std::cout << "Maintenance follow-up checks passed: " << test << '\n';
}
