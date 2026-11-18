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

// Navigate by the visible selected item, so layout changes cannot retarget a
// privacy/radio regression to a different action at an old numeric offset.
static std::string selected_setting(lv_obj_t* root) {
    if (lv_obj_get_style_border_width(root, LV_PART_MAIN) == 3 && lv_obj_get_child_count(root)) {
        auto* label = lv_obj_get_child(root, 0);
        if (lv_obj_check_type(label, &lv_label_class)) return lv_label_get_text(label);
    }
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i) {
        const auto name = selected_setting(lv_obj_get_child(root, i));
        if (!name.empty()) return name;
    }
    return "";
}
static void select_setting(screen::SettingsScreen& screen, const std::string& name) {
    for (int i = 0; i < 30; ++i) screen.on_up();
    for (int i = 0; i < 26; ++i) {
        if (selected_setting(screen.root()) == name) return;
        screen.on_down();
    }
    CHECK(false);
}
static lv_obj_t* label_starting(lv_obj_t* root, const std::string& prefix) {
    if (lv_obj_check_type(root, &lv_label_class) && std::string(lv_label_get_text(root)).rfind(prefix, 0) == 0) return root;
    for (uint32_t i = 0; i < lv_obj_get_child_count(root); ++i)
        if (auto* found = label_starting(lv_obj_get_child(root, i), prefix)) return found;
    return nullptr;
}

static void save_png(const char* path, lv_obj_t* root);

static void maintenance_data_regression() {
    viewmodel::BaseViewModel vm;
    std::string sent; vm.set_command_sink([&](const std::string& line) { sent=line; return true; });
    vm.set_connected(true);
    vm.set_channels({{2,"Field",true,"PRIMARY",0,"live"}});
    viewmodel::NodeInfo a; a.id="!11111111";a.name="Alpha";
    auto b=a;b.id="!22222222";b.name="Beta";vm.set_nodes({a,b});
    const auto dm = [](const std::string& peer,const std::string& text,const std::string& id,long time) {
        viewmodel::MessageInfo m;m.from=peer;m.to="self";m.text=text;m.id=id;m.time=time;return m;
    };
    auto group=dm("!44444444","Group text","group",15);group.to="broadcast";group.channel=2;group.channelIdentity="live";
    auto archived=group;archived.id="archived";archived.channelIdentity="retired";archived.channelName="Retired";archived.channelPrivate=true;archived.time=25;
    vm.set_messages({dm(a.id,"old needle","a1",10),dm(b.id,"Beta recent","b1",30),dm(a.id,"Alpha latest","a2",20),group,archived});
    vm.remember_conversation("dm:!33333333");
    vm.save_draft("dm:!11111111@a","first draft");vm.save_draft("dm:!11111111@z","second draft");vm.toggle_pin("dm:!22222222");
    auto rows=vm.conversation_rows();CHECK(rows.size()==5);
    CHECK(rows[0].key=="dm:!22222222" && rows[0].name=="* Beta");
    CHECK(rows[1].name=="Archived: Retired [Private]");
    CHECK(rows[2].key=="dm:!11111111" && rows[2].preview=="Draft: first draft");
    CHECK(rows[3].key=="ch:live" && rows[3].name=="# 2: Field [Private]");
    CHECK(rows[4].key=="dm:!33333333" && rows[4].preview=="No messages");
    CHECK(vm.conversation_rows("needle").size()==1);CHECK(vm.conversation_rows("first draft").size()==1);
    CHECK(vm.conversation_rows("second draft").empty());CHECK(vm.conversation_rows("Retired").size()==1);
    CHECK(vm.conversation_rows("* Beta").empty()); // A visual pin marker is not searchable content.
    CHECK(vm.conversation_rows("absent").empty());
    viewmodel::BaseViewModel equal;equal.set_command_sink([](const std::string&) {return true;});
    equal.set_messages({dm(a.id,"first","1",5),dm(b.id,"second","2",5),dm(a.id,"latest tie","3",5)});
    rows=equal.conversation_rows();CHECK(rows[0].key=="dm:!11111111" && rows[0].preview=="latest tie");CHECK(rows[1].key=="dm:!22222222");
    // A refresh can reorder chats, but Enter must still open the highlighted identity.
    viewmodel::BaseViewModel selected;selected.set_command_sink([](const std::string&) {return true;});
    selected.set_nodes({a,b}); selected.set_messages({dm(a.id,"Older","1",1),dm(b.id,"Newer","2",2)});
    selected.set_channels({{2,"Field",true,"PRIMARY",0,"live"}});
    app::AssetManager assets;screen::MessagesScreen screen(selected,assets);lv_screen_load(screen.root());
    screen.on_down(); // Beta is the latest chat.
    selected.set_messages({dm(a.id,"Now latest","3",3),dm(b.id,"Newer","2",2)});
    CHECK(selected_setting(screen.root())=="Beta");
    screen.on_enter();screen.on_enter();CHECK(labels(screen.root()).find("DM via 2: Field [Private] to Beta")!=std::string::npos);
}

