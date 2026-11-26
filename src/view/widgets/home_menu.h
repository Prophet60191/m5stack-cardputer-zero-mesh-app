// SPDX-License-Identifier: MIT
#pragma once

#include "lvgl.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace cp0_ui {

// NFC Tools home geometry, independently implemented as a presentation-only
// LVGL component. App palette and content are inputs, not global state.
struct HomeMenuPalette {
    uint32_t background, bar, text, accent, muted, border;
};

class HomeMenu {
public:
    static constexpr int width = 320, height = 170, header_height = 23;
    static constexpr int center_x = 52, center_y = 32, center_width = 216, center_height = 105;
    static constexpr int center_icon_y = 33, title_y = 82, subtitle_y = 108;
    static constexpr int left_icon_x = 14, right_icon_x = 270, side_icon_y = 69, side_icon_width = 37;
    static constexpr int dots_y = 138, divider_y = 146, footer_y = 153;
    static constexpr int center_icon_px = 40, side_icon_px = 28;
    static constexpr std::size_t max_slots = 24;

    // LVGL owns all objects. The caller owns navigation and invalidates this
    // non-owning handle when it deletes/cleans the root.
    lv_obj_t *header_bar{}, *header_icon{}, *header_title{}, *battery_label{};
    lv_obj_t *center_group{}, *left_icon{}, *center_icon{}, *right_icon{};
    lv_obj_t *title{}, *subtitle{}, *footer_label{};

    static HomeMenu create(lv_obj_t* root, HomeMenuPalette palette, std::size_t count,
                           const char* app_icon, const char* app_title, const char* footer) {
        HomeMenu m;
        m.root_ = root;
        m.header_bar = panel(root, 0, 0, width, header_height);
        m.header_icon = label(m.header_bar, 8, 5, 20, &lv_font_montserrat_12, LV_TEXT_ALIGN_LEFT);
        m.header_title = label(m.header_bar, 28, 5, 190, &lv_font_montserrat_12, LV_TEXT_ALIGN_LEFT);
        m.battery_label = label(m.header_bar, 272, 7, 40, &lv_font_montserrat_10, LV_TEXT_ALIGN_RIGHT);
        lv_obj_set_height(m.header_title, 17);
        lv_label_set_long_mode(m.header_title, LV_LABEL_LONG_DOT);
        lv_label_set_text(m.header_icon, nonnull(app_icon));
        lv_label_set_text(m.header_title, nonnull(app_title));

        m.left_icon = label(root, left_icon_x, side_icon_y, side_icon_width, &lv_font_montserrat_28, LV_TEXT_ALIGN_LEFT);
        m.right_icon = label(root, right_icon_x, side_icon_y, side_icon_width, &lv_font_montserrat_28, LV_TEXT_ALIGN_LEFT);
        for (auto* icon : {m.left_icon, m.right_icon})
            lv_obj_set_style_opa(icon, LV_OPA_40, 0);
        m.center_group = panel(root, center_x, center_y, center_width, center_height);
        m.center_icon = label(m.center_group, 0, center_icon_y - center_y, center_width, &lv_font_montserrat_40, LV_TEXT_ALIGN_CENTER);
        m.title = label(m.center_group, 0, title_y - center_y, center_width, &lv_font_montserrat_16, LV_TEXT_ALIGN_CENTER);
        m.subtitle = label(m.center_group, 0, subtitle_y - center_y, center_width, &lv_font_montserrat_10, LV_TEXT_ALIGN_CENTER);

        if (count > 1 && count <= max_slots) {
            m.count_ = count;
            // Center the inactive slots, not each animated pill. An active
            // pill grows to the right, matching the reference, including x=135
            // for the first of six slots.
            const int first = (width - (static_cast<int>(count) - 1) * 9 - 4) / 2;
            for (std::size_t i = 0; i < count; ++i) {
                m.dots_[i] = panel(root, first + static_cast<int>(i) * 9, dots_y, 4, 3);
                lv_obj_set_style_radius(m.dots_[i], 2, 0);
            }
        }
        m.divider_ = panel(root, 8, divider_y, 304, 1);
        m.footer_label = label(root, 8, footer_y, 307, &lv_font_montserrat_10, LV_TEXT_ALIGN_LEFT);
        lv_obj_set_height(m.footer_label, 13);
        lv_label_set_long_mode(m.footer_label, LV_LABEL_LONG_DOT);
        lv_label_set_text(m.footer_label, nonnull(footer));
        m.set_palette(palette);
        m.set_item(0, "", "", "", "", "");
        m.set_battery(-1);
        return m;
    }

