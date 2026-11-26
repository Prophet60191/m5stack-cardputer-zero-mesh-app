/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jose Di Lenola
 */
#pragma once

#include "base_screen.h"
#include "home_menu.h"

namespace screen {

// The home screen: a rolling carousel. Left/Right roll the selection, Enter opens
// it. One big icon + label centered that slides as it changes, with the
// neighbouring icons shown faintly at the left/right edges.
class MenuScreen : public BaseScreen {
public:
    MenuScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);
    ~MenuScreen() override;

private:
    bool owns_home_chrome() const override { return true; }
    void build_content(lv_obj_t* root) override;
    void update(bool animate);
    void refresh_battery();
    void refresh_mesh_status();
    static void on_change(lv_observer_t* observer, lv_subject_t* subject);
    static void on_battery_tick(lv_timer_t* timer);

    cp0_ui::HomeMenu menu_;
    lv_obj_t* radio_ = nullptr;
    lv_obj_t* region_ = nullptr;
    lv_obj_t* nodes_ = nullptr;
    lv_obj_t* unread_ = nullptr;
    lv_timer_t* battery_timer_ = nullptr;
    int last_index_ = -1;
};

} // namespace screen
