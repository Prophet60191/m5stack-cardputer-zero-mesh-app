// SPDX-License-Identifier: MIT
// Real LVGL, synthetic history, no socket or RF.
#include "asset_manager.h"
#include "base_viewmodel.h"
#include "messages_screen.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <filesystem>
#include <fstream>
#define CHECK(x) do {if(!(x)){std::cerr<<"FAIL "<<__LINE__<<": "<<#x<<'\n';std::abort();}}while(0)
static std::string shots;
static void capture(lv_obj_t* root,const std::string& name,bool dark) {
    if(shots.empty())return;
    std::filesystem::create_directories(shots);
    lv_obj_update_layout(root);
    auto* image=lv_snapshot_take(root,LV_COLOR_FORMAT_ARGB8888);CHECK(image);
    std::ofstream out(std::filesystem::path(shots)/(name+(dark?"-dark":"-light")+".ppm"),std::ios::binary);
    out<<"P6\n"<<image->header.w<<" "<<image->header.h<<"\n255\n";
    for(unsigned y=0;y<image->header.h;++y)for(unsigned x=0;x<image->header.w;++x){
        const auto* pixel=image->data+y*image->header.stride+x*4;
        const char rgb[]={static_cast<char>(pixel[2]),static_cast<char>(pixel[1]),static_cast<char>(pixel[0])};out.write(rgb,3);
    }
    lv_draw_buf_destroy(image);
}
static lv_obj_t* find(lv_obj_t* root,const std::string& text) {
    if(lv_obj_check_type(root,&lv_label_class) && text==lv_label_get_text(root))return root;
    for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto* p=find(lv_obj_get_child(root,i),text))return p;
    return nullptr;
}
static lv_obj_t* input(lv_obj_t* root) {
    if(lv_obj_check_type(root,&lv_textarea_class))return root;
    for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)if(auto* p=input(lv_obj_get_child(root,i)))return p;
    return nullptr;
}
static size_t objects(lv_obj_t* root) {
    size_t n=1;for(uint32_t i=0;i<lv_obj_get_child_count(root);++i)n+=objects(lv_obj_get_child(root,i));return n;
}
static bool selected(lv_obj_t* root,const std::string& text) {
    auto* p=find(root,text);return p && lv_obj_get_style_border_width(lv_obj_get_parent(p),LV_PART_MAIN)==2;
}
static void search(screen::MessagesScreen& s,const char* text) {
    CHECK(s.on_key('/'));lv_textarea_set_text(input(s.root()),text);s.on_enter();
}
static viewmodel::MessageInfo message(const std::string& id,const std::string& text,long t,bool dm=false) {
    viewmodel::MessageInfo m;m.id=id;m.text=text;m.time=t;m.from="!10000001";
    m.to=dm?"self":"broadcast";m.channelIdentity="field";m.channelName="Field";m.channel=1;return m;
}
static void run(bool dark) {
    viewmodel::BaseViewModel vm;app::AssetManager assets;vm.set_dark_mode(dark);
    vm.set_command_sink([](const std::string&){return true;});
    vm.set_channels({{1,"Field",true,"PRIMARY",0,"field"},{2,"Second",true,"SECONDARY",0,"second"}});
    viewmodel::NodeInfo n;n.id="!10000001";n.name="Peer";vm.set_nodes({n});
    auto old=message("group-old","Needle older",1),dm=message("dm","Needle direct",3,true),recent=message("group-new","Needle latest",4);
    vm.set_messages({old,dm,recent});vm.toggle_pin("dm:!10000001");
    CHECK(vm.conversation_rows()[0].key=="ch:field"); // recency wins over pin
    auto results=vm.search_messages("nEeDlE");CHECK(results.size()==3);
    CHECK(results[0].message_key==recent.from+"\n"+recent.id);CHECK(results[1].key=="dm:!10000001");
    CHECK(vm.search_messages("").empty());CHECK(vm.search_messages("not present").empty());
    screen::MessagesScreen s(vm,assets);lv_screen_load(s.root());
    CHECK(find(s.root(),"Channels"));CHECK(find(s.root(),"Direct Messages"));
    CHECK(find(s.root(),"# 1: Field [Private]"));CHECK(!find(s.root(),"* Peer"));capture(s.root(),"channels",dark);
    CHECK(s.on_key(LV_KEY_RIGHT));CHECK(s.on_key(LV_KEY_RIGHT));CHECK(find(s.root(),"* Peer"));CHECK(!find(s.root(),"# 1: Field [Private]"));
    capture(s.root(),"direct-messages",dark);
    CHECK(s.on_key(LV_KEY_LEFT));CHECK(find(s.root(),"# 1: Field [Private]"));CHECK(s.on_key(LV_KEY_RIGHT));
    search(s,"needle");capture(s.root(),"combined-search",dark);CHECK(find(s.root(),"Channel: # 1: Field [Private]"));CHECK(find(s.root(),"DM: * Peer"));
    s.on_enter();CHECK(selected(s.root(),"Needle latest"));capture(s.root(),"search-match",dark);CHECK(s.on_back());CHECK(find(s.root(),"Search: needle"));
    s.on_down();s.on_enter();CHECK(selected(s.root(),"Needle direct")); // no send-channel picker for reading
    CHECK(s.on_back());s.on_down();s.on_enter();CHECK(selected(s.root(),"Needle older"));CHECK(s.on_back());
    CHECK(s.on_back());CHECK(find(s.root(),"* Peer")); // returns to the DM tab selected on entry
    search(s,"not present");s.on_up();s.on_down();s.on_enter();CHECK(find(s.root(),"No messages match this search."));CHECK(s.on_back());
    std::vector<viewmodel::MessageInfo> history;
    for(int i=0;i<2000;++i)history.push_back(message(std::to_string(i),"Synthetic "+std::to_string(i),i));
    vm.set_messages(history);CHECK(s.on_key(LV_KEY_NEXT));
    auto start=std::chrono::steady_clock::now();s.on_down();s.on_enter();
    auto us=std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now()-start).count();
    CHECK(find(s.root(),"Synthetic 1999"));CHECK(!find(s.root(),"Synthetic 0"));CHECK(objects(s.root())<400);
    s.on_up();for(int i=0;i<50;++i)s.on_up();CHECK(selected(s.root(),"Synthetic 1949"));
    for(int i=0;i<51;++i)s.on_down();CHECK(!selected(s.root(),"Synthetic 1999")); // compose row again
    CHECK(s.on_back());search(s,"Synthetic 0");s.on_enter();CHECK(selected(s.root(),"Synthetic 0"));CHECK(objects(s.root())<400);CHECK(s.on_back());
    search(s,"Synthetic");CHECK(objects(s.root())<230); // 2,000 results, bounded LVGL objects
    for(int i=0;i<55;++i)s.on_down();s.on_enter();CHECK(selected(s.root(),"Synthetic 1944"));CHECK(s.on_back());
    auto removed=history;removed.erase(removed.begin()+1944);vm.set_messages(removed);s.on_enter();CHECK(selected(s.root(),"Synthetic 1943"));CHECK(s.on_back());
    CHECK(vm.messages().size()==1999);
    std::cout<<"dark="<<dark<<" open_2000_message_chat_us="<<us<<" history_preserved=1999\n";
}
int main(int argc,char** argv){if(argc>1)shots=argv[1];lv_init();auto* d=lv_display_create(320,170);auto* g=lv_group_create();lv_group_set_default(g);for(bool dark:{false,true})run(dark);lv_group_delete(g);lv_display_delete(d);}