    void set_palette(HomeMenuPalette palette) {
        if (!root_) return;
        palette_ = palette;
        bg(root_, palette.background);
        bg(header_bar, palette.bar);
        bg(center_group, palette.background);
        bg(divider_, palette.border);
        for (auto* item : {header_title, title}) ink(item, palette.text);
        for (auto* item : {header_icon, center_icon}) ink(item, palette.accent);
        for (auto* item : {left_icon, right_icon, subtitle, footer_label, battery_label}) ink(item, palette.muted);
        draw_dots();
    }

    void set_item(std::size_t index, const char* left, const char* center, const char* right,
                  const char* name, const char* detail) {
        if (!root_) return;
        lv_label_set_text(left_icon, nonnull(left));
        lv_label_set_text(center_icon, nonnull(center));
        lv_label_set_text(right_icon, nonnull(right));
        lv_label_set_text(title, nonnull(name));
        lv_label_set_text(subtitle, nonnull(detail));
        active_ = index;
        draw_dots();
    }

    void set_battery(int percent) {
        if (!battery_label) return;
        if (percent < 0 || percent > 100) {
            lv_label_set_text(battery_label, "");
            return;
        }
        char value[6];
        std::snprintf(value, sizeof(value), "%d%%", percent);
        lv_label_set_text(battery_label, value);
    }

    void animate(int direction) {
        if (!center_group) return;
        lv_anim_delete(center_group, translate);
        lv_anim_delete(center_group, opacity);
        if (!direction) {
            translate(center_group, 0);
            opacity(center_group, LV_OPA_COVER);
            return;
        }
        lv_anim_t motion;
        lv_anim_init(&motion);
        lv_anim_set_var(&motion, center_group);
        lv_anim_set_values(&motion, direction > 0 ? 46 : -46, 0);
        lv_anim_set_duration(&motion, 180);
        lv_anim_set_path_cb(&motion, lv_anim_path_ease_out);
        lv_anim_set_exec_cb(&motion, translate);
        lv_anim_start(&motion);
        lv_anim_t fade;
        lv_anim_init(&fade);
        lv_anim_set_var(&fade, center_group);
        lv_anim_set_values(&fade, LV_OPA_TRANSP, LV_OPA_COVER);
        lv_anim_set_duration(&fade, 180);
        lv_anim_set_exec_cb(&fade, opacity);
        lv_anim_start(&fade);
    }

private:
    lv_obj_t *root_{}, *divider_{};
    HomeMenuPalette palette_{};
    std::array<lv_obj_t*, max_slots> dots_{};
    std::size_t count_{}, active_{};

    static const char* nonnull(const char* value) { return value ? value : ""; }
    static void bg(lv_obj_t* obj, uint32_t color) {
        lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
        lv_obj_set_style_bg_opa(obj, LV_OPA_COVER, 0);
    }
    static void ink(lv_obj_t* obj, uint32_t color) {
        lv_obj_set_style_text_color(obj, lv_color_hex(color), 0);
    }
    static lv_obj_t* panel(lv_obj_t* parent, int x, int y, int w, int h) {
        auto* obj = lv_obj_create(parent);
        lv_obj_remove_style_all(obj);
        lv_obj_set_pos(obj, x, y);
        lv_obj_set_size(obj, w, h);
        lv_obj_clear_flag(obj, static_cast<lv_obj_flag_t>(
            LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_CLICK_FOCUSABLE | LV_OBJ_FLAG_SCROLLABLE));
        return obj;
    }
    static lv_obj_t* label(lv_obj_t* parent, int x, int y, int w, const lv_font_t* font, lv_text_align_t align) {
        auto* obj = lv_label_create(parent);
        lv_obj_set_pos(obj, x, y);
        lv_obj_set_width(obj, w);
        lv_obj_set_style_text_font(obj, font, 0);
        lv_obj_set_style_text_align(obj, align, 0);
        lv_label_set_long_mode(obj, LV_LABEL_LONG_CLIP);
        return obj;
    }
    void draw_dots() {
        for (std::size_t i = 0; i < count_; ++i) {
            lv_obj_set_width(dots_[i], i == active_ ? 8 : 4);
            bg(dots_[i], i == active_ ? palette_.accent : palette_.border);
        }
    }
    static void translate(void* obj, int32_t x) {
        lv_obj_set_style_translate_x(static_cast<lv_obj_t*>(obj), x, 0);
    }
    static void opacity(void* obj, int32_t opa) {
        lv_obj_set_style_opa(static_cast<lv_obj_t*>(obj), static_cast<lv_opa_t>(opa), 0);
    }
};

} // namespace cp0_ui
