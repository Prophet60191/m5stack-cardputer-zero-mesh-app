/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jose Di Lenola
 */
#pragma once

#include "base_screen.h"

namespace screen {

// A relative-position plot: you at the centre, other nodes placed by their
// bearing and distance from you (offline — no map tiles).
class MapScreen : public BaseScreen {
public:
    MapScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

private:
    void build_content(lv_obj_t* content) override;
    void rebuild();
    static void on_change(lv_observer_t* observer, lv_subject_t* subject);

    lv_obj_t* plot_ = nullptr;
};

} // namespace screen
