// SPDX-License-Identifier: MIT
#pragma once
#include <string>

namespace view {
// Bound previews in bytes without cutting a multibyte UTF-8 character in half.
inline std::string preview_text(const std::string &text, std::size_t max_bytes) {
    if (text.size() <= max_bytes) return text;
    std::size_t end = max_bytes;
    while (end && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) --end;
    return text.substr(0, end) + "\xE2\x80\xA6";
}
} // namespace view
