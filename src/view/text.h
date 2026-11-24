// SPDX-License-Identifier: MIT
#pragma once
#include <string>

namespace view {
// One-line previews bounded in bytes, without splitting a UTF-8 character.
inline std::string preview_text(const std::string &text, std::size_t max_bytes) {
    const bool truncated = text.size() > max_bytes;
    std::size_t end = truncated ? max_bytes : text.size();
    if (truncated)
        while (end && (static_cast<unsigned char>(text[end]) & 0xc0) == 0x80) --end;
    std::string result = text.substr(0, end);
    for (char &c : result)
        if (static_cast<unsigned char>(c) < 0x20 || c == 0x7f) c = ' ';
    if (truncated) result += "\xE2\x80\xA6";
    return result;
}
} // namespace view
