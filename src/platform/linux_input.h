#pragma once

#include "lvgl.h"
#include "keyboard_decoder.h"

#include <cstddef>
#include <cstdint>

namespace platform {

constexpr size_t kNavKeyCount = 5;

using KeyReleaseListener = void (*)(uint32_t key, const char* key_name, void* user_data);
using GlobalKeyListener = bool (*)(uint32_t key,
                                   const char* key_name,
                                   bool long_pressed,
                                   void* user_data);

void init_key_input(lv_display_t* display);
// Call once for each new decoded press/release, inside the indev read callback,
// BEFORE LVGL can deliver it to a focused object. Consumed shortcuts never type.
void route_key_input(lv_indev_data_t* data);
using TextEntryQuery = bool (*)(void* user_data);
void set_text_entry_query(TextEntryQuery query, void* user_data);
void clear_text_entry_query(TextEntryQuery query, void* user_data);
void set_nav_shortcut_mode(bool enabled);
void set_text_entry_mode(bool enabled);
void register_nav_button(size_t index, lv_obj_t* button);
void unregister_nav_button(size_t index, lv_obj_t* button);
void set_key_release_listener(KeyReleaseListener listener, void* user_data);
void clear_key_release_listener(KeyReleaseListener listener, void* user_data);
void set_global_key_listener(GlobalKeyListener listener, void* user_data);
void clear_global_key_listener(GlobalKeyListener listener, void* user_data);
const char* describe_key(uint32_t key);

} // namespace platform