static void unchanged_thread_regression() {
    viewmodel::BaseViewModel vm; std::string sent;
    vm.set_command_sink([&](const std::string& line) {sent=line;return true;});vm.set_connected(true);
    vm.set_channels({{1,"Synthetic",true,"PRIMARY",0,"synthetic"}});
    viewmodel::MessageInfo m;m.from="!11111111";m.to="broadcast";m.id="message";m.text="Synthetic message";m.channelIdentity="synthetic";m.channel=1;vm.set_messages({m});
    app::AssetManager assets;screen::MessagesScreen screen(vm,assets);lv_screen_load(screen.root());
    screen.on_down();screen.on_enter();screen.on_enter();auto* input=textarea(screen.root());CHECK(input);
    lv_textarea_set_text(input,"new local draft");
    json::Value event; CHECK(json::parse(R"({"event":"threads","history_enabled":true,"threads":{"ch:synthetic":{"draft":"old draft","pinned":false}}})",event));
    const auto revision=lv_subject_get_int(vm.messages_version_subject());vm.apply_threads(event);
    CHECK(vm.draft("ch:synthetic")=="new local draft");CHECK(lv_subject_get_int(vm.messages_version_subject())==revision);
    CHECK(screen.is_composing() && lv_obj_is_valid(input) && std::string(lv_textarea_get_text(input))=="new local draft");
    vm.flush_drafts();
    json::Value save;
    CHECK(json::parse(sent, save));
    CHECK(json::parse("{\"event\":\"thread.saved\",\"id\":\"" + save.str_at("id") +
                          "\",\"thread\":\"ch:synthetic\",\"durable\":true}",
                      event));
    vm.apply_thread_result(event);
    CHECK(json::parse(
        R"({"event":"threads","history_enabled":true,"threads":{"ch:synthetic":{"draft":"new local draft","pinned":false}}})",
        event));
    for (int i=0;i<100;++i) vm.apply_threads(event);
    CHECK(lv_subject_get_int(vm.messages_version_subject())==revision);
    CHECK(json::parse(R"({"event":"threads","history_enabled":false,"threads":{"ch:synthetic":{"draft":"new local draft","pinned":true}}})",event));
    vm.apply_threads(event);CHECK(lv_subject_get_int(vm.messages_version_subject())==revision+1);CHECK(vm.pinned("ch:synthetic"));CHECK(!vm.history_enabled());
    CHECK(vm.service_request("history.set",",\"enabled\":false"));json::Value request;CHECK(json::parse(sent,request));
    auto id=request.str_at("id");CHECK(json::parse("{\"event\":\"threads\",\"id\":\""+id+"\",\"history_enabled\":false,\"threads\":{\"ch:synthetic\":{\"draft\":\"new local draft\",\"pinned\":true}}}",event));
    vm.apply_threads(event);CHECK(lv_subject_get_int(vm.messages_version_subject())==revision+1);
    vm.tick(std::chrono::steady_clock::now()+std::chrono::seconds(91));CHECK(vm.service_event().str_at("event")=="threads");
    CHECK(json::parse(R"({"event":"threads","history_enabled":false,"threads":{}})",event));vm.apply_threads(event);
    CHECK(lv_subject_get_int(vm.messages_version_subject())==revision+2);CHECK(vm.conversation_keys().empty());
}

