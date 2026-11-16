// SPDX-License-Identifier: MIT
#include "base_viewmodel.h"
#include "application_config.h"
#include <sys/stat.h>
#include <fstream>
#include "asset_manager.h"
#include "messages_screen.h"
#include "nodes_screen.h"
#include "map_screen.h"
#include "menu_screen.h"
#include <filesystem>
#include "settings_screen.h"
#include "supervisor_client.h"
#include "json.h"
#include <png.h>
#include <sys/socket.h>
#include <unistd.h>
#include <fcntl.h>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <thread>

#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " << #x << '\n'; std::abort(); } } while (0)

namespace platform {
struct SupervisorClientTest {
    static void queue_bounds() {
        SupervisorClient client;
        json::Value v; v.type = json::Value::Obj;
        for (int i = 0; i < 128; ++i) CHECK(client.enqueue(json::Value(v)));
        CHECK(!client.enqueue(json::Value(v))); CHECK(client.queue_.empty());
        CHECK(client.overflow_);
        client.overflow_ = false;
        for (int i = 0; i < 40; ++i) CHECK(client.enqueue(json::Value(v)));
        int count = 0; client.on_event_ = [&](const json::Value&) { ++count; };
        client.poll(); CHECK(count == 32); client.poll(); CHECK(count == 40);
    }
    static void partial_frame() {
        int fds[2]; CHECK(socketpair(AF_UNIX, SOCK_STREAM, 0, fds) == 0);
        int size = 4096; setsockopt(fds[0], SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
        setsockopt(fds[1], SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
#ifdef SO_NOSIGPIPE
        int one = 1; setsockopt(fds[0], SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof(one));
#endif
        CHECK(fcntl(fds[0], F_SETFL, O_NONBLOCK) == 0);
        SupervisorClient client;
        client.fd_ = fds[0]; client.running_ = true; client.connected_ = true;
        client.writer_ = std::thread([&] { client.write_loop(); });
        std::string line = "{\"intent\":\"status\",\"padding\":[0";
        for (int i = 0; i < 20000; ++i) line += ",0";
        line += "]}";
        auto start = std::chrono::steady_clock::now();
        CHECK(client.send_line(line));
        CHECK(std::chrono::steady_clock::now() - start < std::chrono::milliseconds(50));
        std::this_thread::sleep_for(std::chrono::milliseconds(700));
        CHECK(!client.connected());
        CHECK(!client.send_line("{\"intent\":\"radio.start\"}"));
        std::string wire; char buf[4096]; ssize_t n;
        while ((n = read(fds[1], buf, sizeof(buf))) > 0) wire.append(buf, n);
        CHECK(!wire.empty()); CHECK(wire.find('\n') == std::string::npos);
        CHECK(wire.find("radio.start") == std::string::npos);
        bool failure = false;
        const auto ui_thread = std::this_thread::get_id();
        client.on_event_ = [&](const json::Value& ev) {
            CHECK(std::this_thread::get_id() == ui_thread);
            if (ev.str_at("event") == "error") failure = true;
        };
        client.poll(); CHECK(failure);
        client.stop(); close(fds[0]); close(fds[1]);
    }
};
}

static std::string labels(lv_obj_t* obj) {
    std::string result;
    if (lv_obj_check_type(obj, &lv_label_class)) result += std::string(lv_label_get_text(obj)) + '\n';
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i) result += labels(lv_obj_get_child(obj, i));
    return result;
}

static lv_obj_t* textarea(lv_obj_t* obj) {
    if (lv_obj_check_type(obj, &lv_textarea_class)) return obj;
    for (uint32_t i = 0; i < lv_obj_get_child_count(obj); ++i)
        if (auto* found = textarea(lv_obj_get_child(obj, i))) return found;
    return nullptr;
}

static void save_png(const char* path, lv_obj_t* root);

static void dual_channel_ui_regression(const char* screenshot_path) {
    viewmodel::BaseViewModel vm;
    std::string sent;
    vm.set_command_sink([&](const std::string& s) { sent = s; return true; });
    vm.set_connected(true); vm.apply_status("online", "US", "Demo");
    vm.set_channels({{0, "Public", false, "PRIMARY"}, {1, "Private A", true, "SECONDARY"}, {2, "Private B", true, "SECONDARY"}});
    viewmodel::NodeInfo peer; peer.id = "!11111111"; peer.name = std::string(80, 'P'); vm.set_nodes({peer});
    app::AssetManager assets;
    screen::MessagesScreen screen(vm, assets); lv_screen_load(screen.root());
    auto capture = [&](const char* name, lv_obj_t* root) {
        if (screenshot_path) save_png((std::filesystem::path(screenshot_path).parent_path() / name).c_str(), root);
    };
    screen.on_enter(); // New message
    capture("public-private-picker.png", screen.root());
    CHECK(labels(screen.root()).find("[Public]") != std::string::npos);
    CHECK(labels(screen.root()).find("[Private]") != std::string::npos);
    screen.on_enter(); // public group
    auto write = [&](int channel, const std::string& destination) {
        screen.on_enter(); auto* input = textarea(screen.root()); CHECK(input);
        lv_textarea_set_text(input, "Synthetic dual channel test"); screen.on_enter();
        json::Value v; CHECK(json::parse(sent, v)); CHECK(v.str_at("intent") == "message.send");
        CHECK(v.int_at("channel") == channel); CHECK(v.str_at("dest") == destination);
    };
    write(0, "");
    const auto before = sent;
    screen.on_key('o'); screen.on_down(); screen.on_enter(); // public precise location
    CHECK(labels(screen.root()).find("Precise location") != std::string::npos);
    capture("public-location-confirm.png", screen.root());
    screen.on_enter(); CHECK(sent == before); // default Cancel
    screen.on_key('o'); screen.on_down(); screen.on_enter(); screen.on_down(); screen.on_enter();
    json::Value v; CHECK(json::parse(sent, v)); CHECK(v.str_at("intent") == "position.send");
    CHECK(v.int_at("channel") == 0); CHECK(v.find("public_confirmed")->as_bool());
    CHECK(screen.on_back()); screen.on_enter(); screen.on_down(); screen.on_enter();
    write(1, "");
    CHECK(screen.on_back()); screen.on_enter();
    screen.on_down(); screen.on_down(); screen.on_down(); screen.on_enter(); // Peer -> DM channel picker
    CHECK(labels(screen.root()).find("DM via channel") != std::string::npos);
    capture("dm-channel-picker.png", screen.root());
    const auto before_dm = sent;
    screen.on_down(); screen.on_enter(); CHECK(sent == before_dm); // choose private A
    write(1, peer.id);
    screen.on_key('o'); screen.on_down(); screen.on_down(); screen.on_enter(); screen.on_enter(); // choose public DM
    write(0, peer.id);
    CHECK(labels(screen.root()).find("DM via 0: Public [Public]") != std::string::npos);
    capture("public-dm-long-name.png", screen.root());
    const auto before_removed = sent;
    vm.set_channels({{1, "Private A", true, "SECONDARY"}});
    screen.on_enter(); CHECK(sent == before_removed); // removed public channel cannot fall back to private
    CHECK(labels(screen.root()).find("DM via channel") != std::string::npos);
    CHECK(screen.on_back());

    vm.set_channels({{0, "Public", false, "PRIMARY"}, {1, "Private A", true, "SECONDARY"}});
    vm.apply_config("LONG_FAST", 3, false, false, "", 20);
    screen::SettingsScreen settings(vm, assets); lv_screen_load(settings.root());
    for (int i = 0; i < 6; ++i) settings.on_down();
    const auto before_location = sent;
    settings.on_enter(); settings.on_enter(); // public channel -> confirmation
    CHECK(labels(settings.root()).find("Public location") != std::string::npos);
    capture("public-position-confirm.png", settings.root());
    settings.on_enter(); CHECK(sent == before_location); // Cancel
    settings.on_enter(); settings.on_enter(); settings.on_down(); settings.on_enter();
    CHECK(json::parse(sent, v)); CHECK(v.str_at("path") == "position.enabled");
    CHECK(v.int_at("channel") == 0); CHECK(v.find("public_confirmed")->as_bool());
    const auto request = v.str_at("id");
    vm.set_channels({{0, "Public", false, "PRIMARY", 15}, {1, "Private A", true, "SECONDARY"}});
    vm.apply_config("LONG_FAST", 3, true, false, request, 20);
    CHECK(labels(settings.root()).find("Public: 0") != std::string::npos);
    for (int i = 6; i < 17; ++i) settings.on_down();
    settings.on_enter(); auto* slot = textarea(settings.root()); CHECK(slot);
    lv_textarea_set_text(slot, "21"); settings.on_enter();
    CHECK(json::parse(sent, v)); CHECK(v.str_at("path") == "lora.channel_num"); CHECK(v.int_at("value") == 21);
}

static void save_png(const char* path, lv_obj_t* root) {
    lv_obj_update_layout(root);
    auto* shot = lv_snapshot_take(root, LV_COLOR_FORMAT_ARGB8888); CHECK(shot);
    png_image image{}; image.version = PNG_IMAGE_VERSION;
    image.width = shot->header.w; image.height = shot->header.h; image.format = PNG_FORMAT_BGRA;
    CHECK(png_image_write_to_file(&image, path, false, shot->data, shot->header.stride, nullptr));
    lv_draw_buf_destroy(shot);
}

static void settings_file_regression() {
    char directory[] = "/tmp/mesh-settings-XXXXXX"; CHECK(mkdtemp(directory));
    const std::string path = std::string(directory) + "/settings.ini";
    app::ApplicationConfig a, b;
    a.canned = {"hello\n[messaging]\nauto_start=yes"};
    b.canned = {"second writer"}; b.retention_cap = 100;
    auto save = [&](const app::ApplicationConfig& config) {
        for (int i = 0; i < 10; ++i) { std::string error; CHECK(app::save_application_config(path, config, error)); }
    };
    std::thread first([&] { save(a); }); std::thread second([&] { save(b); });
    first.join(); second.join();
    std::string error; CHECK(app::save_application_config(path, a, error));
    struct stat st{}; CHECK(stat(path.c_str(), &st) == 0); CHECK((st.st_mode & 0777) == 0600);
    app::ApplicationConfig loaded; CHECK(app::load_application_config(path, loaded, error));
    CHECK(!loaded.auto_start); CHECK(loaded.canned.size() == 1);
    { std::ofstream file(path); file << "[messaging]\nretention=9999999999999999999999999999\n"; }
    CHECK(app::load_application_config(path, loaded, error)); CHECK(loaded.retention_cap == 0);
    for (const auto& entry : std::filesystem::directory_iterator(directory)) CHECK(entry.path().filename() == "settings.ini");
    std::filesystem::remove_all(directory);
}

int main(int argc, char** argv) {
    settings_file_regression();
    json::Value v;
    CHECK(json::parse("{\"text\":\"\\ud83d\\udccd\"}", v)); CHECK(v.str_at("text").size() == 4);
    for (const auto& bad : {"{}{}", "01", "+1", "1e", "1.2.3", "1e999", "{\"x\":1,\"x\":2}", "\"raw\nnewline\"", "{  "}) CHECK(!json::parse(bad, v));
    CHECK(!json::parse(std::string("\"\xff\""), v));
    CHECK(!json::parse("\"" + std::string(4098, 'x') + "\"", v));
    CHECK(json::parse("1e30", v)); CHECK(v.as_int(-1) == -1); CHECK(v.as_int32(-1) == -1);
    CHECK(!json::parse(std::string(65, '[') + "0" + std::string(65, ']'), v));
    CHECK(json::parse("\"" + json::escape(std::string("\1\b\f", 3)) + "\"", v));
    platform::SupervisorClientTest::queue_bounds();
    platform::SupervisorClientTest::partial_frame();
    lv_init(); auto* display = lv_display_create(320, 170); CHECK(display);
    dual_channel_ui_regression(argc > 1 ? argv[1] : nullptr);
    {
        viewmodel::BaseViewModel vm;
        std::string sent;
        vm.set_command_sink([&](const std::string& s) { sent = s; return true; });
        vm.set_position_sharing(false); CHECK(std::string(vm.config_state()) == "unknown");
        CHECK(!lv_subject_get_int(vm.position_subject()));
        vm.set_connected(true); CHECK(sent.find("message.retention") != std::string::npos);
        vm.apply_status("online", "UNSET", "Demo");
        vm.set_channels({{0, "Public", false, "PRIMARY"}, {1, "Field One", true, "SECONDARY"}, {2, "Field Two", true, "SECONDARY"}});
        CHECK(vm.channel(0) && !vm.channel(0)->isPrivate); CHECK(vm.channel(1)->isPrivate); CHECK(!vm.channel(7));
        vm.apply_config("MEDIUM_FAST", 3, true, false);
        vm.set_position_sharing(false); CHECK(std::string(vm.config_state()) == "pending");
        CHECK(lv_subject_get_int(vm.position_subject()));
        CHECK(json::parse(sent, v)); auto id = v.str_at("id");
        vm.apply_config("MEDIUM_FAST", 3, false, false, "wrong-request");
        CHECK(std::string(vm.config_state()) == "pending");
        vm.apply_config("MEDIUM_FAST", 3, false, false, id);
        CHECK(std::string(vm.config_state()) == "confirmed"); CHECK(!lv_subject_get_int(vm.position_subject()));
        vm.set_command_sink([](const std::string&) { return false; });
        vm.send_message(1, "Offline test"); CHECK(vm.messages().back().status == viewmodel::kMsgFailed);
        vm.set_connected(false); CHECK(std::string(vm.config_state()) == "unknown");
        vm.set_command_sink([&](const std::string& s) { sent = s; return true; });
        vm.request_channel_share(2); CHECK(json::parse(sent, v)); id = v.str_at("id");
        vm.apply_channel_share(1, "Wrong", "wrong-url", id); CHECK(vm.channel_share_url().empty());
        vm.cancel_channel_share(); vm.apply_channel_share(2, "Field Two", "late-url", id); CHECK(vm.channel_share_url().empty());
        for (int i = 0; i < 2100; ++i) { viewmodel::MessageInfo m; m.id = std::to_string(i); m.to = "broadcast"; vm.add_message(m); }
        CHECK(vm.messages().size() == 2000);
        vm.set_messages({}); CHECK(vm.total_unread() == 0);
        viewmodel::MessageInfo m; m.id = "demo-1"; m.from = "!11111111"; m.fromName = "Field One";
        m.to = "broadcast"; m.channel = 2; m.time = 1; m.text = "Radio check. All clear.";
        vm.add_message(m); vm.add_message(m); CHECK(vm.messages().size() == 1);
        vm.apply_status("online", "UNSET", "Demo");
        app::AssetManager assets;
        viewmodel::NodeInfo demo; demo.id = "!11111111"; demo.name = "Field One";
        demo.hasPos = true; demo.lat = 0.01; demo.lon = 0.01;
        viewmodel::NodeInfo local = demo; local.id = "!22222222"; local.name = "Demo"; local.isSelf = true;
        local.lat = 0; local.lon = 0;
        auto malformed = demo; malformed.lat = 1e308;
        vm.set_nodes({local, malformed}); CHECK(!vm.nodes()[1].hasPos);
        auto antipode = demo; antipode.lat = 0; antipode.lon = 180;
        vm.set_nodes({local, antipode});
        { screen::MapScreen map(vm, assets); CHECK(lv_obj_is_valid(map.root())); }
        vm.set_nodes({local, demo});
        screen::MessagesScreen screen(vm, assets); lv_screen_load(screen.root());
        screen.on_down(); screen.on_enter();  // open group 2
        CHECK(labels(screen.root()).find("Field Two") != std::string::npos);
        if (argc > 1) save_png(argv[1], screen.root());
        screen.on_key('o'); screen.on_down(); screen.on_down(); screen.on_enter();
        CHECK(json::parse(sent, v)); CHECK(v.str_at("intent") == "channel.share"); CHECK(v.int_at("channel") == 2);
        id = v.str_at("id");
        CHECK(screen.on_back());  // cancel pending share overlay
        vm.apply_channel_share(2, "Field Two", "late-url", id);
        CHECK(labels(screen.root()).find("late-url") == std::string::npos);
        screen::SettingsScreen settings(vm, assets);
        CHECK(labels(settings.root()).find("Unknown") != std::string::npos);
        if (argc > 1) {
            const auto directory = std::filesystem::path(argv[1]).parent_path();
            auto save = [&](const char* name, lv_obj_t* root) { save_png((directory / name).c_str(), root); };
            vm.apply_config("MEDIUM_FAST", 3, false, false);
            save("settings.png", settings.root());
            screen::NodesScreen nodes(vm, assets); save("nodes.png", nodes.root());
            screen::MapScreen map(vm, assets); save("map.png", map.root());
            screen::MenuScreen menu(vm, assets); save("carousel.png", menu.root());
        }
    }
    lv_display_delete(display);
    std::cout << "Mesh UI, parser, and partial-write regressions passed\n";
}
