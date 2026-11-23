/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jose Di Lenola
 */
#include "menu_screen.h"

#include "asset_manager.h"
#include "bindings.h"
#include "theme.h"

namespace screen {
namespace {

struct Entry {
    const char* icon;
    const char* label;
};

// Order must match the view-model's menu index (0=Messages .. 3=Settings).
const Entry kEntries[4] = {
    {LV_SYMBOL_ENVELOPE, "Messages"},
    {LV_SYMBOL_WIFI,     "Nodes"},
    {LV_SYMBOL_GPS,      "Map"},
    {LV_SYMBOL_SETTINGS, "Settings"},
};

constexpr int kEntryCount = 4;
constexpr int kSlidePx = 46;     // how far the new item slides in from
constexpr uint32_t kSlideMs = 180;

// LVGL animation exec callbacks (plain C function pointers).
void anim_translate_x_cb(void* obj, int32_t value) {
    lv_obj_set_style_translate_x(static_cast<lv_obj_t*>(obj), value, 0);
}

void anim_opa_cb(void* obj, int32_t value) {
    lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(value), 0);
}

}  // namespace

MenuScreen::MenuScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

void MenuScreen::build_content(lv_obj_t* content) {
    // Faint neighbour icons peeking at the far edges.
    prev_icon_ = lv_label_create(content);
    lv_obj_set_style_text_font(prev_icon_, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_opa(prev_icon_, LV_OPA_40, 0);
    lv_obj_align(prev_icon_, LV_ALIGN_LEFT_MID, 12, 0);

    next_icon_ = lv_label_create(content);
    lv_obj_set_style_text_font(next_icon_, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_opa(next_icon_, LV_OPA_40, 0);
    lv_obj_align(next_icon_, LV_ALIGN_RIGHT_MID, -12, 0);

    // The centre group: one big icon above its label. This is what slides.
    center_ = lv_obj_create(content);
    lv_obj_remove_style_all(center_);
    lv_obj_set_size(center_, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(center_, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(center_, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(center_, 10, 0);
    lv_obj_center(center_);
    lv_obj_clear_flag(center_, LV_OBJ_FLAG_SCROLLABLE);

    icon_ = lv_label_create(center_);
    lv_obj_set_style_text_font(icon_, &lv_font_montserrat_40, 0);

    label_ = lv_label_create(center_);
    auto* label_font = assets().load_standard_font(17, app::StandardFontWeight::Regular);
    lv_obj_set_style_text_font(label_, label_font ? label_font : &lv_font_montserrat_18, 0);
    reactive::bind_theme(label_, view_model().dark_mode_subject(), reactive::ThemeRole::Text);

    update(false);
    lv_subject_add_observer_obj(view_model().menu_index_subject(), on_change, icon_, this);
    lv_subject_add_observer_obj(view_model().dark_mode_subject(), on_change, icon_, this);
}

void MenuScreen::on_change(lv_observer_t* observer, lv_subject_t* subject) {
    auto* self = static_cast<MenuScreen*>(lv_observer_get_user_data(observer));
    if (!self) {
        return;
    }
    // Only slide when the selection changed; a theme flip just recolours.
    const bool selection_changed = subject == self->view_model().menu_index_subject();
    self->update(selection_changed);
}

void MenuScreen::update(bool animate) {
    int i = lv_subject_get_int(view_model().menu_index_subject());
    if (i < 0 || i >= kEntryCount) {
        i = 0;
    }
    const auto colors = view::palette(view_model().is_dark_mode());

    lv_label_set_text(icon_, kEntries[i].icon);
    lv_label_set_text(label_, kEntries[i].label);
    lv_obj_set_style_text_color(icon_, view_model().is_dark_mode() ? colors.primary : lv_color_hex(0x139a4d), 0);  // active icon in mesh green

    // Faint neighbours (wrap around).
    const int prev = (i + kEntryCount - 1) % kEntryCount;
    const int next = (i + 1) % kEntryCount;
    lv_label_set_text(prev_icon_, kEntries[prev].icon);
    lv_label_set_text(next_icon_, kEntries[next].icon);
    lv_obj_set_style_text_color(prev_icon_, lv_color_hex(view_model().is_dark_mode() ? 0x616161 : 0xc2c2c2), 0);
    lv_obj_set_style_text_color(next_icon_, lv_color_hex(view_model().is_dark_mode() ? 0x616161 : 0xc2c2c2), 0);

    if (animate && last_index_ >= 0 && i != last_index_) {
        // Which way did we roll? next (incl. 3->0 wrap) slides in from the right.
        const int delta = i - last_index_;
        const bool went_next = (delta == 1) || (delta == -(kEntryCount - 1));
        animate_in(went_next ? +1 : -1);
    }
    last_index_ = i;
}

void MenuScreen::animate_in(int direction) {
    // Slide the new item in from the edge it came from, fading up as it lands.
    lv_anim_t slide;
    lv_anim_init(&slide);
    lv_anim_set_var(&slide, center_);
    lv_anim_set_values(&slide, direction * kSlidePx, 0);
    lv_anim_set_duration(&slide, kSlideMs);
    lv_anim_set_path_cb(&slide, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&slide, anim_translate_x_cb);
    lv_anim_start(&slide);

    lv_anim_t fade;
    lv_anim_init(&fade);
    lv_anim_set_var(&fade, center_);
    lv_anim_set_values(&fade, LV_OPA_TRANSP, LV_OPA_COVER);
    lv_anim_set_duration(&fade, kSlideMs);
    lv_anim_set_exec_cb(&fade, anim_opa_cb);
    lv_anim_start(&fade);
}

} // namespace screen
