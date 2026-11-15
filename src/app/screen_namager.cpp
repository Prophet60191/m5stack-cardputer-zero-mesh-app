/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "screen_manager.h"

#include "map_screen.h"
#include "menu_screen.h"
#include "messages_screen.h"
#include "nodes_screen.h"
#include "settings_screen.h"

namespace app {
namespace {

void flush_requested_page_async(void* user_data) {
    auto* manager = static_cast<ScreenManager*>(user_data);
    if (manager) {
        manager->flush_requested_page();
    }
}

void current_page_observer(lv_observer_t* observer, lv_subject_t* subject) {
    auto* manager = static_cast<ScreenManager*>(lv_observer_get_target(observer));
    if (!manager) {
        return;
    }

    manager->request_page(static_cast<model::AppPage>(lv_subject_get_int(subject)));
}

} // namespace

ScreenManager::ScreenManager(viewmodel::BaseViewModel& view_model, AssetManager& assets)
    : view_model_(view_model), assets_(assets) {}

ScreenManager::~ScreenManager() {
    lv_async_call_cancel(flush_requested_page_async, this);

    if (page_observer_) {
        lv_observer_remove(page_observer_);
        page_observer_ = nullptr;
    }
}

void ScreenManager::start() {
    if (page_observer_) {
        return;
    }

    page_observer_ = lv_subject_add_observer_with_target(view_model_.current_page_subject(),
                                                         current_page_observer,
                                                         this,
                                                         nullptr);
}

void ScreenManager::show_menu_screen() {
    load_screen(std::make_unique<screen::MenuScreen>(view_model_, assets_));
    loaded_page_ = model::AppPage::Menu;
    has_loaded_page_ = true;
}

void ScreenManager::show_messages_screen() {
    load_screen(std::make_unique<screen::MessagesScreen>(view_model_, assets_));
    loaded_page_ = model::AppPage::Messages;
    has_loaded_page_ = true;
}

void ScreenManager::show_nodes_screen() {
    load_screen(std::make_unique<screen::NodesScreen>(view_model_, assets_));
    loaded_page_ = model::AppPage::Nodes;
    has_loaded_page_ = true;
}

void ScreenManager::show_map_screen() {
    load_screen(std::make_unique<screen::MapScreen>(view_model_, assets_));
    loaded_page_ = model::AppPage::Map;
    has_loaded_page_ = true;
}

void ScreenManager::show_settings_screen() {
    load_screen(std::make_unique<screen::SettingsScreen>(view_model_, assets_));
    loaded_page_ = model::AppPage::Settings;
    has_loaded_page_ = true;
}

void ScreenManager::request_page(model::AppPage page) {
    requested_page_ = page;

    if (page_switch_scheduled_) {
        return;
    }

    page_switch_scheduled_ = lv_async_call(flush_requested_page_async, this) == LV_RESULT_OK;
}

void ScreenManager::flush_requested_page() {
    page_switch_scheduled_ = false;

    if (has_loaded_page_ && requested_page_ == loaded_page_) {
        return;
    }

    switch (requested_page_) {
        case model::AppPage::Messages:
            show_messages_screen();
            return;
        case model::AppPage::Nodes:
            show_nodes_screen();
            return;
        case model::AppPage::Map:
            show_map_screen();
            return;
        case model::AppPage::Settings:
            show_settings_screen();
            return;
        case model::AppPage::Menu:
        default:
            show_menu_screen();
            return;
    }
}

lv_obj_t* ScreenManager::current_screen() const {
    return current_screen_ ? current_screen_->root() : nullptr;
}

void ScreenManager::key_up() {
    if (current_screen_) current_screen_->on_up();
}

void ScreenManager::key_down() {
    if (current_screen_) current_screen_->on_down();
}

void ScreenManager::key_enter() {
    if (current_screen_) current_screen_->on_enter();
}

bool ScreenManager::key_back() {
    return current_screen_ && current_screen_->on_back();
}

bool ScreenManager::key_char(uint32_t key) {
    return current_screen_ && current_screen_->on_key(key);
}

bool ScreenManager::is_composing() {
    return current_screen_ && current_screen_->is_composing();
}

void ScreenManager::load_screen(std::unique_ptr<screen::BaseScreen> next_screen) {
    if (!next_screen || !next_screen->root()) {
        return;
    }

    lv_screen_load(next_screen->root());
    current_screen_ = std::move(next_screen);
}

} // namespace app
