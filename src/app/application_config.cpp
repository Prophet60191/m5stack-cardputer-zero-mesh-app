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
#include <sstream>
#include <charconv>
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

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
    std::error_code ec;
    if (!std::filesystem::is_regular_file(path, ec) || std::filesystem::file_size(path, ec) > 256 * 1024 || ec) {
        error = "config must be a regular file no larger than 256 KiB";
        return false;
    }
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
            else if (key == "retention") {
                const auto value = trim(line.substr(sep + 1));
                int cap = 0;
                auto parsed = std::from_chars(value.data(), value.data() + value.size(), cap);
                if (parsed.ec == std::errc{} && parsed.ptr == value.data() + value.size())
                    config.retention_cap = std::clamp(cap, 0, 2000);
            }
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

    // Each writer gets an exclusive 0600 temp file. Sync its contents before
    // atomic replacement, then sync the directory entry; never follow a shared
    // predictable .tmp symlink or interleave two clients' saves.
    std::ostringstream out;
    out << "[application]\n";
    out << "dark_mode=" << (config.dark_mode ? "yes" : "no") << "\n\n";
    out << "[messaging]\n";
    out << "show_signal=" << (config.show_signal ? "yes" : "no") << "\n";
    out << "retention=" << std::clamp(config.retention_cap, 0, 2000) << "\n";
    out << "mute_all=" << (config.mute_all ? "yes" : "no") << "\n";
    out << "want_ack=" << (config.want_ack ? "yes" : "no") << "\n";
    out << "auto_start=" << (config.auto_start ? "yes" : "no") << "\n";
    auto line_value = [](std::string value) {
        std::replace(value.begin(), value.end(), '\n', ' ');
        std::replace(value.begin(), value.end(), '\r', ' ');
        return value;
    };
    for (const auto& c : config.canned) out << "canned=" << line_value(c) << "\n";
    for (const auto& m : config.muted) out << "muted=" << line_value(m) << "\n";
    for (const auto& f : config.favorites) out << "favorite=" << line_value(f) << "\n";
    const std::string content = out.str();
    if (content.size() > 256 * 1024) { error = "config exceeds 256 KiB"; return false; }
    std::string tmp = path + ".tmp.XXXXXX";
    const int fd = ::mkstemp(tmp.data());
    if (fd < 0) { error = "could not create private temporary config"; return false; }
    size_t offset = 0;
    bool ok = true;
    while (offset < content.size()) {
        const auto n = ::write(fd, content.data() + offset, content.size() - offset);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { ok = false; break; }
        offset += static_cast<size_t>(n);
    }
    if (ok && ::fsync(fd) != 0) ok = false;
    if (::close(fd) != 0) ok = false;
    if (!ok || ::rename(tmp.c_str(), path.c_str()) != 0) {
        ::unlink(tmp.c_str()); error = "config write/sync/rename failed"; return false;
    }
    const std::string directory = parent.empty() ? "." : parent.string();
    const int dir = ::open(directory.c_str(), O_RDONLY);
    if (dir < 0) { error = "config saved, but directory could not be opened for sync"; return false; }
    ok = ::fsync(dir) == 0;
    ::close(dir);
    if (!ok) { error = "config saved, but directory sync failed"; return false; }
    return true;
}

} // namespace app
