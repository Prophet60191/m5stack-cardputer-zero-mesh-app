/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jose Di Lenola  (originally M5Stack Template, MIT)
 */
#include "titlebar.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>

#include "asset_manager.h"
#include "bindings.h"
#include "device_status.h"
#include "theme.h"
#include "ui_const.h"

namespace view::widgets {
namespace {

constexpr uint32_t kDeviceRefreshTicks = 5;

void set_visible(lv_obj_t* obj, bool visible) {
    if (visible) lv_obj_remove_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace

TitleBar::TitleBar(lv_obj_t* parent, viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseWidgets(parent), view_model_(view_model), assets_(assets) {}

TitleBar::~TitleBar() {
    if (refresh_timer_) {
        lv_timer_delete(refresh_timer_);
        refresh_timer_ = nullptr;
    }
}

void TitleBar::build() {
    if (core_obj_) {
        return;
    }

    core_obj_ = lv_obj_create(parent_);
    lv_obj_remove_style_all(core_obj_);
    lv_obj_set_size(core_obj_, LV_PCT(100), kTitleBarHeight);
    lv_obj_align(core_obj_, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_clear_flag(core_obj_, LV_OBJ_FLAG_SCROLLABLE);
    reactive::bind_theme(core_obj_, view_model_.dark_mode_subject(), reactive::ThemeRole::Bar);

    const lv_font_t* text_font = assets_.load_standard_font(13, app::StandardFontWeight::Regular);
    if (!text_font) text_font = &lv_font_montserrat_14;

    // Left: radio dot + state + region.
    auto* left = lv_obj_create(core_obj_);
    lv_obj_remove_style_all(left);
    lv_obj_set_size(left, 214, LV_PCT(100));
    lv_obj_align(left, LV_ALIGN_LEFT_MID, 13, 0);
    lv_obj_set_flex_flow(left, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(left, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(left, 6, 0);
    lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);

    radio_dot_ = lv_obj_create(left);
    lv_obj_remove_style_all(radio_dot_);
    lv_obj_set_size(radio_dot_, 10, 10);
    lv_obj_set_style_radius(radio_dot_, LV_RADIUS_CIRCLE, 0);
    lv_obj_clear_flag(radio_dot_, LV_OBJ_FLAG_SCROLLABLE);

    state_label_ = lv_label_create(left);

    lv_obj_set_style_text_font(state_label_, text_font, 0);
    reactive::bind_theme(state_label_, view_model_.dark_mode_subject(), reactive::ThemeRole::Text);

    region_label_ = lv_label_create(left);
    lv_label_bind_text(region_label_, view_model_.region_subject(), nullptr);
    lv_obj_set_style_text_font(region_label_, text_font, 0);
    reactive::bind_theme(region_label_, view_model_.dark_mode_subject(), reactive::ThemeRole::Text);

    nodes_label_ = lv_label_create(left);
    lv_obj_set_style_text_font(nodes_label_, text_font, 0);
    reactive::bind_theme(nodes_label_, view_model_.dark_mode_subject(), reactive::ThemeRole::Text);

    // Unread badge (envelope + count) — hidden when there is nothing unread.
    unread_label_ = lv_label_create(left);
    lv_obj_set_style_text_font(unread_label_, &lv_font_montserrat_14, 0);
    lv_obj_add_flag(unread_label_, LV_OBJ_FLAG_HIDDEN);

    // Right: battery percentage, without a decorative icon.
    auto* right = lv_obj_create(core_obj_);
    lv_obj_remove_style_all(right);
    lv_obj_set_size(right, 70, LV_PCT(100));
    lv_obj_align(right, LV_ALIGN_RIGHT_MID, -6, 0);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(right, 4, 0);
    lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLLABLE);

    battery_percent_label_ = lv_label_create(right);
    lv_obj_set_style_text_font(battery_percent_label_, text_font, 0);
    reactive::bind_theme(battery_percent_label_, view_model_.dark_mode_subject(), reactive::ThemeRole::Text);

    update_radio_dot();
    update_node_count();
    update_unread();
    refresh_device_status();

    lv_subject_add_observer_obj(view_model_.radio_state_subject(), radio_cb, core_obj_, this);
    lv_subject_add_observer_obj(view_model_.health_subject(), radio_cb, core_obj_, this);
    lv_subject_add_observer_obj(view_model_.dark_mode_subject(), radio_cb, core_obj_, this);
    lv_subject_add_observer_obj(view_model_.nodes_version_subject(), nodes_cb, core_obj_, this);
    lv_subject_add_observer_obj(view_model_.unread_version_subject(), unread_cb, core_obj_, this);

    refresh_timer_ = lv_timer_create(refresh_timer_cb, 1000, this);
}

void TitleBar::update_radio_dot() {
    if (!radio_dot_) return;
    const auto colors = view::palette(view_model_.is_dark_mode());
    const char* state = lv_subject_get_string(view_model_.radio_state_subject());
    lv_color_t color = colors.text_disabled;  // off
    if (state) {
        if (std::strcmp(state, "online") == 0) color = colors.success;
        else if (std::strcmp(state, "starting") == 0) color = colors.warning;
        else if (std::strcmp(state, "error") == 0) color = colors.error;
    }
    const char *label = state ? state : "unknown";
    if (state && std::strcmp(state, "online") == 0) {
        label = view_model_.radio_mode() == "receive-only" ? "RX"
                : view_model_.radio_mode() == "transmit"   ? "TX/RX"
                                                           : "Online ?";
    }
    lv_label_set_text(state_label_, label);
    lv_obj_set_style_bg_opa(radio_dot_, LV_OPA_COVER, 0);
    lv_obj_set_style_bg_color(radio_dot_, color, 0);
    lv_obj_invalidate(core_obj_);
}

void TitleBar::refresh_device_status() {
    const auto status = platform::read_device_status();
    set_visible(battery_percent_label_, status.battery_present);
    if (status.battery_present) {
        std::array<char, 6> percent{};
        std::snprintf(percent.data(), percent.size(), "%d%%", std::clamp(status.battery_percent, 0, 100));
        lv_label_set_text(battery_percent_label_, percent.data());
    }
    // Refresh the complete strip: partial fbdev redraws can otherwise erase
    // unchanged status text after removing the neighboring battery glyph.
    lv_obj_invalidate(core_obj_);
}

void TitleBar::update_node_count() {
    if (!nodes_label_) return;
    const size_t count = view_model_.nodes().size();
    char buf[24];
    std::snprintf(buf, sizeof(buf), "%zu node%s", count, count == 1 ? "" : "s");
    lv_label_set_text(nodes_label_, buf);
    lv_obj_invalidate(core_obj_);
}

void TitleBar::update_unread() {
    if (!unread_label_) return;
    const int unread = view_model_.total_unread();
    if (unread <= 0) {
        lv_obj_add_flag(unread_label_, LV_OBJ_FLAG_HIDDEN);
        lv_obj_invalidate(core_obj_);
        return;
    }
    const auto colors = view::palette(view_model_.is_dark_mode());
    char buf[24];
    std::snprintf(buf, sizeof(buf), LV_SYMBOL_ENVELOPE " %d", unread);
    lv_label_set_text(unread_label_, buf);
    lv_obj_set_style_text_color(unread_label_, colors.primary, 0);
    lv_obj_remove_flag(unread_label_, LV_OBJ_FLAG_HIDDEN);
    lv_obj_invalidate(core_obj_);
}

void TitleBar::radio_cb(lv_observer_t* observer, lv_subject_t* /*subject*/) {
    auto* self = static_cast<TitleBar*>(lv_observer_get_user_data(observer));
    if (self) {
        self->update_radio_dot();
    }
}

void TitleBar::nodes_cb(lv_observer_t* observer, lv_subject_t* /*subject*/) {
    auto* self = static_cast<TitleBar*>(lv_observer_get_user_data(observer));
    if (self) {
        self->update_node_count();
    }
}

void TitleBar::unread_cb(lv_observer_t* observer, lv_subject_t* /*subject*/) {
    auto* self = static_cast<TitleBar*>(lv_observer_get_user_data(observer));
    if (self) {
        self->update_unread();
    }
}

void TitleBar::refresh_timer_cb(lv_timer_t* timer) {
    auto* self = static_cast<TitleBar*>(lv_timer_get_user_data(timer));
    if (!self) return;
    if (++self->refresh_tick_ >= kDeviceRefreshTicks) {
        self->refresh_tick_ = 0;
        self->refresh_device_status();
    }
}

} // namespace view::widgets
