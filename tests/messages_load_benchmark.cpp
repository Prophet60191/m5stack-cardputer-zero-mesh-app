// SPDX-License-Identifier: MIT
// Synthetic headless load timing. Does not connect to a supervisor or any device.
#include "asset_manager.h"
#include "base_viewmodel.h"
#include "messages_screen.h"
#include <chrono>
#include <cstdio>
#include <cstdlib>
static unsigned objects(lv_obj_t* o) {
    unsigned n=1;for(uint32_t i=0;i<lv_obj_get_child_count(o);++i)n+=objects(lv_obj_get_child(o,i));return n;
}
int main(int argc,char** argv){
    std::setvbuf(stdout,nullptr,_IOLBF,0);
    const int count=argc>1?std::atoi(argv[1]):2000;
    if(count<1 || count>2000)return 2;
    lv_init();auto* d=lv_display_create(320,170);
    {
        viewmodel::BaseViewModel vm;app::AssetManager assets;
        vm.set_channels({{1,"Synthetic",true,"PRIMARY",0,"synthetic"}});
        std::vector<viewmodel::MessageInfo> history;
        for(int i=0;i<count;++i){viewmodel::MessageInfo m;m.id=std::to_string(i);m.from="!10000001";
            m.to="broadcast";m.channelIdentity="synthetic";m.text="Synthetic timing message "+m.id;m.time=i;history.push_back(m);}
        vm.set_messages(history);
        const auto start=std::chrono::steady_clock::now();
        screen::MessagesScreen s(vm,assets);s.on_down();s.on_enter();lv_obj_update_layout(s.root());
        const auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
        std::printf("synthetic_open_ms=%.3f lvgl_objects=%u messages_retained=%zu\n",ms,objects(s.root()),vm.messages().size());
    }
    lv_display_delete(d);
}
