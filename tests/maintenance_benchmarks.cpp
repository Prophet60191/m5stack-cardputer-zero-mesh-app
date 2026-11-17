// SPDX-License-Identifier: MIT
// Synthetic, headless measurements only: no socket, camera, framebuffer or RF.
#include "base_viewmodel.h"
#include "asset_manager.h"
#include "messages_screen.h"
#include "json.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <vector>

using Clock = std::chrono::steady_clock;
template<class Fn> static double median_us(Fn fn, int count = 21) {
    std::vector<double> samples;
    for (int i = 0; i < count + 3; ++i) {
        const auto start = Clock::now(); fn();
        const auto elapsed = std::chrono::duration<double, std::micro>(Clock::now()-start).count();
        if (i >= 3) samples.push_back(elapsed);
    }
    std::sort(samples.begin(), samples.end()); return samples[samples.size()/2];
}

int main(int argc, char** argv) {
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    const bool maintenance_only = argc == 2 && std::strcmp(argv[1], "--maintenance") == 0;
    lv_init(); auto* display = lv_display_create(320,170);
    {
        viewmodel::BaseViewModel vm;
        vm.set_command_sink([](const std::string&) { return true; });
        vm.set_connected(true);
        if (!maintenance_only) for (int peers : {32, 256, 2000}) {
            std::vector<viewmodel::MessageInfo> messages;
            for (int i = 0; i < 2000; ++i) {
                viewmodel::MessageInfo m;
                char id[12]; std::snprintf(id,sizeof(id),"!%08x",0x01000000+i%peers);
                m.from=id; m.to="self"; m.id=std::to_string(i); m.time=i+1;
                m.text=i==0 ? "Synthetic needle" : "Synthetic message";
                messages.push_back(std::move(m));
            }
            vm.set_messages(std::move(messages));
            for (const std::string search : {"", "needle"}) {
                size_t rows=0;
                const auto us=median_us([&] { rows=vm.conversation_rows(search).size(); });
                std::printf("conversation_rows messages=2000 peers=%d search=%s rows=%zu median_us=%.3f\n",
                    peers,search.empty()?"none":"needle",rows,us);
            }
        }
        vm.set_channels({{1,"Synthetic",true,"PRIMARY",0,"synthetic-channel"}});
        for (int peers : {1, 32, 256, 2000}) {
            std::vector<viewmodel::MessageInfo> messages;
            for (int i = 0; i < 2000; ++i) {
                viewmodel::MessageInfo m;
                m.id = std::to_string(i);
                m.to = "broadcast";
                m.channelIdentity = "synthetic-" + std::to_string(i % peers);
                m.text = "Synthetic";
                messages.push_back(std::move(m));
            }
            vm.load_messaging_prefs(false, 1, false, true);
            const auto us = median_us([&] { vm.set_messages(messages); }, 7);
            std::printf("retention messages=2000 peers=%d cap=1 retained=%zu median_us=%.3f\n", peers,
                        vm.messages().size(), us);
        }
        vm.load_messaging_prefs(false, 0, false, true);
        for (int peers : {32, 256}) {
            vm.set_channels({});
            std::vector<viewmodel::MessageInfo> messages;
            for (int i = 0; i < 2000; ++i) {
                viewmodel::MessageInfo m;
                char peer[16];
                std::snprintf(peer, sizeof(peer), "!%08x", 0x10000000 + i % peers);
                m.from = peer;
                m.to = "self";
                m.id = std::to_string(i);
                m.text = "Synthetic";
                messages.push_back(m);
            }
            vm.set_messages(messages);
            app::AssetManager assets;
            screen::MessagesScreen screen(vm, assets);
            const auto us = median_us(
                [&] {
                    screen.on_down();
                    screen.on_up();
                },
                7);
            std::printf("conversation_navigation peers=%d roundtrip_median_us=%.3f\n", peers, us);
        }
        vm.set_channels({{1, "Synthetic", true, "PRIMARY", 0, "synthetic-channel"}});
        vm.set_messages({});
        vm.save_draft("ch:synthetic-channel","Synthetic draft"); vm.flush_drafts();
        json::Value event;
        if (!json::parse(R"({"event":"threads","history_enabled":true,"threads":{"ch:synthetic-channel":{"draft":"Synthetic draft","pinned":false}}})",event)) return 2;
        vm.apply_threads(event);
        const auto initial=lv_subject_get_int(vm.messages_version_subject());
        const auto echo_us=median_us([&] {vm.apply_threads(event);},101);
        std::printf("unchanged_thread_echo repeats=104 revisions=%d median_us=%.3f\n",
            lv_subject_get_int(vm.messages_version_subject())-initial,echo_us);
        app::AssetManager assets;
        screen::MessagesScreen screen(vm,assets);
        const auto screen_us=median_us([&] {vm.apply_threads(event);},21);
        std::printf("unchanged_thread_echo_with_screen median_us=%.3f\n",screen_us);
    }
    lv_display_delete(display);
}
