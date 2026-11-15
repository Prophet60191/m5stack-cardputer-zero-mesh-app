/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include "asset_manager.h"
#include "base_screen.h"
#include "base_viewmodel.h"
#include "lvgl.h"

#include <memory>

namespace app {

class ScreenManager {
public:
    ScreenManager(viewmodel::BaseViewModel& view_model, AssetManager& assets);
    ~ScreenManager();

    ScreenManager(const ScreenManager&) = delete;
    ScreenManager& operator=(const ScreenManager&) = delete;

    void start();
    void show_menu_screen();
    void show_messages_screen();
    void show_nodes_screen();
    void show_map_screen();
    void show_settings_screen();
    void request_page(model::AppPage page);
    void flush_requested_page();
    lv_obj_t* current_screen() const;
    void key_up();     // delegate keyboard nav to the active screen
    void key_down();
    void key_enter();
    bool key_back();   // true if the screen consumed Esc (e.g. closed an overlay)
    bool key_char(uint32_t key);   // other keys (e.g. 'f'); true if the screen used it
    bool is_composing();           // the active screen is capturing text input

private:
    void load_screen(std::unique_ptr<screen::BaseScreen> next_screen);

    viewmodel::BaseViewModel& view_model_;
    AssetManager& assets_;
    std::unique_ptr<screen::BaseScreen> current_screen_;
    lv_observer_t* page_observer_ = nullptr;
    model::AppPage requested_page_ = model::AppPage::Menu;
    model::AppPage loaded_page_ = model::AppPage::Menu;
    bool has_loaded_page_ = false;
    bool page_switch_scheduled_ = false;
};

} // namespace app
