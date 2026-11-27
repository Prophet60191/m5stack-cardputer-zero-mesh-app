// SPDX-License-Identifier: MIT
// Headless LVGL at physical resolution; synthetic data only, no hardware or RF.
#include "asset_manager.h"
#include "base_viewmodel.h"
#include "messages_screen.h"
#include "nodes_screen.h"
#include "settings_screen.h"
#include "map_screen.h"
#include "menu_screen.h"
#include "toast.h"
#include "help_popup.h"
#include "text.h"
#include <png.h>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <functional>

#define CHECK(x) do { if (!(x)) { std::cerr << "FAIL " << __LINE__ << ": " << #x << '\n'; std::abort(); } } while (0)
static bool dark = false;
static lv_obj_t *find(lv_obj_t *o, const std::string &text) {
    if (lv_obj_check_type(o, &lv_label_class) && text == lv_label_get_text(o)) return o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i)
        if (auto *result = find(lv_obj_get_child(o, i), text)) return result;
    return nullptr;
}
static lv_obj_t *find_prefix(lv_obj_t *o, const std::string &prefix) {
    if (lv_obj_check_type(o,&lv_label_class) && std::string(lv_label_get_text(o)).rfind(prefix,0)==0) return o;
    for (uint32_t i=0;i<lv_obj_get_child_count(o);++i)
        if(auto *result=find_prefix(lv_obj_get_child(o,i),prefix))return result;
    return nullptr;
}
static lv_obj_t *input(lv_obj_t *o) {
    if (lv_obj_check_type(o, &lv_textarea_class)) return o;
    for (uint32_t i = 0; i < lv_obj_get_child_count(o); ++i)
        if (auto *result = input(lv_obj_get_child(o, i))) return result;
    return nullptr;
}
static std::string selected(lv_obj_t *o) {
    if (lv_obj_get_style_border_width(o, LV_PART_MAIN) == 3 && lv_obj_get_child_count(o)) {
        auto *first = lv_obj_get_child(o, 0);
        if (lv_obj_check_type(first, &lv_label_class)) return lv_label_get_text(first);
    }
    // The last child is the foreground overlay, if present.
    for (uint32_t i = lv_obj_get_child_count(o); i > 0; --i)
        if (auto result = selected(lv_obj_get_child(o, i - 1)); !result.empty()) return result;
    if (lv_obj_check_type(o, &lv_label_class) &&
        (lv_obj_get_style_border_width(lv_obj_get_parent(o), LV_PART_MAIN) == 3 ||
         (lv_obj_get_style_bg_opa(o, LV_PART_MAIN) == LV_OPA_COVER && lv_obj_get_style_radius(o, LV_PART_MAIN) == 6)))
        return lv_label_get_text(o);
    return "";
}
static void bounds(lv_obj_t *o, int left = 0, int right = 319) {
    CHECK(o); lv_obj_update_layout(o);
    lv_area_t r; lv_obj_get_coords(o, &r);
    CHECK(r.x1 >= left && r.x2 <= right);
}
static void one_line(lv_obj_t *label) {
    bounds(label);
    CHECK(lv_obj_get_height(label) == lv_font_get_line_height(lv_obj_get_style_text_font(label,LV_PART_MAIN)));
}
static void choose(screen::BaseScreen &s, const std::string &name) {
    for (int i = 0; i < 80; ++i) {
        if (selected(s.root()) == name) { s.on_enter(); return; }
        s.on_down();
    }
    std::cerr << "Missing action " << name << "; selected=" << selected(s.root()) << "\n";
    CHECK(false);
}
static void screenshot(lv_obj_t *root, const std::string &dir, const std::string &name) {
    if (dir.empty()) return;
    std::filesystem::create_directories(dir);
    lv_obj_update_layout(root);
    auto *shot = lv_snapshot_take(root, LV_COLOR_FORMAT_ARGB8888); CHECK(shot);
    png_image im{}; im.version=PNG_IMAGE_VERSION; im.width=shot->header.w;
    im.height=shot->header.h; im.format=PNG_FORMAT_BGRA;
    CHECK(png_image_write_to_file(&im,(std::filesystem::path(dir)/name).c_str(),false,
                                  shot->data,shot->header.stride,nullptr));
    lv_draw_buf_destroy(shot);
}
static void channel(viewmodel::BaseViewModel &vm) {
    vm.set_channels({{1,"Synthetic",true,"PRIMARY",0,"synthetic"}});
    vm.remember_conversation("ch:synthetic");
}
static void wrap() {
    viewmodel::BaseViewModel vm;vm.set_dark_mode(dark); app::AssetManager assets; channel(vm);
    viewmodel::NodeInfo a,b; a.id="!10000001"; a.name="Alpha"; b.id="!10000002"; b.name="Beta";
    a.lastHeard=2; b.lastHeard=1; vm.set_nodes({a,b});
    { screen::NodesScreen s(vm,assets);
      CHECK(selected(s.root())=="Alpha"); s.on_up(); CHECK(selected(s.root())=="Beta");
      s.on_down(); CHECK(selected(s.root())=="Alpha");
      s.on_key(LV_KEY_PREV); CHECK(find(s.root(),"Active   \xE2\x87\xA5 filter   s \xE2\x98\x85 fav   m msg")); }
    { screen::MessagesScreen s(vm,assets); CHECK(selected(s.root())=="+  New message");
      s.on_up(); CHECK(selected(s.root())=="# 1: Synthetic [Private]");
      s.on_down(); CHECK(selected(s.root())=="+  New message");
      s.on_enter(); s.on_up(); CHECK(selected(s.root())=="Beta");
      s.on_down(); CHECK(selected(s.root())=="# 1: Synthetic [Private]"); }
}
static void cancel_recipient() {
    viewmodel::BaseViewModel vm;vm.set_dark_mode(dark);app::AssetManager assets;channel(vm);
    viewmodel::NodeInfo a,b;a.id="!10000001";a.name="Alpha";b.id="!10000002";b.name="Beta";
    vm.set_nodes({a,b});vm.remember_conversation("dm:"+a.id);
    screen::MessagesScreen s(vm,assets);s.on_enter();s.on_down();s.on_down();
    CHECK(s.on_back());CHECK(selected(s.root())=="+  New message");
    s.on_enter();CHECK(find(s.root(),"New message to:"));CHECK(s.on_back());
}
static void long_labels(const std::string &dir) {
    viewmodel::BaseViewModel vm;vm.set_dark_mode(dark); app::AssetManager assets; channel(vm);
    viewmodel::NodeInfo n; n.id="!10000001"; n.name=std::string(70,'W');
    n.shortName="WWWW"; n.lastHeard=1; n.hasPos=true; n.lat=12; n.lon=20;
    vm.set_nodes({n});
    { screen::NodesScreen s(vm,assets); one_line(find_prefix(s.root(),std::string(10,'W')));
      screenshot(s.root(),dir,"nodes-long.png"); s.on_enter();
      bounds(find(s.root(),n.name)); screenshot(s.root(),dir,"node-detail-long.png"); }
    viewmodel::MessageInfo m; m.id="synthetic-message";m.from=n.id;m.fromName=n.name;
    m.to="broadcast";m.text="Synthetic text";m.channel=1;m.channelIdentity="synthetic"; vm.set_messages({m});
    { screen::MessagesScreen s(vm,assets); s.on_down(); s.on_enter();
      bounds(find(s.root(),n.name)); screenshot(s.root(),dir,"sender-long.png"); }
    vm.set_messages({});vm.remember_conversation("dm:"+n.id);
    { screen::MessagesScreen s(vm,assets); s.on_key(LV_KEY_NEXT); one_line(find_prefix(s.root(),std::string(10,'W')));
      screenshot(s.root(),dir,"conversations-long.png"); }
}
static void invalid_slot() {
    viewmodel::BaseViewModel vm;vm.set_dark_mode(dark); app::AssetManager assets; int writes=0;
    vm.set_command_sink([&](const std::string &s){ if(s.find("config.set")!=std::string::npos)++writes;return true;});
    vm.set_connected(true);vm.apply_status("online","US","Synthetic");vm.apply_config("LONG_FAST",3,false,false);
    screen::SettingsScreen s(vm,assets); choose(s,"Frequency slot");CHECK(s.is_composing());
    auto *box=input(s.root()); CHECK(box);lv_textarea_set_text(box,"999");s.on_enter();
    CHECK(s.is_composing());CHECK(std::string(lv_textarea_get_text(input(s.root())))=="999");CHECK(writes==0);
    lv_textarea_set_text(input(s.root()),"20");s.on_enter();CHECK(!s.is_composing());CHECK(writes==1);
}
static void quote_without_channel() {
    viewmodel::BaseViewModel vm;vm.set_dark_mode(dark);app::AssetManager assets;
    viewmodel::MessageInfo m;m.id="synthetic";m.from="!10000001";m.to="self";m.text="Original text";
    vm.set_messages({m});vm.set_dm_target(m.from,"Synthetic peer");
    screen::MessagesScreen s(vm,assets);CHECK(s.on_back()); // no-channel info
    s.on_up();s.on_enter();choose(s,"Quote / edit");
    CHECK(!s.is_composing());CHECK(std::string(lv_textarea_get_text(input(s.root()))).empty());
}
static void toasts(const std::string &dir) {
    viewmodel::BaseViewModel vm;vm.set_dark_mode(dark);app::AssetManager assets;
    auto *root=lv_obj_create(nullptr);lv_obj_set_size(root,320,170);
    {
        view::widgets::Toast t(root,vm,assets); t.show("Literal #ff0000 text#");
        auto *l=find(t.root(),"Literal #ff0000 text#");CHECK(l);CHECK(!lv_label_get_recolor(l));
        t.show(std::string(600,'W'));lv_obj_update_layout(root);lv_area_t r;lv_obj_get_coords(t.root(),&r);
        CHECK(r.x1>=0 && r.x2<320 && r.y1>=30 && r.y2<170);
        screenshot(root,dir,"toast-long.png");
        t.show_highlighted("Literal #123456 ","#ff0000 value#"," # suffix");
        CHECK(!lv_label_get_recolor(l));
    }
    lv_obj_delete(root);
}
static void qr(const std::string &dir) {
    viewmodel::BaseViewModel vm;vm.set_dark_mode(dark);app::AssetManager assets;channel(vm);
    json::Value request;
    vm.set_command_sink([&](const std::string &s){ CHECK(json::parse(s,request));return true;});
    vm.set_connected(true);screen::MessagesScreen s(vm,assets);s.on_down();s.on_enter();
    s.on_key('o');choose(s,"Share channel (QR)");
    CHECK(request.str_at("intent")=="channel.share");
    vm.apply_channel_share(1,"Synthetic", "https://meshtastic.org/e/#Ci0SIAABAgMEBQYHCAkKCwwNDg8QERITFBUWFxgZGhscHR4fGglTeW50aGV0aWM",request.str_at("id"),"synthetic");
    auto *h=find(s.root(),"Scan to join Synthetic");CHECK(h);lv_obj_update_layout(s.root());
    auto *panel=lv_obj_get_parent(h); auto *box=lv_obj_get_child(panel,1);lv_area_t r;
    lv_obj_get_coords(h,&r);CHECK(r.y1>=30);lv_obj_get_coords(box,&r);CHECK(r.y1>=30 && r.y2<170);
    screenshot(s.root(),dir,"share-qr.png");CHECK(s.on_back());
    s.on_key('o');choose(s,"Share channel (QR)");
    vm.apply_channel_share(1,"Synthetic",std::string(6000,'W'),request.str_at("id"),"synthetic");
    CHECK(find(s.root(),"Could not render this share code. Try sharing a single channel."));
    CHECK(s.on_back());
}
static void header() {
    viewmodel::BaseViewModel vm;vm.set_dark_mode(dark);app::AssetManager assets;
    vm.apply_status("starting","EU_868","Synthetic");
    std::vector<viewmodel::NodeInfo> nodes(256);
    for(size_t i=0;i<nodes.size();++i)nodes[i].id="synthetic-"+std::to_string(i);
    vm.set_nodes(nodes);
    for(int i=0;i<2000;++i) { viewmodel::MessageInfo m;m.id=std::to_string(i);m.from="!10000001";
        m.to="broadcast";m.channelIdentity="synthetic";m.text="Synthetic";vm.add_message(m); }
    screen::MenuScreen s(vm,assets);auto *badge=find(s.root(),LV_SYMBOL_ENVELOPE " 2000");CHECK(badge);
    bounds(badge,220,266);
}
static void home_standard(const std::string &dir) {
    viewmodel::BaseViewModel vm;
    vm.set_dark_mode(dark);
    app::AssetManager assets;
    screen::MenuScreen home(vm,assets);
    auto *root=home.root();
    auto *previous=lv_screen_active();
    lv_screen_load(root);
    auto geometry=[](lv_obj_t* obj, int x, int y, int w) {
        CHECK(obj);
        lv_obj_update_layout(obj);
        lv_area_t box;
        lv_obj_get_coords(obj,&box);
        CHECK(box.x1==x && box.y1==y && box.x2==x+w-1);
    };
    auto *header=find(root,"MESH");
    geometry(header,28,5,53);
    CHECK(lv_obj_get_height(lv_obj_get_parent(header))==23);
#if USE_DESKTOP
    auto *battery=find(root,"100%");
    geometry(battery,272,7,40);
    CHECK(lv_obj_get_style_text_align(battery,LV_PART_MAIN)==LV_TEXT_ALIGN_RIGHT);
#endif
    auto *title=find(root,"Messages");
    auto *subtitle=find(root,"Read, write & manage messages");
    geometry(title,52,82,216);
    geometry(subtitle,52,108,216);
    CHECK(lv_obj_get_style_text_font(title,LV_PART_MAIN)==&lv_font_montserrat_16);
    CHECK(lv_obj_get_style_text_font(subtitle,LV_PART_MAIN)==&lv_font_montserrat_10);
    auto *left_icon=find(root,LV_SYMBOL_SETTINGS);
    geometry(left_icon,14,69,37);
    CHECK(lv_obj_get_style_opa(left_icon,LV_PART_MAIN)==LV_OPA_40);
    CHECK(lv_obj_get_style_text_font(left_icon,LV_PART_MAIN)==&lv_font_montserrat_28);
    auto *center_icon=find(root,LV_SYMBOL_ENVELOPE);
    geometry(center_icon,52,33,216);
    CHECK(lv_obj_get_style_text_font(center_icon,LV_PART_MAIN)==&lv_font_montserrat_40);
    // The header also uses the Wi-Fi symbol, so search for its side position.
    bool side=false, divider=false;
    int active_x=-1, dots=0;
    auto check_dots=[&]() {
        dots=0; active_x=-1;
        for(uint32_t i=0;i<lv_obj_get_child_count(root);++i) {
            auto *child=lv_obj_get_child(root,i); lv_area_t box;
            lv_obj_get_coords(child,&box);
            if(box.x1==270 && box.y1==69 && box.x2==306) {
                side=true;
                CHECK(lv_obj_get_style_opa(child,LV_PART_MAIN)==LV_OPA_40);
            }
            if(box.x1==8 && box.y1==146 && box.x2==311 && box.y2==146) divider=true;
            if(box.y1==138 && box.y2==140) {
                ++dots;
                int width=box.x2-box.x1+1;
                CHECK(width==4 || width==8);
                if(width==8) { CHECK(active_x<0); active_x=box.x1; }
            }
        }
        CHECK(dots==4);
    };
    auto *footer=find(root,"Z / C  choose    Enter  open    Fn+H  help");
    geometry(footer,8,153,307);
    CHECK(lv_obj_get_style_text_font(footer,LV_PART_MAIN)==&lv_font_montserrat_10);
    check_dots(); CHECK(active_x==144 && divider && side);
    screenshot(root,dir,"mesh-home-standard.png");
    vm.menu_prev(); // backward wrap into Settings
    CHECK(find(root,"Settings")&&find(root,"Radio, appearance & preferences"));
    lv_tick_inc(200); lv_anim_refr_now(); lv_obj_update_layout(root);
    check_dots(); CHECK(active_x==171);
    screenshot(root,dir,"mesh-home-settings.png");
    vm.set_dark_mode(!dark);
    CHECK(find(root,"Settings"));
    lv_tick_inc(200); lv_anim_refr_now(); lv_obj_update_layout(root);
    check_dots(); CHECK(active_x==171);
    vm.menu_next(); // forward wrap into Messages
    CHECK(find(root,"Messages"));
    lv_tick_inc(200); lv_anim_refr_now(); lv_obj_update_layout(root);
    check_dots(); CHECK(active_x==144);
    lv_screen_load(previous);
}
static void map_edges(const std::string &dir) {
    viewmodel::BaseViewModel vm;vm.set_dark_mode(dark);app::AssetManager assets;
    viewmodel::NodeInfo me,n;me.id="self";me.isSelf=true;me.hasPos=true;
    n.id="peer";n.name=std::string(70,'W');n.hasPos=true;n.lon=1;vm.set_nodes({me,n});
    screen::MapScreen s(vm,assets);bounds(find(s.root(),n.name));screenshot(s.root(),dir,"map-long.png");
}
static void search() {
    viewmodel::BaseViewModel vm;vm.set_dark_mode(dark);app::AssetManager assets;channel(vm);
    screen::MessagesScreen s(vm,assets);CHECK(s.on_key('/'));
    CHECK(std::string(lv_textarea_get_placeholder_text(input(s.root())))=="Search all messages");
    lv_textarea_set_text(input(s.root()),"Absent");s.on_enter();
    auto *context=find(s.root(),"Search: Absent");CHECK(context && !lv_obj_has_flag(context,LV_OBJ_FLAG_HIDDEN));
    CHECK(find(s.root(),"No messages match this search."));CHECK(s.on_back());
    CHECK(!find(s.root(),"No messages match this search."));
    CHECK(find(s.root(),"# 1: Synthetic [Private]"));
}
static void unicode() {
    CHECK(view::preview_text("abc",3)=="abc");
    CHECK(view::preview_text("line1\nline2\r\tend",40)=="line1 line2  end");
    CHECK(view::preview_text(std::string("a\0b",3),40)=="a b");
    CHECK(view::preview_text("aéz",2)=="a\xE2\x80\xA6");
    CHECK(view::preview_text("aéz",3)=="aé\xE2\x80\xA6");
    CHECK(view::preview_text("😀z",3)=="\xE2\x80\xA6");
    CHECK(view::preview_text("😀z",4)=="😀\xE2\x80\xA6");
}
static void font_fit(const std::string &dir) {
    viewmodel::BaseViewModel vm;vm.set_dark_mode(dark);app::AssetManager assets;channel(vm);
    auto *root=lv_obj_create(nullptr);lv_obj_set_size(root,320,170);
    {
        view::widgets::HelpPopup help(root,vm,assets);help.build();help.show(model::AppPage::Messages);
        lv_obj_update_layout(root);screenshot(root,dir,"help.png");
        const auto inspect=[&](auto &&self,lv_obj_t *o)->void {
            if(lv_obj_has_flag(o,LV_OBJ_FLAG_HIDDEN))return;
            if(lv_obj_check_type(o,&lv_label_class)) {
                const int line=lv_font_get_line_height(lv_obj_get_style_text_font(o,LV_PART_MAIN));
                CHECK(lv_obj_get_height(o)>=line);
                CHECK(lv_obj_get_height(lv_obj_get_parent(o))>=line);
                lv_area_t r,parent;lv_obj_get_coords(o,&r);lv_obj_get_coords(lv_obj_get_parent(o),&parent);
                CHECK(r.x1>=parent.x1 && r.x2<=parent.x2 && r.y1>=parent.y1 && r.y2<=parent.y2);
            }
            for(uint32_t i=0;i<lv_obj_get_child_count(o);++i)self(self,lv_obj_get_child(o,i));
        };
        inspect(inspect,help.root());
    }
    lv_obj_delete(root);
 }
