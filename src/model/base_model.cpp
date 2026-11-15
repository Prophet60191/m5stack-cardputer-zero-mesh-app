/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "base_model.h"

namespace model {

const char* BaseModel::app_title() const {
    return "Mesh";
}

const char* BaseModel::greeting() const {
    return "Hello World";
}

bool BaseModel::dark_mode() const {
    return dark_mode_;
}

void BaseModel::set_dark_mode(bool enabled) {
    dark_mode_ = enabled;
}

void BaseModel::toggle_dark_mode() {
    dark_mode_ = !dark_mode_;
}

AppPage BaseModel::current_page() const {
    return current_page_;
}


void BaseModel::set_current_page(AppPage page) {
    current_page_ = page;
}

void BaseModel::toggle_page() {
    // Cycle Messages -> Nodes -> Settings -> Messages.
    current_page_ = static_cast<AppPage>((static_cast<int>(current_page_) + 1) % kAppPageCount);
}

void BaseModel::prev_page() {
    current_page_ = static_cast<AppPage>(
        (static_cast<int>(current_page_) + kAppPageCount - 1) % kAppPageCount);
}

} // namespace model
