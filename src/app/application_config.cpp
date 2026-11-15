/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "application_config.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>

namespace app {
namespace {

std::string trim(std::string value) {
    const auto is_not_space = [](unsigned char character) { return !std::isspace(character); };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), is_not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), is_not_space).base(), value.end());
    return value;
}

std::string lowercase(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return value;
}

bool to_bool(const std::string& value) {
    const auto v = lowercase(trim(value));
    return v == "yes" || v == "true" || v == "1" || v == "on";
}

// Value after the first '=', with a trailing CR stripped but inner spaces kept
// (so a quick message keeps its exact text).
std::string raw_value(const std::string& line, std::size_t sep) {
    std::string v = line.substr(sep + 1);
    if (!v.empty() && v.back() == '\r') v.pop_back();
    return v;
}

} // namespace

bool load_application_config(const std::string& path,
                             ApplicationConfig& config,
                             std::string& error) {
    error.clear();
    std::ifstream input(path);
    if (!input) {
        error = "could not open file";
        return false;
    }

    std::string section;
    std::string line;
    while (std::getline(input, line)) {
        std::string t = trim(line);
        if (t.empty() || t.front() == '#' || t.front() == ';') continue;
        if (t.front() == '[' && t.back() == ']') {
            section = lowercase(trim(t.substr(1, t.size() - 2)));
            if (section == "messaging") config.has_messaging = true;
            continue;
        }
        const auto sep = line.find('=');
        if (sep == std::string::npos) continue;
        const std::string key = lowercase(trim(line.substr(0, sep)));

        if (section == "application") {
            if (key == "dark_mode") config.dark_mode = to_bool(line.substr(sep + 1));
        } else if (section == "messaging") {
            if (key == "show_signal") config.show_signal = to_bool(line.substr(sep + 1));
            else if (key == "retention") config.retention_cap = std::atoi(trim(line.substr(sep + 1)).c_str());
            else if (key == "mute_all") config.mute_all = to_bool(line.substr(sep + 1));
            else if (key == "want_ack") config.want_ack = to_bool(line.substr(sep + 1));
            else if (key == "auto_start") config.auto_start = to_bool(line.substr(sep + 1));
            else if (key == "canned") config.canned.push_back(raw_value(line, sep));
            else if (key == "muted") config.muted.push_back(raw_value(line, sep));
            else if (key == "favorite") config.favorites.push_back(raw_value(line, sep));
        }
    }
    return true;
}

bool save_application_config(const std::string& path,
                             const ApplicationConfig& config,
                             std::string& error) {
    error.clear();
    const std::filesystem::path config_path(path);
    const auto parent = config_path.parent_path();
    if (!parent.empty()) {
        std::error_code ec;
        std::filesystem::create_directories(parent, ec);
        if (ec) { error = "could not create parent directory: " + ec.message(); return false; }
    }

    // Write to a temp file and atomically rename, so a power loss mid-write can
    // never leave a truncated or empty config behind.
    const std::string tmp = path + ".tmp";
    {
        std::ofstream out(tmp, std::ios::trunc);
        if (!out) { error = "could not open file for writing"; return false; }

        out << "[application]\n";
        out << "dark_mode=" << (config.dark_mode ? "yes" : "no") << "\n\n";

        out << "[messaging]\n";
        out << "show_signal=" << (config.show_signal ? "yes" : "no") << "\n";
        out << "retention=" << config.retention_cap << "\n";
        out << "mute_all=" << (config.mute_all ? "yes" : "no") << "\n";
        out << "want_ack=" << (config.want_ack ? "yes" : "no") << "\n";
        out << "auto_start=" << (config.auto_start ? "yes" : "no") << "\n";
        for (const auto& c : config.canned) out << "canned=" << c << "\n";
        for (const auto& m : config.muted) out << "muted=" << m << "\n";
        for (const auto& f : config.favorites) out << "favorite=" << f << "\n";
        out.flush();
        if (!out) { error = "write failed"; return false; }
    }
    std::error_code rename_ec;
    std::filesystem::rename(tmp, path, rename_ec);
    if (rename_ec) { error = "rename failed: " + rename_ec.message(); return false; }
    return true;
}

} // namespace app
