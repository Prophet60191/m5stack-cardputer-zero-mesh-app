/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 * SPDX-FileCopyrightText: 2026 Jose Di Lenola
 * SPDX-License-Identifier: MIT
 *
 * Symbol names/codes: M5Stack launcher cp0_keyboard_keymap.c, revision
 * 4c05b444cb977cb90e2f33eb4c8261226e370713. Also supplied on the device as
 * /usr/share/keymaps/tca8418_keypad_m5stack_keymap.map. See docs/keyboard.md.
 */
#include "keyboard_decoder.h"
#include <charconv>

namespace platform {
namespace {
struct Symbol { uint16_t code; std::string_view name; char character; };
constexpr std::array<Symbol, 32> symbols{{
    {26,"exclam",'!'}, {27,"at",'@'}, {39,"numbersign",'#'}, {40,"dollar",'$'},
    {41,"percent",'%'}, {43,"asciicircum",'^'}, {51,"ampersand",'&'}, {52,"asterisk",'*'},
    {53,"parenleft",'('}, {94,"parenright",')'}, {55,"asciitilde",'~'}, {69,"grave",'`'},
    {70,"underscore",'_'}, {71,"minus",'-'}, {72,"plus",'+'}, {73,"equal",'='},
    {74,"bracketleft",'['}, {75,"bracketright",']'}, {76,"braceleft",'{'}, {77,"braceright",'}'},
    {79,"semicolon",';'}, {80,"colon",':'}, {81,"apostrophe",'\''}, {82,"quotedbl",'"'},
    {83,"less",'<'}, {85,"greater",'>'}, {86,"backslash",'\\'}, {89,"bar",'|'},
    {90,"comma",','}, {91,"period",'.'}, {92,"slash",'/'}, {93,"question",'?'},
}};

// Linux input-event-codes.h values. Kept portable so macOS tests exercise the
// very same device decoder, rather than compiling it out behind USE_DESKTOP.
uint32_t us_character(uint16_t code, bool shift) {
    constexpr char number_row[] = "1234567890";
    constexpr char shifted_row[] = "!@#$%^&*()";
    if (code >= 2 && code <= 11) return shift ? shifted_row[code-2] : number_row[code-2];
    struct Row { uint16_t first; std::string_view letters; };
    constexpr Row rows[] = {{16,"qwertyuiop"},{30,"asdfghjkl"},{44,"zxcvbnm"}};
    for (const auto& row : rows) {
        if (code >= row.first && code-row.first < row.letters.size()) {
            const char c = row.letters[code-row.first];
            return shift ? c-'a'+'A' : c;
        }
    }
    switch (code) {
        case 57: return ' '; // KEY_SPACE
        case 12: return shift ? '_' : '-';
        case 13: return shift ? '+' : '=';
        case 26: return shift ? '{' : '[';
        case 27: return shift ? '}' : ']';
        case 43: return shift ? '|' : '\\';
        case 39: return shift ? ':' : ';';
        case 40: return shift ? '"' : '\'';
        case 41: return shift ? '~' : '`';
        case 51: return shift ? '<' : ',';
        case 52: return shift ? '>' : '.';
        case 53: return shift ? '?' : '/';
        default: return 0;
    }
}

std::string_view trim(std::string_view value) {
    constexpr std::string_view space = " \t\r";
    const auto begin = value.find_first_not_of(space);
    if (begin == value.npos) return {};
    return value.substr(begin, value.find_last_not_of(space)-begin+1);
}
} // namespace

KeyboardDecoder::KeyboardDecoder(Profile profile) : profile_(profile) {
    for (size_t i=0;i<symbols.size();++i) symbols_[i]=symbols[i].character;
}

bool KeyboardDecoder::is_cardputer(std::string_view name) {
    return name == "tca8418c" || name == "tca8418";
}

void KeyboardDecoder::reset() {
    active_.fill(0);
    left_shift_=right_shift_=left_ctrl_=right_ctrl_=left_alt_=right_alt_=false;
}

void KeyboardDecoder::restore_modifiers(bool left_shift, bool right_shift, bool left_ctrl,
                                        bool right_ctrl, bool left_alt, bool right_alt) {
    left_shift_=left_shift; right_shift_=right_shift;
    left_ctrl_=left_ctrl; right_ctrl_=right_ctrl;
    left_alt_=left_alt; right_alt_=right_alt;
}

bool KeyboardDecoder::load_symbol_map(std::string_view source, std::string& error) {
    error.clear();
    const auto fail = [&error](const char* why) { error=why; return false; };
    if (source.empty() || source.size()>8192 || source.find('\0')!=source.npos)
        return fail("symbol map is empty, oversized or contains NUL");
    std::array<char,32> candidate{};
    while (!source.empty()) {
        const auto end=source.find('\n');
        auto line=source.substr(0,end);
        source=end==source.npos ? std::string_view{} : source.substr(end+1);
        line=trim(line.substr(0,line.find('#')));
        if (line.empty()) continue;
        if (line.substr(0,8)!="keycode ") return fail("invalid keycode record");
        line=trim(line.substr(8));
        uint16_t code=0;
        const auto parsed=std::from_chars(line.data(),line.data()+line.size(),code);
        if (parsed.ec!=std::errc{}) return fail("invalid symbol code");
        line=trim(line.substr(static_cast<size_t>(parsed.ptr-line.data())));
        if (line.empty() || line.front()!='=') return fail("missing symbol assignment");
        const auto name=trim(line.substr(1));
        size_t index=symbols.size(); char character=0;
        for (size_t i=0;i<symbols.size();++i) {
            if (symbols[i].code==code) index=i;
            if (symbols[i].name==name) character=symbols[i].character;
        }
        if (index==symbols.size() || !character) return fail("unsupported symbol code or name");
        if (candidate[index]) return fail("duplicate symbol code");
        candidate[index]=character;
    }
    for (char c : candidate) if (!c) return fail("incomplete symbol map");
    symbols_=candidate;
    return true;
}

uint32_t KeyboardDecoder::translate(uint16_t code, bool text_entry) const {
    // The built-in driver has ALREADY selected the symbol-layer keycode. Do not
    // gate this lookup on a physical Sym-down event: it also covers one-shot,
    // locked and already-active-before-launch modes. Never apply it to USB keys.
    if (profile_==Profile::CardputerZero) {
        for (size_t i=0;i<symbols.size();++i) if (symbols[i].code==code) return symbols_[i];
    }
    switch (code) {
        case 1: return LV_KEY_ESC;
        case 28: case 96: return LV_KEY_ENTER;
        case 103: return LV_KEY_UP;
        case 108: return LV_KEY_DOWN;
        case 105: return LV_KEY_LEFT;
        case 106: return LV_KEY_RIGHT;
        case 14: return LV_KEY_BACKSPACE;
        case 111: return LV_KEY_DEL;
        case 15: return LV_KEY_NEXT;
        case 102: return LV_KEY_HOME;
        case 107: return LV_KEY_END;
        case 104: return kKeyPageUp;
        case 109: return kKeyPageDown;
        case 110: return kKeyInsert;
        case 99: return kKeyPrintScreen;
        case 138: return kKeyHelp;
        // Delivered as semantic non-text events; apps may bind them deliberately.
        case 113: case 114: case 115: case 163: case 164: case 165:
        case 224: case 225: return kKeySystemAction+code;
        default: break;
    }
    if (code>=59 && code<=68) return kKeyFunction1+code-59;
    if (code==87 || code==88) return kKeyFunction1+10+code-87;
    if (left_ctrl_ || right_ctrl_ || left_alt_ || right_alt_) return 0; // unbound chords are not text
    if (!text_entry) {
        switch (code) {
            case 33: return LV_KEY_UP; case 45: return LV_KEY_DOWN;
            case 44: return LV_KEY_LEFT; case 46: return LV_KEY_RIGHT;
            default: break;
        }
    }
    return us_character(code,left_shift_ || right_shift_);
}

std::optional<KeyboardDecoder::Event> KeyboardDecoder::feed(uint16_t code, int value, bool text_entry) {
    if (code>=active_.size() || value<0 || value>2) return {};
    if (code==42 || code==54 || code==29 || code==97 || code==56 || code==100) {
        if (code==42) left_shift_=value!=0;
        if (code==54) right_shift_=value!=0;
        if (code==29) left_ctrl_=value!=0;
        if (code==97) right_ctrl_=value!=0;
        if (code==56) left_alt_=value!=0;
        if (code==100) right_alt_=value!=0;
        return {};
    }
    // Physical Fn/Sym events are not text; symbol codes and Fn control codes
    // already describe their output. Both legacy and current modifier codes.
    if (code==127 || code==383 || code==389 || code==464) return {};
    if (value==0) {
        const auto key=active_[code]; active_[code]=0;
        if (key) return Event{key,false,false};
        return {};
    }
    if (value==2 || active_[code]) { // LVGL owns held-key repeat timing.
        if (active_[code]) return Event{active_[code],true,true};
        return {};
    }
    const auto key=translate(code,text_entry);
    if (!key) return {};
    active_[code]=key;
    return Event{key,true,false};
}
} // namespace platform
