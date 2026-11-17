/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */

#pragma once

#include <cstdint>
#include <filesystem>

namespace platform {

struct DeviceStatus {
    bool battery_present{false};
    bool battery_charging{false};
    int32_t battery_percent{-1};
};

DeviceStatus read_device_status();
DeviceStatus read_battery_status(const std::filesystem::path &root);

} // namespace platform
