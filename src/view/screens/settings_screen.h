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
    static void services_cb(lv_observer_t*, lv_subject_t*);
    void service_result();
    void request_service(const std::string& route, const std::string& intent, const std::string& fields = "");
    bool extended_picker(const std::string& pending, int choice);
    bool extended_text(const std::string& pending, const std::string& value);
    std::string service_route_, setup_region_, setup_kind_, channel_action_, channel_id_, profile_id_;
    std::string channel_fields_, channel_name_;
    int managed_channel_ = -1;
    bool offered_setup_ = false;
    std::vector<json::Value> service_items_;
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
    std::vector<std::string> position_channel_ids_;
    int public_position_channel_ = -1;
    std::string public_position_identity_;
    std::string pending_;      // which setting the overlay edits
    bool from_canned_ = false; // a text editor opened from the Quick-messages list
    int canned_edit_index_ = -1;
};

} // namespace screen
