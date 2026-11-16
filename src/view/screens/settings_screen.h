/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jose Di Lenola
 */
#pragma once

#include "base_screen.h"

#include <string>
#include <vector>

namespace screen {

// A selectable settings list (Up/Down + Enter), with pickers, toggles, text
// editors and info overlays for the rows.
class SettingsScreen : public BaseScreen {
public:
    SettingsScreen(viewmodel::BaseViewModel& view_model, app::AssetManager& assets);

    void on_up() override;
    void on_down() override;
    void on_enter() override;
    bool on_back() override;
    bool is_composing() override;

private:
    enum class Overlay { None, Picker, Text, Info, Canned };

    void build_content(lv_obj_t* content) override;
    void rebuild();
    std::string value_for(int row);
    void row_action(int row);
    void open_picker(const std::string& title, std::vector<std::string> options,
                     const std::string& pending, int initial);
    void open_text(const std::string& title, const std::string& initial, const std::string& pending);
    void open_info(const std::string& title, std::vector<std::string> rows);
    void open_canned();
    void teardown_overlay_obj();
    void rebuild_picker();
    void close_overlay();
    void apply_overlay();
    static void on_change(lv_observer_t* observer, lv_subject_t* subject);

    lv_obj_t* list_ = nullptr;
    lv_obj_t* overlay_obj_ = nullptr;
    lv_obj_t* input_ = nullptr;
    std::string overlay_title_;
    Overlay overlay_mode_ = Overlay::None;
    int selected_ = 0;         // row selection
    int opt_sel_ = 0;          // picker selection
    std::vector<std::string> options_;
    std::vector<int> position_channels_;
    int public_position_channel_ = -1;
    std::string pending_;      // which setting the overlay edits
    bool from_canned_ = false; // a text editor opened from the Quick-messages list
    int canned_edit_index_ = -1;
};

} // namespace screen
