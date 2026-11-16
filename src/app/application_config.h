/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <string>
#include <vector>

namespace app {

// The app-local, persisted settings (an INI file in the user's config dir).
// Mesh state (radio, channels, node db) lives in the supervisor; this is only
// what the UI owns: the theme and the messaging preferences.
struct ApplicationConfig {
    bool dark_mode = false;

    // Messaging preferences.
    bool show_signal = false;    // show SNR/hops under received bubbles
    int retention_cap = 0;       // max messages kept per thread (0 = keep all)
    bool mute_all = false;       // suppress all message notifications
    bool want_ack = true;        // request delivery acks when sending
    int quiet_start = -1; // local minutes since midnight; -1 disables
    int quiet_end = -1;
    bool auto_start = false;     // start the radio when the app launches

    std::vector<std::string> canned;     // quick messages
    std::vector<std::string> muted;      // muted thread keys
    std::vector<std::string> favorites;  // favourited node ids

    // True once a [messaging] section was seen on load; lets the caller tell a
    // first run (keep the built-in defaults) from a saved-but-empty list.
    bool has_messaging = false;
};

bool load_application_config(const std::string& path,
                             ApplicationConfig& config,
                             std::string& error);
bool save_application_config(const std::string& path,
                             const ApplicationConfig& config,
                             std::string& error);

} // namespace app
