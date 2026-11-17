/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#include "device_status.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <string>

#ifndef APP_POWER_SUPPLY_ROOT
#define APP_POWER_SUPPLY_ROOT "/sys/class/power_supply"
#endif

namespace platform {
namespace {

std::string trim(std::string value) {
    const auto not_space = [](unsigned char ch) { return std::isspace(ch) == 0; };
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), not_space));
    value.erase(std::find_if(value.rbegin(), value.rend(), not_space).base(), value.end());
    return value;
}

bool read_text_file(const std::filesystem::path& path, std::string& value) {
    std::ifstream file(path);
    if (!file) {
        return false;
    }
    std::getline(file, value);
    value = trim(value);
    return true;
}

int32_t read_int_file(const std::filesystem::path& path, int32_t fallback = -1) {
    std::ifstream file(path);
    int32_t value = fallback;
    return file >> value ? value : fallback;
}

void read_battery(DeviceStatus &status, const std::filesystem::path &root) {
    std::error_code ec;
    if (!std::filesystem::exists(root, ec)) {
        return;
    }

    const std::filesystem::directory_iterator end;
    for (std::filesystem::directory_iterator it(root, ec); !ec && it != end; it.increment(ec)) {
        const auto &entry = *it;
        std::string type;
        if (!read_text_file(entry.path() / "type", type) || type != "Battery") {
            continue;
        }

        status.battery_percent = read_int_file(entry.path() / "capacity");
        status.battery_present = status.battery_percent >= 0;
        std::string battery_state;
        status.battery_charging = read_text_file(entry.path() / "status", battery_state) &&
                                  battery_state == "Charging";
        return;
    }
}

} // namespace

DeviceStatus read_battery_status(const std::filesystem::path &root) {
    DeviceStatus status;
    read_battery(status, root);
    return status;
}

DeviceStatus read_device_status() {
#if USE_DESKTOP
    DeviceStatus status;
    status.battery_present = status.battery_charging = true;
    status.battery_percent = 100;
    return status;
#else
    return read_battery_status(APP_POWER_SUPPLY_ROOT);
#endif
}

} // namespace platform
