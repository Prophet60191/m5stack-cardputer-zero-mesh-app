/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jose Di Lenola
 */
#pragma once

#include "lvgl.h"
#include <array>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

namespace platform {

constexpr uint32_t kSpecialKeyBase = 0x200000U;
constexpr uint32_t kKeyPrintScreen = kSpecialKeyBase | 18U;
constexpr uint32_t kKeyHelp = kSpecialKeyBase | 19U;
constexpr uint32_t kKeyPageUp = kSpecialKeyBase | 20U;
constexpr uint32_t kKeyPageDown = kSpecialKeyBase | 21U;
constexpr uint32_t kKeyInsert = kSpecialKeyBase | 22U;
constexpr uint32_t kKeyFunction1 = kSpecialKeyBase | 32U;
constexpr uint32_t kKeySystemAction = kSpecialKeyBase | 64U;
constexpr bool is_action_key(uint32_t key) { return (key & 0xfffff000U) == kSpecialKeyBase; }

// A UI-independent evdev decoder (LVGL key constants only; no objects, FDs,
// timers, globals, or hardware calls). Reuse this module with its regression
// fixture rather than writing another character table for each application.
class KeyboardDecoder {
public:
    enum class Profile { CardputerZero, UsKeyboard };
    struct Event { uint32_t key; bool pressed; bool repeat; };
    explicit KeyboardDecoder(Profile profile = Profile::CardputerZero);
    std::optional<Event> feed(uint16_t code, int value, bool text_entry);
    // Atomic, bounded parse of the complete 32-entry system symbol map. Failure
    // leaves the previously valid map intact. The caller owns trusted file I/O.
    bool load_symbol_map(std::string_view source, std::string& error);
    void reset();
    void restore_modifiers(bool left_shift, bool right_shift, bool left_ctrl=false,
                           bool right_ctrl=false, bool left_alt=false, bool right_alt=false);
    static bool is_cardputer(std::string_view evdev_name);

private:
    uint32_t translate(uint16_t code, bool text_entry) const;
    Profile profile_;
    std::array<char, 32> symbols_{};
    std::array<uint32_t, 768> active_{};
    bool left_shift_{false}, right_shift_{false}, left_ctrl_{false}, right_ctrl_{false};
    bool left_alt_{false}, right_alt_{false};
};

} // namespace platform
