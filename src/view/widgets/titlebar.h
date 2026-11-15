/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jose Di Lenola  (originally M5Stack Template, MIT)
 */
#pragma once

#include "base_widget.h"
#include "base_viewmodel.h"

namespace app {
class AssetManager;
}

namespace view::widgets {

// A status-only top bar (no tab names): a radio status dot + state + region on
// the left, battery on the right. Room to add node count, unread, etc.
class TitleBar : public BaseWidgets {
public:
    TitleBar(lv_obj_t* parent, viewmodel::BaseViewModel& view_model, app::AssetManager& assets);
    ~TitleBar() override;

    void build() override;

private:
    void update_radio_dot();
    void update_node_count();
    void update_unread();
    void refresh_device_status();
    static void refresh_timer_cb(lv_timer_t* timer);
    static void radio_cb(lv_observer_t* observer, lv_subject_t* subject);
    static void nodes_cb(lv_observer_t* observer, lv_subject_t* subject);
    static void unread_cb(lv_observer_t* observer, lv_subject_t* subject);

    viewmodel::BaseViewModel& view_model_;
    app::AssetManager& assets_;
    lv_obj_t* radio_dot_{nullptr};
    lv_obj_t* state_label_{nullptr};
    lv_obj_t* region_label_{nullptr};
    lv_obj_t* nodes_label_{nullptr};
    lv_obj_t* unread_label_{nullptr};
    lv_obj_t* battery_icon_label_{nullptr};
    lv_obj_t* battery_percent_label_{nullptr};
    lv_timer_t* refresh_timer_{nullptr};
    uint32_t refresh_tick_{0};
};

} // namespace view::widgets
