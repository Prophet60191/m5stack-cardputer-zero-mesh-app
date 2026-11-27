// SPDX-License-Identifier: MIT
#pragma once
#include "system_keys.h"
#include "../linux_input.h"
#include "lvgl.h"
namespace cp0keys {
// An app-owned hook, not a global keyboard reader or background service.
// Only the LVGL thread polls results and creates/updates the transient label.
class Bridge {
 Controls controls_;lv_timer_t* timer_=nullptr;lv_obj_t* label_=nullptr;uint32_t shown_=0;
 static bool input(unsigned code,int value,void* data){return static_cast<Bridge*>(data)->controls_.key(code,value);}
 void poll(){
  while(auto result=controls_.poll()){
   if(!label_){label_=lv_label_create(lv_layer_top());lv_obj_set_size(label_,300,24);lv_label_set_long_mode(label_,LV_LABEL_LONG_DOT);
    lv_obj_set_style_bg_color(label_,lv_color_hex(0x202020),0);lv_obj_set_style_bg_opa(label_,LV_OPA_COVER,0);
    lv_obj_set_style_text_color(label_,lv_color_hex(0xffffff),0);lv_obj_set_style_text_font(label_,&lv_font_montserrat_14,0);
    lv_obj_set_style_pad_all(label_,4,0);lv_obj_align(label_,LV_ALIGN_TOP_MID,0,2);lv_obj_remove_flag(label_,LV_OBJ_FLAG_CLICKABLE);lv_obj_remove_flag(label_,LV_OBJ_FLAG_SCROLLABLE);}
   lv_obj_move_foreground(label_);lv_label_set_text(label_,result->text.c_str());lv_obj_remove_flag(label_,LV_OBJ_FLAG_HIDDEN);shown_=lv_tick_get();
  }
  if(label_&&lv_tick_elaps(shown_)>=1200)lv_obj_add_flag(label_,LV_OBJ_FLAG_HIDDEN);
 }
 public:
 Bridge(){platform::set_system_key_listener(input,this);timer_=lv_timer_create([](lv_timer_t* t){static_cast<Bridge*>(lv_timer_get_user_data(t))->poll();},40,this);}
 ~Bridge(){platform::clear_system_key_listener(input,this);if(timer_)lv_timer_delete(timer_);if(label_)lv_obj_delete(label_);}
 Bridge(const Bridge&)=delete;Bridge& operator=(const Bridge&)=delete;
};
}
