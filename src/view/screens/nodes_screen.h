/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jose Di Lenola
 */
#pragma once

#include "base_screen.h"
#include "mesh_data.h"

#include <vector>

namespace screen {

class NodesScreen : public BaseScreen {
public:
    NodesScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

    void on_up() override;
    void on_down() override;
    void on_enter() override;
    bool on_back() override;
    bool on_key(uint32_t key) override;

private:
    void build_content(lv_obj_t* content) override;
    void rebuild_display();   // apply filter + sort into display_
    void rebuild();
    void open_detail();
    void close_detail();
    const viewmodel::NodeInfo* acting_node() const;   // the node an action targets
    static void on_change(lv_observer_t* observer, lv_subject_t* subject);

    lv_obj_t* list_ = nullptr;
    lv_obj_t* content_area_ = nullptr;
    lv_obj_t* detail_ = nullptr;
    std::string detail_id_;   // the node shown in the detail overlay (stable target)
    int selected_ = 0;
    int filter_ = 0;   // 0 = all, 1 = favorites, 2 = active (heard < 1h)
    std::vector<viewmodel::NodeInfo> display_;
};

} // namespace screen