static void dual_channel_ui_regression(const char* screenshot_path) {
    viewmodel::BaseViewModel vm;
    std::string sent;
    vm.set_command_sink([&](const std::string& s) { sent = s; return true; });
    vm.set_connected(true); vm.apply_status("online", "US", "Demo");
    vm.set_channels({{0, "Public", false, "PRIMARY", 0, "synthetic-public"}, {1, "Private A", true, "SECONDARY", 0, "synthetic-private"}, {2, "Private B", true, "SECONDARY", 0, "synthetic-other"}});
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
    screen.on_key('o'); screen.on_down(); screen.on_down(); screen.on_enter(); // public precise location
    CHECK(labels(screen.root()).find("Precise location") != std::string::npos);
    capture("public-location-confirm.png", screen.root());
    screen.on_enter(); CHECK(sent == before); // default Cancel
    screen.on_key('o'); screen.on_down(); screen.on_down(); screen.on_enter(); screen.on_down(); screen.on_enter();
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
    screen.on_key('o'); screen.on_down(); screen.on_down(); screen.on_down(); screen.on_enter(); screen.on_enter(); // choose public DM
    write(0, peer.id);
    CHECK(labels(screen.root()).find("DM via 0: Public [Public]") != std::string::npos);
    capture("public-dm-long-name.png", screen.root());
    const auto before_removed = sent;
    vm.set_channels({{1, "Private A", true, "SECONDARY", 0, "synthetic-private"}});
    screen.on_enter(); CHECK(sent == before_removed); // removed public channel cannot fall back to private
    CHECK(labels(screen.root()).find("DM via channel") != std::string::npos);
    CHECK(screen.on_back());

    vm.set_channels({{0, "Public", false, "PRIMARY", 0, "synthetic-public"}, {1, "Private A", true, "SECONDARY", 0, "synthetic-private"}});
    vm.apply_config("LONG_FAST", 3, false, false, "", 20);
    screen::SettingsScreen settings(vm, assets); lv_screen_load(settings.root());
    select_setting(settings, "Share position");
    const auto before_location = sent;
    settings.on_enter(); settings.on_enter(); // public channel -> confirmation
    CHECK(labels(settings.root()).find("Public location") != std::string::npos);
    capture("public-position-confirm.png", settings.root());
    settings.on_enter(); CHECK(sent == before_location); // Cancel
    settings.on_enter(); settings.on_enter(); settings.on_down(); settings.on_enter();
    CHECK(json::parse(sent, v)); CHECK(v.str_at("path") == "position.enabled");
    CHECK(v.int_at("channel") == 0); CHECK(v.find("public_confirmed")->as_bool());
    const auto request = v.str_at("id");
    vm.set_channels({{0, "Public", false, "PRIMARY", 15, "synthetic-public"}, {1, "Private A", true, "SECONDARY", 0, "synthetic-private"}});
    vm.apply_config("LONG_FAST", 3, true, false, request, 20);
    CHECK(labels(settings.root()).find("Public: 0") != std::string::npos);
    select_setting(settings, "Frequency slot");
    settings.on_enter(); auto* slot = textarea(settings.root()); CHECK(slot);
    lv_textarea_set_text(slot, "21"); settings.on_enter();
    CHECK(json::parse(sent, v)); CHECK(v.str_at("path") == "lora.channel_num"); CHECK(v.int_at("value") == 21);
}

static void draft_only_conversation_regression() {
    viewmodel::BaseViewModel vm;vm.set_command_sink([](const std::string&){return true;});
    vm.set_channels({{1,"Draft group",true,"SECONDARY",0,"draft-only"}});
    json::Value state;CHECK(json::parse(R"({"threads":{"ch:draft-only":{"draft":"Unsent only","pinned":true},"dm:":{"draft":"invalid"},"x":{"draft":"invalid"}},"history_enabled":true})",state));
    vm.apply_threads(state);CHECK(vm.conversation_keys().size()==1);
    app::AssetManager assets;screen::MessagesScreen screen(vm,assets);lv_screen_load(screen.root());
    CHECK(labels(screen.root()).find("Draft group")!=std::string::npos);CHECK(labels(screen.root()).find("Unsent only")!=std::string::npos);
    screen.on_down();screen.on_enter();screen.on_enter();auto* input=textarea(screen.root());CHECK(input);CHECK(std::string(lv_textarea_get_text(input))=="Unsent only");
    screen.on_back();screen.on_back();vm.save_draft("ch:draft-only","");vm.flush_drafts();vm.clear_thread("ch:draft-only");
    CHECK(labels(screen.root()).find("Draft group")!=std::string::npos);vm.delete_thread("ch:draft-only");CHECK(labels(screen.root()).find("Draft group")==std::string::npos);
}

