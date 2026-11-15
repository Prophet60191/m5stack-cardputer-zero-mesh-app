/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

namespace model {

enum class AppPage {
    Menu     = 0,   // the rolling carousel home screen
    Messages = 1,
    Nodes    = 2,
    Map      = 3,
    Settings = 4,
};

inline constexpr int kAppPageCount = 5;
inline constexpr int kMenuEntryCount = 4;   // Messages, Nodes, Map, Settings

class BaseModel {
public:
    const char* app_title() const;
    const char* greeting() const;

    bool dark_mode() const;
    void set_dark_mode(bool enabled);
    void toggle_dark_mode();

    AppPage current_page() const;
    void set_current_page(AppPage page);
    void toggle_page();   // next page (cycles forward)
    void prev_page();     // previous page (cycles backward)

private:
    bool dark_mode_ = false;
    AppPage current_page_ = AppPage::Menu;
};

} // namespace model
