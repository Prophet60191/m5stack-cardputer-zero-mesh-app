/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jose Di Lenola
 */
#pragma once

#include "base_screen.h"

namespace screen {

// The home screen: a rolling carousel. Left/Right roll the selection, Enter opens
// it. One big icon + label centered that slides as it changes, with the
// neighbouring icons shown faintly at the left/right edges.
class MenuScreen : public BaseScreen {
public:
    MenuScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
    void update(bool animate);
    void animate_in(int direction);   // -1 = came from the left, +1 = from the right
    static void on_change(lv_observer_t* observer, lv_subject_t* subject);

    lv_obj_t* center_ = nullptr;   // the sliding icon+label group
    lv_obj_t* icon_ = nullptr;
    lv_obj_t* label_ = nullptr;
    lv_obj_t* prev_icon_ = nullptr;   // faint neighbour at the left edge
    lv_obj_t* next_icon_ = nullptr;   // faint neighbour at the right edge
    int last_index_ = -1;
};

} // namespace screen