static void identity_confirmation_regression() {
    viewmodel::BaseViewModel vm; std::string sent;
    vm.set_command_sink([&](const std::string& value) {sent=value;return true;});
    vm.set_connected(true);vm.apply_status("online","US","Synthetic");
    vm.set_channels({{1,"Original",true,"SECONDARY",0,"old-id"}});
    vm.request_channel_share(1);const auto request=vm.share_request_id();
    vm.set_channels({{1,"Replacement",true,"SECONDARY",0,"new-id"}});
    vm.apply_channel_share(1,"Original","synthetic-url",request,"old-id");CHECK(vm.channel_share_url().empty());
    vm.set_channels({{1,"Original",true,"SECONDARY",0,"old-id"}});
    vm.request_channel_share(1);vm.apply_channel_share(1,"Original","synthetic-url",vm.share_request_id(),"old-id");
    CHECK(vm.channel_share_url()=="synthetic-url");
    vm.set_channels({{2,"Original",true,"SECONDARY",0,"old-id"}});CHECK(vm.channel_share_url().empty());
    viewmodel::MessageInfo m;m.id="synthetic-retry";m.from="self";m.to="broadcast";m.text="Synthetic";m.channel=1;m.channelIdentity="old-id";m.outgoing=true;
    vm.set_messages({m});vm.resend_message(m.id);json::Value event;CHECK(json::parse(sent,event));CHECK(event.int_at("channel")==2);CHECK(event.str_at("channel_id")=="old-id");
    vm.set_channels({{0,"Original",false,"PRIMARY",0,"public-old"}});vm.apply_config("LONG_FAST",3,false,false);
    app::AssetManager assets;screen::SettingsScreen settings(vm,assets);lv_screen_load(settings.root());
    select_setting(settings, "Share position");settings.on_enter(); // position channel picker
    vm.set_channels({{0,"Replacement",false,"PRIMARY",0,"public-new"}});
    const auto before=sent;settings.on_enter();CHECK(sent==before);CHECK(vm.last_notice().find("Channel changed")!=std::string::npos);
    settings.on_enter();settings.on_enter(); // choose current public channel -> confirmation
    vm.set_channels({{0,"Another",false,"PRIMARY",0,"public-third"}});
    settings.on_down();settings.on_enter();CHECK(sent==before);CHECK(vm.last_notice().find("Channel changed")!=std::string::npos);
}

static void save_png(const char* path, lv_obj_t* root) {
    lv_obj_update_layout(root);
    auto* shot = lv_snapshot_take(root, LV_COLOR_FORMAT_ARGB8888); CHECK(shot);
    png_image image{}; image.version = PNG_IMAGE_VERSION;
    image.width = shot->header.w; image.height = shot->header.h; image.format = PNG_FORMAT_BGRA;
    CHECK(png_image_write_to_file(&image, path, false, shot->data, shot->header.stride, nullptr));
    lv_draw_buf_destroy(shot);
}

