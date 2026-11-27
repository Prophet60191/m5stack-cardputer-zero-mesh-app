/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jose Di Lenola
 */
#include "menu_screen.h"

#include "device_status.h"
#include "ui_const.h"

#include <cstdio>
#include <cstring>

namespace screen {
namespace {

struct Entry {
    const char* icon;
    const char* title;
    const char* subtitle;
};

// Order must match the view-model's menu index (0=Messages .. 3=Settings).
constexpr Entry kEntries[] = {
    {LV_SYMBOL_ENVELOPE, "Messages", "Read, write & manage messages"},
    {LV_SYMBOL_WIFI,     "Nodes",    "Find and manage mesh nodes"},
    {LV_SYMBOL_GPS,      "Map",      "View node locations"},
    {LV_SYMBOL_SETTINGS, "Settings", "Radio, appearance & preferences"},
};

constexpr int kEntryCount = sizeof(kEntries) / sizeof(kEntries[0]);

cp0_ui::HomeMenuPalette menu_palette(bool dark_mode) {
    using namespace view::color;
    return dark_mode
        ? cp0_ui::HomeMenuPalette{dark::kBody, dark::kAction, dark::kTextPrimary,
                                dark::kPrimary, dark::kTextDisabled, dark::kBorder}
        : cp0_ui::HomeMenuPalette{light::kBody, light::kAction, light::kTextPrimary,
                                light::kSuccess, light::kTextDisabled, light::kBorder};
}

lv_obj_t* status_label(lv_obj_t* bar, int x, int width) {
    auto* label = lv_label_create(bar);
    lv_obj_set_pos(label, x, 7);
    lv_obj_set_size(label, width, 13);
    lv_obj_set_style_text_font(label, &lv_font_montserrat_10, 0);
    lv_label_set_long_mode(label, LV_LABEL_LONG_DOT);
    return label;
}

} // namespace

MenuScreen::MenuScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets)
    : BaseScreen(view_model, assets) {
    init();
}

MenuScreen::~MenuScreen() {
    if (battery_timer_) lv_timer_delete(battery_timer_);
}

void MenuScreen::build_content(lv_obj_t* root) {
    menu_ = cp0_ui::HomeMenu::create(root, menu_palette(view_model().is_dark_mode()), kEntryCount,
                                     LV_SYMBOL_WIFI, "MESH", "Z / C  choose    Enter  open    Fn+H  help");
    // The NFC chrome is unchanged. Mesh only fills part of the blank space
    // between app title and battery with its existing operational status.
    lv_obj_set_width(menu_.header_title, 53);
    radio_ = status_label(menu_.header_bar, 85, 46);
    region_ = status_label(menu_.header_bar, 134, 47);
    nodes_ = status_label(menu_.header_bar, 184, 33);
    unread_ = status_label(menu_.header_bar, 220, 47);
    update(false);
    refresh_mesh_status();
    refresh_battery();
    lv_subject_add_observer_obj(view_model().menu_index_subject(), on_change, root, this);
    lv_subject_add_observer_obj(view_model().dark_mode_subject(), on_change, root, this);
    for (auto* subject : {view_model().radio_state_subject(), view_model().health_subject(),
                          view_model().region_subject(), view_model().nodes_version_subject(),
                          view_model().unread_version_subject()}) {
        lv_subject_add_observer_obj(subject, on_change, root, this);
    }
    battery_timer_ = lv_timer_create(on_battery_tick, 5000, this);
}

void MenuScreen::on_change(lv_observer_t* observer, lv_subject_t* subject) {
    if (auto* self = static_cast<MenuScreen*>(lv_observer_get_user_data(observer))) {
        if (subject == self->view_model().menu_index_subject()) self->update(true);
        else if (subject == self->view_model().dark_mode_subject()) self->update(false);
        self->refresh_mesh_status();
    }
}

void MenuScreen::update(bool animate) {
    int i = lv_subject_get_int(view_model().menu_index_subject());
    if (i < 0 || i >= kEntryCount) i = 0;
    menu_.set_palette(menu_palette(view_model().is_dark_mode()));
    menu_.set_item(static_cast<std::size_t>(i),
                   kEntries[(i + kEntryCount - 1) % kEntryCount].icon,
                   kEntries[i].icon, kEntries[(i + 1) % kEntryCount].icon,
                   kEntries[i].title, kEntries[i].subtitle);
    if (animate && last_index_ >= 0 && i != last_index_) {
        const int delta = i - last_index_;
        const bool went_next = delta == 1 || delta == -(kEntryCount - 1);
        menu_.animate(went_next ? +1 : -1);
    }
    last_index_ = i;
}

void MenuScreen::refresh_battery() {
    const auto status = platform::read_device_status();
    menu_.set_battery(status.battery_present ? status.battery_percent : -1);
    lv_obj_invalidate(menu_.header_bar);
}

void MenuScreen::refresh_mesh_status() {
    const char* state = lv_subject_get_string(view_model().radio_state_subject());
    const char* label = state ? state : "unknown";
    const auto colors = menu_palette(view_model().is_dark_mode());
    auto radio_color = lv_color_hex(colors.muted);
    if (state && std::strcmp(state, "online") == 0) {
        label = view_model().radio_mode() == "receive-only" ? "RX"
                : view_model().radio_mode() == "transmit" ? "TX/RX" : "Online ?";
        radio_color = lv_color_hex(colors.accent);
    } else if (state && std::strcmp(state, "error") == 0) {
        radio_color = lv_color_hex(view_model().is_dark_mode() ? view::color::dark::kError : view::color::light::kError);
    }
    lv_label_set_text(radio_, label);
    lv_obj_set_style_text_color(radio_, radio_color, 0);
    lv_label_set_text(region_, lv_subject_get_string(view_model().region_subject()));
    lv_obj_set_style_text_color(region_, lv_color_hex(colors.muted), 0);
    char count[25];
    std::snprintf(count, sizeof(count), "%zun", view_model().nodes().size());
    lv_label_set_text(nodes_, count);
    lv_obj_set_style_text_color(nodes_, lv_color_hex(colors.muted), 0);
    const int unread = view_model().total_unread();
    if (unread > 0) {
        std::snprintf(count, sizeof(count), LV_SYMBOL_ENVELOPE " %d", unread);
        lv_label_set_text(unread_, count);
    } else {
        lv_label_set_text(unread_, "");
    }
    lv_obj_set_style_text_color(unread_, lv_color_hex(colors.accent), 0);
    lv_obj_invalidate(menu_.header_bar);
}

void MenuScreen::on_battery_tick(lv_timer_t* timer) {
    if (auto* self = static_cast<MenuScreen*>(lv_timer_get_user_data(timer))) self->refresh_battery();
}

} // namespace screen