static void composer_fit(const std::string &dir) {
    viewmodel::BaseViewModel vm;vm.set_dark_mode(dark);app::AssetManager assets;channel(vm);
    screen::MessagesScreen s(vm,assets);s.on_down();s.on_enter();s.on_enter();
    CHECK(s.is_composing());auto *box=input(s.root());lv_obj_update_layout(s.root());
    std::cout << "composer content=" << lv_obj_get_content_height(box) << " line="
              << lv_font_get_line_height(lv_obj_get_style_text_font(box,LV_PART_MAIN)) << std::endl;
    CHECK(lv_obj_get_content_height(box)>=lv_font_get_line_height(lv_obj_get_style_text_font(box,LV_PART_MAIN)));
    screenshot(s.root(),dir,"composer.png");s.on_back();
}
int main(int argc,char **argv) {
    lv_init(); auto *display=lv_display_create(320,170);CHECK(display);
    auto *group=lv_group_create();lv_group_set_default(group);
    const std::string test=argc>1?argv[1]:"all",output=argc>2?argv[2]:"";
    for (bool theme : {false,true}) {
    dark = theme;
    const std::string dir=output.empty()?"":(std::filesystem::path(output)/(dark?"dark":"light")).string();
    for (const auto &entry : std::vector<std::pair<std::string,std::function<void()>>>{
         {"wrap",wrap},{"cancel-recipient",cancel_recipient},{"labels",[&]{long_labels(dir);}},{"slot",invalid_slot},
         {"quote",quote_without_channel},{"toast",[&]{toasts(dir);}},{"qr",[&]{qr(dir);}},{"header",header},{"home-standard",[&]{home_standard(dir);}},{"map",[&]{map_edges(dir);}},{"search",search},{"unicode",unicode},{"fonts",[&]{font_fit(dir);}},{"composer",[&]{composer_fit(dir);}}})
        if(test=="all" || test==entry.first) { std::cout<<entry.first<<std::endl;entry.second(); }
    }
    CHECK(lv_group_get_obj_count(group)==0);lv_group_delete(group);lv_display_delete(display);
}