static void compact_layout_regression(const char* screenshot_path) {
    viewmodel::BaseViewModel vm;
    vm.set_command_sink([](const std::string&) { return true; });
    vm.set_connected(true); vm.apply_status("online", "US", "Synthetic");
    vm.apply_health("transmit", "");
    vm.set_channels({{1,"Field Alpha",true,"SECONDARY",0,"layout-channel"}});
    vm.apply_config("MEDIUM_FAST",3,false,false,"",45,"DISABLED");
    app::AssetManager assets;
    screen::SettingsScreen settings(vm,assets); lv_screen_load(settings.root());
    const char* order[] = {"Radio", "Transmit mode", "Auto-start radio", "Region", "Modem preset", "Frequency slot", "Hop limit",
        "Mesh profiles", "Channel manager", "Add a channel", "Channels", "Node name", "GPS receiver", "Share position", "Internet (MQTT)",
        "Quick messages", "Message acks", "Signal on messages", "Save conversations", "Keep per chat", "Mute all", "Quiet hours",
        "Theme", "First-run setup", "Diagnostics", "About"};
    for (const auto* name : order) { CHECK(selected_setting(settings.root()) == name); settings.on_down(); }
    CHECK(selected_setting(settings.root()) == "About");
    settings.on_down(); CHECK(selected_setting(settings.root()) == "About");
    for (int i=0;i<30;++i) settings.on_up(); CHECK(selected_setting(settings.root()) == "Radio");
    select_setting(settings,"About");settings.on_enter();lv_obj_update_layout(settings.root());
    auto* version=label_starting(settings.root(),"Version:  ");CHECK(version);
    auto* info=lv_obj_get_parent(version);CHECK(lv_obj_get_scroll_bottom(info)>0);
    settings.on_down();CHECK(lv_obj_get_scroll_top(info)>0);
    settings.on_up();CHECK(lv_obj_get_scroll_top(info)==0);
    CHECK(settings.on_back());select_setting(settings,"Radio");
    if (screenshot_path) {
        const auto dir=std::filesystem::path(screenshot_path).parent_path();
        save_png((dir/"settings-radio.png").c_str(),settings.root());
        for (const auto* name : {"Mesh profiles","GPS receiver","Quick messages","Mute all","Theme"}) {
            select_setting(settings,name);save_png((dir/(std::string("settings-")+name+".png")).c_str(),settings.root());
        }
    }
    viewmodel::NodeInfo peer;peer.id="!12345678";peer.name=std::string(80,'P');vm.set_nodes({peer});
    vm.set_dm_target(peer.id,peer.name);
    viewmodel::MessageInfo message;message.id="layout-message";message.from=peer.id;message.to="!87654321";message.text="Synthetic layout check";message.channel=1;message.channelIdentity="layout-channel";
    vm.set_messages({message});
    screen::MessagesScreen messages(vm,assets);lv_screen_load(messages.root());messages.on_enter(); // choose DM channel
    lv_obj_update_layout(messages.root());
    auto* context=label_starting(messages.root(),"DM via 1: Field Alpha [Private]");CHECK(context);
    auto* help=label_starting(messages.root(),"Enter: write");CHECK(help);
    const auto check_help = [&](lv_obj_t* label) {
        CHECK(label);
        lv_obj_update_layout(messages.root());lv_area_t area;lv_obj_get_coords(label,&area);
        lv_point_t size;lv_text_get_size(&size,lv_label_get_text(label),lv_obj_get_style_text_font(label,LV_PART_MAIN),0,0,LV_COORD_MAX,LV_TEXT_FLAG_NONE);
        CHECK(size.x<=lv_obj_get_width(label));CHECK(area.x1>=0 && area.x2<320);CHECK(area.y1>=148 && area.y2<170);
    };
    check_help(help);lv_area_t context_before;lv_obj_get_coords(context,&context_before);CHECK(context_before.y1>=30 && context_before.y2<51);
    if (screenshot_path) save_png((std::filesystem::path(screenshot_path).parent_path()/"dm-header-help.png").c_str(),messages.root());
    messages.on_up();check_help(label_starting(messages.root(),"Enter: actions"));
    if (screenshot_path) {
        const auto dir = std::filesystem::path(screenshot_path).parent_path();
        save_png((dir/"message-selected-light.png").c_str(),messages.root());
        vm.set_dark_mode(true);
        {
            screen::MessagesScreen dark_messages(vm,assets);
            dark_messages.on_enter(); dark_messages.on_up();
            save_png((dir/"message-selected-dark.png").c_str(),dark_messages.root());
        }
        vm.set_dark_mode(false);
    }
    lv_area_t context_after;lv_obj_get_coords(context,&context_after);CHECK(context_after.y1==context_before.y1);
    messages.on_back();messages.on_enter();CHECK(messages.is_composing());CHECK(!lv_obj_has_flag(context,LV_OBJ_FLAG_HIDDEN));
    if (screenshot_path) save_png((std::filesystem::path(screenshot_path).parent_path()/"dm-header-compose.png").c_str(),messages.root());
    vm.set_channels({{1,"Replacement",false,"PRIMARY",0,"replacement-channel"}});
    CHECK(labels(messages.root()).find("DM via choose channel")!=std::string::npos);
    messages.on_back();messages.on_back();lv_obj_update_layout(messages.root());
    CHECK(lv_obj_has_flag(context,LV_OBJ_FLAG_HIDDEN));
    check_help(label_starting(messages.root(),"/: Search"));
}

static void qr_scan_ui_regression(const char* screenshot_path) {
    viewmodel::BaseViewModel vm;
    std::vector<json::Value> sent;
    int scans = 0;
    bool accept = true;
    vm.set_command_sink([&](const std::string &line) {
        json::Value v;
        CHECK(json::parse(line, v));
        sent.push_back(v);
        if (v.str_at("intent") == "channel.scan")
            ++scans;
        return accept;
    });
    vm.set_connected(true);
    vm.apply_status("off", "US", "Synthetic");
    json::Value ready;
    CHECK(json::parse(R"({"event":"setup","ready":true})", ready));
    vm.apply_setup(ready);
    app::AssetManager assets;
    screen::SettingsScreen settings(vm, assets);
    lv_screen_load(settings.root());
    const auto result = [&](const std::string &id, const std::string &fields) {
        json::Value v;
        CHECK(json::parse("{\"id\":\"" + id + "\"," + fields + "}", v));
        vm.apply_service(v);
    };
    const auto last = [&](const std::string &intent) {
        for (auto it = sent.rbegin(); it != sent.rend(); ++it)
            if (it->str_at("intent") == intent)
                return *it;
        CHECK(false);
        return json::Value{};
    };
    const auto context = [&] {
        result(
            last("channel.setup.context").str_at("id"),
            R"("event":"channel.setup.context","base":"synthetic","tx":false,"tuning":{"region":"US","preset":"LONG_FAST","slot":20,"hop":3,"use_preset":true})");
    };
    const auto option = [&](const std::string &name) {
        const auto selected = [&](auto &&self, lv_obj_t *o) -> std::string {
            if (lv_obj_check_type(o, &lv_label_class) &&
                lv_obj_get_style_bg_opa(o, LV_PART_MAIN) == LV_OPA_COVER &&
                lv_obj_get_style_radius(o, LV_PART_MAIN) == 6)
                return lv_label_get_text(o);
            for (uint32_t i = lv_obj_get_child_count(o); i > 0; --i) {
                auto x = self(self, lv_obj_get_child(o, i - 1));
                if (!x.empty())
                    return x;
            }
            return "";
        };
        for (int i = 0; i < 35; ++i)
            settings.on_up();
        for (int i = 0; i < 40; ++i) {
            if (selected(selected, settings.root()).rfind(name, 0) == 0) {
                settings.on_enter();
                return;
            }
            settings.on_down();
        }
        CHECK(false);
    };
    const auto capture = [&](const char *name) {
        if (screenshot_path)
            save_png((std::filesystem::path(screenshot_path).parent_path() / name).c_str(), settings.root());
    };
    const auto open_scan = [&] {
        select_setting(settings, "Add a channel");
        settings.on_enter();
        context();
        settings.on_enter();
        CHECK(labels(settings.root()).find("Start scan (Return)") != std::string::npos);
        CHECK(labels(settings.root()).find("simulator") == std::string::npos);
    };
    const auto start = [&] {
        settings.on_enter();
        CHECK(labels(settings.root()).find("Scanning QR code...") != std::string::npos);
        return last("channel.scan").str_at("id");
    };
    const auto discard = [&] {
        option("Cancel setup");
        settings.on_down();
        settings.on_enter();
    };
    const std::string preview =
        R"("event":"channel.preview","new":1,"token":"synthetic-token","channels":[{"name":"Synthetic","private":true,"existing":false}])";
    open_scan();
    CHECK(scans == 0);
    capture("qr-scan-ready.png");
    const auto first = start();
    CHECK(scans == 1);
    CHECK(last("channel.scan").find("setup")->as_bool());
    capture("qr-scanning.png");
    // Scanning while Off never powers the radio or imports automatically.
    for (const auto &req : sent)
        CHECK(req.str_at("intent") != "radio.start");
    vm.apply_setup(ready);
    for (int i = 0; i < 5; ++i)
        settings.on_enter();
    CHECK(scans == 1);
    result("wrong-request", preview);
    CHECK(labels(settings.root()).find("Scanning") != std::string::npos);
    result(first, R"("event":"error","text":"No QR code found. Aim and retry.")");
    CHECK(labels(settings.root()).find("No QR code found") != std::string::npos);
    capture("qr-scan-failed.png");
    option("Add by:");
    settings.on_enter();
    const auto second = start();
    CHECK(first != second);
    result(first, preview);
    CHECK(labels(settings.root()).find("Scanning") != std::string::npos);
    result(second, preview);
    CHECK(labels(settings.root()).find("Synthetic [Private]") != std::string::npos);
    CHECK(labels(settings.root()).find("QR scanned (unsaved)") != std::string::npos);
    capture("channel-import-preview.png");
    CHECK(settings.on_key('1'));
    auto review = last("channel.setup.preview");
    CHECK(review.str_at("import_token") == "synthetic-token");
    result(
        review.str_at("id"),
        R"("event":"channel.setup.preview","token":"plan","channels":[{"name":"Synthetic","private":true}],"warnings":[])");
    // Valid receive-only Save proceeds automatically; there is no second review page.
    capture("channel-import-saving.png");
    auto commit = last("channel.setup.commit");
    CHECK(commit.find("confirmed")->as_bool());
    auto count = sent.size();
    settings.on_enter();
    settings.on_back();
    CHECK(sent.size() == count);
    result(
        commit.str_at("id"),
        R"("event":"channel.setup.saved","verified":"radio","added":1,"channels":[{"index":1,"identity":"synthetic"}])");
    CHECK(labels(settings.root()).find("Channel setup complete") != std::string::npos);
    capture("channel-import-complete.png");
    settings.on_back();
    open_scan();
    auto dismissed = start();
    settings.on_back();
    CHECK(last("channel.import.cancel").str_at("request") == dismissed);
    result(dismissed, preview);
    CHECK(labels(settings.root()).find("Synthetic [Private]") == std::string::npos);
    discard();
    open_scan();
    auto duplicate = start();
    result(duplicate, R"("event":"channel.preview","new":0,"token":"","channels":[])");
    CHECK(labels(settings.root()).find("Already present") != std::string::npos);
    discard();
    open_scan();
    accept = false;
    settings.on_enter();
    CHECK(labels(settings.root()).find("Supervisor unavailable") != std::string::npos);
    accept = true;
    option("Add by:");
    settings.on_enter();
    auto disconnected = start();
    vm.set_connected(false);
    CHECK(labels(settings.root()).find("Connection lost") != std::string::npos);
    result(disconnected, preview);
    CHECK(labels(settings.root()).find("Synthetic [Private]") == std::string::npos);
    vm.set_connected(true);
    discard();
    open_scan();
    auto after = start();
    result(after, R"("event":"channel.preview","new":0,"token":"","channels":[])");
    discard();
    select_setting(settings, "Mesh profiles");
    settings.on_enter();
    json::Value error;
    CHECK(
        json::parse(R"({"event":"error","intent":"profile.list","text":"Invalid profile request"})", error));
    vm.apply_service(error);
    CHECK(labels(settings.root()).find("Invalid profile request") != std::string::npos);
    settings.on_back();
    select_setting(settings, "Add a channel");
    settings.on_enter();
    context();
    settings.on_down();
    settings.on_enter();
    CHECK(textarea(settings.root()));
    CHECK(lv_textarea_get_password_mode(textarea(settings.root())));
    lv_textarea_set_text(textarea(settings.root()), "https://meshtastic.org/e/#synthetic");
    settings.on_enter();
    auto pasted = last("channel.preview");
    CHECK(!pasted.str_at("url").empty());
    result(pasted.str_at("id"), preview);
    discard();
    CHECK(last("channel.import.cancel").str_at("request") == pasted.str_at("id"));
    std::string destroyed;
    {
        screen::SettingsScreen temporary(vm, assets);
        select_setting(temporary, "Add a channel");
        temporary.on_enter();
        context();
        temporary.on_enter();
        temporary.on_enter();
        destroyed = last("channel.scan").str_at("id");
    }
    CHECK(last("channel.import.cancel").str_at("request") == destroyed);
    result(destroyed, preview);
    lv_screen_load(settings.root());
}

static void services_ui_regression(const char* screenshot_path) {
    viewmodel::BaseViewModel vm;
    std::string sent;
    vm.set_command_sink([&](const std::string& value) { sent = value; return true; });
    vm.set_connected(true);
    app::AssetManager assets;
    screen::SettingsScreen settings(vm, assets); lv_screen_load(settings.root());
    json::Value event;
    CHECK(json::parse(R"({"event":"setup","ready":false,"mode":"not configured"})", event));
    vm.apply_setup(event);
    const auto capture = [&](const char* name, lv_obj_t* root) { if (screenshot_path) save_png((std::filesystem::path(screenshot_path).parent_path()/name).c_str(), root); };
    capture("first-run-region.png", settings.root());
    CHECK(labels(settings.root()).find("First run") != std::string::npos);
    settings.on_enter(); settings.on_enter(); // region US, private
    auto* text = textarea(settings.root()); CHECK(text);
    lv_textarea_set_text(text,"Synthetic"); settings.on_enter();
    CHECK(labels(settings.root()).find("RX only") != std::string::npos);
    capture("first-run-confirm.png", settings.root());
    const auto before = sent; settings.on_enter(); CHECK(sent == before); // Cancel
    select_setting(settings, "First-run setup");
    settings.on_enter(); CHECK(json::parse(sent,event)); CHECK(event.str_at("intent")=="setup.status");
    const auto request=event.str_at("id");
    CHECK(json::parse("{\"event\":\"setup\",\"id\":\""+request+"\",\"ready\":true,\"mode\":\"receive-only\",\"region\":\"US\",\"firewall\":true}",event));
    vm.apply_setup(event); CHECK(labels(settings.root()).find("Setup ready")!=std::string::npos);
    settings.on_back(); select_setting(settings, "Transmit mode"); settings.on_enter(); // TX
    CHECK(labels(settings.root()).find("Antenna")!=std::string::npos);
    settings.on_enter(); CHECK(sent.find("setup.status")!=std::string::npos); // Cancel stays off
    settings.on_enter(); settings.on_down(); settings.on_enter();
    CHECK(json::parse(sent,event)); CHECK(event.str_at("intent")=="radio.tx"); CHECK(event.find("antenna_confirmed")->as_bool());
    settings.on_back(); // dismiss waiting; stale response may not replace new overlay
    select_setting(settings, "Channel manager"); settings.on_enter(); CHECK(json::parse(sent,event)); CHECK(event.str_at("intent")=="channel.slots");
    auto slot_request=event.str_at("id");
    CHECK(json::parse("{\"event\":\"channel.slots\",\"id\":\""+slot_request+"\",\"channels\":[{\"index\":1,\"identity\":\"synthetic-id\",\"name\":\"Synthetic\",\"private\":true,\"role\":\"SECONDARY\"}]}",event));
    vm.apply_service(event);
    capture("channel-manager.png", settings.root());
    settings.on_down();
    settings.on_enter(); // existing slot
    capture("channel-edit.png", settings.root());
    settings.on_down(); settings.on_down(); settings.on_enter(); // rotate
    settings.on_down(); settings.on_enter();
    CHECK(json::parse(sent,event)); CHECK(event.str_at("intent")=="channel.manage"); CHECK(event.str_at("channel_id")=="synthetic-id"); CHECK(event.str_at("action")=="rotate");
    settings.on_back();
    vm.set_channels({{1,"Original",true,"SECONDARY",0,"group-old"}});
    viewmodel::MessageInfo m; m.from="self"; m.id="synthetic"; m.to="broadcast"; m.outgoing=true; m.channel=1; m.channelIdentity="group-old"; m.channelName="Original"; m.channelPrivate=true; m.text="Needle";
    vm.set_messages({m}); vm.save_draft("ch:group-old","Unsent secret"); vm.flush_drafts();
    vm.set_channels({{1,"Replacement",false,"SECONDARY",0,"group-new"}});
    CHECK(vm.channel_for_thread("ch:group-old")==-1);
    const auto before_retry=sent; vm.resend_message(m.id); CHECK(sent==before_retry);
    screen::MessagesScreen messages(vm,assets); lv_screen_load(messages.root());
    messages.on_down(); messages.on_enter();
    CHECK(labels(messages.root()).find("Archived: Original")!=std::string::npos);
    capture("archived-channel.png", messages.root());
    const auto before_compose=sent; messages.on_enter(); CHECK(sent==before_compose);
    CHECK(messages.on_back());
    CHECK(messages.on_key('/')); text=textarea(messages.root()); CHECK(text);
    lv_textarea_set_text(text,"absent"); messages.on_enter(); CHECK(labels(messages.root()).find("Archived: Original")==std::string::npos);
    CHECK(messages.on_back()); CHECK(labels(messages.root()).find("Archived: Original")!=std::string::npos);
    vm.toggle_pin("ch:group-old"); CHECK(json::parse(sent,event)); CHECK(event.str_at("intent")=="thread.state"); CHECK(vm.pinned("ch:group-old"));
    vm.set_quiet_hours(0,1439); CHECK(vm.quiet_start()==0); vm.set_quiet_hours(-1,-1); CHECK(!vm.quiet_now());
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
    maintenance_data_regression();
    unchanged_thread_regression();
    dual_channel_ui_regression(argc > 1 ? argv[1] : nullptr);
    services_ui_regression(argc > 1 ? argv[1] : nullptr);
    qr_scan_ui_regression(argc > 1 ? argv[1] : nullptr);
    identity_confirmation_regression();
    draft_only_conversation_regression();
    compact_layout_regression(argc > 1 ? argv[1] : nullptr);
    {
        viewmodel::BaseViewModel vm;
        std::string sent;
        vm.set_command_sink([&](const std::string& s) { sent = s; return true; });
        vm.set_position_sharing(false); CHECK(std::string(vm.config_state()) == "unknown");
        CHECK(!lv_subject_get_int(vm.position_subject()));
        vm.set_connected(true); CHECK(sent.find("message.retention") != std::string::npos);
        json::Value retention_request;
        CHECK(json::parse(sent, retention_request));
        CHECK(retention_request.find("silent") && retention_request.find("silent")->as_bool());
        vm.set_retention_cap(100);
        CHECK(json::parse(sent, retention_request));
        CHECK(retention_request.int_at("cap") == 100);
        CHECK(!retention_request.find("silent") || !retention_request.find("silent")->as_bool());
        vm.set_connected(false); vm.set_connected(true);
        CHECK(json::parse(sent, retention_request));
        CHECK(retention_request.int_at("cap") == 100 && retention_request.find("silent") && retention_request.find("silent")->as_bool());
        vm.set_retention_cap(0);
        vm.apply_status("online", "UNSET", "Demo");
        vm.set_channels({{0, "Public", false, "PRIMARY", 0, "synthetic-public"}, {1, "Field One", true, "SECONDARY", 0, "synthetic-private"}, {2, "Field Two", true, "SECONDARY", 0, "synthetic-other"}});
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
        m.to = "broadcast"; m.channel = 2; m.channelIdentity = "synthetic-other"; m.time = 1; m.text = "Radio check. All clear.";
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
        screen.on_key('o'); screen.on_down(); screen.on_down(); screen.on_down(); screen.on_enter();
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
