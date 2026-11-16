/*
 * SPDX-License-Identifier: MIT
 * Copyright (c) 2026 Jose Di Lenola
 *
 * A tiny JSON reader for the supervisor's event lines, and a string escaper for
 * building intent lines. The supervisor emits well-formed compact JSON
 * (Python json.dumps), so this handles the subset we actually receive:
 * objects, arrays, strings (with escapes), numbers, true/false/null.
 */
#pragma once

#include <map>
#include <cmath>
#include <limits>
#include <cstdio>
#include <string>
#include <vector>

namespace json {

struct Value {
    enum Type { Null, Bool, Num, Str, Arr, Obj } type = Null;
    bool b = false;
    double num = 0.0;
    std::string str;
    std::vector<Value> arr;
    std::map<std::string, Value> obj;

    bool is_obj() const { return type == Obj; }
    bool is_arr() const { return type == Arr; }

    const Value* find(const std::string& key) const {
        auto it = obj.find(key);
        return it == obj.end() ? nullptr : &it->second;
    }
    std::string as_str(const std::string& def = "") const { return type == Str ? str : def; }
    long as_int(long def = 0) const {
        return type == Num && std::isfinite(num) &&
               num >= static_cast<double>(std::numeric_limits<long>::min()) &&
               num < static_cast<double>(std::numeric_limits<long>::max())
                   ? static_cast<long>(num) : def;
    }
    int as_int32(int def = 0) const {
        return type == Num && std::isfinite(num) &&
               num >= std::numeric_limits<int>::min() && num <= std::numeric_limits<int>::max()
                   ? static_cast<int>(num) : def;
    }
    size_t memory_size() const {
        size_t n = sizeof(Value) + str.capacity();
        for (const auto& v : arr) n += v.memory_size();
        for (const auto& kv : obj) n += 64 + kv.first.capacity() + kv.second.memory_size();
        return n;
    }
    bool as_bool(bool def = false) const { return type == Bool ? b : def; }

    std::string str_at(const std::string& key, const std::string& def = "") const {
        const Value* v = find(key);
        return v ? v->as_str(def) : def;
    }
    long int_at(const std::string& key, long def = 0) const {
        const Value* v = find(key);
        return v ? v->as_int(def) : def;
    }
};

namespace detail {

class Parser {
public:
    explicit Parser(const std::string& s) : s_(s) {}

    bool parse(Value& out) {
        out = Value{};
        skip_ws();
        if (!value(out)) return false;
        skip_ws();
        return i_ == s_.size();
    }

private:
    const std::string& s_;
    size_t i_ = 0;
    int depth_ = 0;
    size_t nodes_ = 0;
    static constexpr int kMaxDepth = 64;   // reject pathologically nested input

    void skip_ws() {
        while (i_ < s_.size() && (s_[i_] == ' ' || s_[i_] == '\t' || s_[i_] == '\n' || s_[i_] == '\r')) ++i_;
    }
    bool eof() const { return i_ >= s_.size(); }
    char cur() const { return s_[i_]; }

    bool value(Value& out) {
        skip_ws();
        if (eof() || ++nodes_ > 50000) return false;
        if (++depth_ > kMaxDepth) { --depth_; return false; }
        bool ok;
        switch (cur()) {
            case '{': ok = object(out); break;
            case '[': ok = array(out); break;
            case '"': out.type = Value::Str; ok = string(out.str); break;
            case 't': case 'f': ok = boolean(out); break;
            case 'n': ok = null_literal(out); break;
            default:  ok = number(out); break;
        }
        --depth_;
        return ok;
    }

    bool object(Value& out) {
        out.type = Value::Obj;
        ++i_;  // {
        skip_ws();
        if (!eof() && cur() == '}') { ++i_; return true; }
        while (!eof()) {
            skip_ws();
            if (eof() || cur() != '"') return false;
            std::string key;
            if (!string(key)) return false;
            skip_ws();
            if (eof() || cur() != ':') return false;
            ++i_;
            Value v;
            if (!value(v)) return false;
            if (!out.obj.emplace(key, std::move(v)).second) return false;
            skip_ws();
            if (eof()) return false;
            if (cur() == ',') { ++i_; continue; }
            if (cur() == '}') { ++i_; return true; }
            return false;
        }
        return false;
    }

    bool array(Value& out) {
        out.type = Value::Arr;
        ++i_;  // [
        skip_ws();
        if (!eof() && cur() == ']') { ++i_; return true; }
        while (!eof()) {
            Value v;
            if (!value(v)) return false;
            out.arr.push_back(std::move(v));
            skip_ws();
            if (eof()) return false;
            if (cur() == ',') { ++i_; continue; }
            if (cur() == ']') { ++i_; return true; }
            return false;
        }
        return false;
    }

    static bool valid_utf8(const std::string& s) {
        size_t i = 0;
        while (i < s.size()) {
            const unsigned char lead = static_cast<unsigned char>(s[i++]);
            if (lead < 0x80) continue;
            unsigned cp, count, minimum;
            if (lead >= 0xC2 && lead <= 0xDF) { cp = lead & 0x1F; count = 1; minimum = 0x80; }
            else if (lead >= 0xE0 && lead <= 0xEF) { cp = lead & 0x0F; count = 2; minimum = 0x800; }
            else if (lead >= 0xF0 && lead <= 0xF4) { cp = lead & 0x07; count = 3; minimum = 0x10000; }
            else return false;
            if (i + count > s.size()) return false;
            while (count--) {
                const auto byte = static_cast<unsigned char>(s[i++]);
                if ((byte & 0xC0) != 0x80) return false;
                cp = (cp << 6) | (byte & 0x3F);
            }
            if (cp < minimum || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return false;
        }
        return true;
    }

    bool string(std::string& out) {
        out.clear();
        ++i_;  // opening "
        while (!eof()) {
            if (out.size() > 4096) return false;
            char c = s_[i_++];
            if (static_cast<unsigned char>(c) < 0x20) return false;
            if (c == '"') return valid_utf8(out);
            if (c == '\\') {
                if (eof()) return false;
                char e = s_[i_++];
                switch (e) {
                    case '"':  out.push_back('"');  break;
                    case '\\': out.push_back('\\'); break;
                    case '/':  out.push_back('/');  break;
                    case 'n':  out.push_back('\n'); break;
                    case 't':  out.push_back('\t'); break;
                    case 'r':  out.push_back('\r'); break;
                    case 'b':  out.push_back('\b'); break;
                    case 'f':  out.push_back('\f'); break;
                    case 'u': {
                        unsigned cp = 0;
                        if (!read_hex4(cp)) return false;
                        // Combine a UTF-16 surrogate pair into one code point;
                        // emit U+FFFD for an unpaired/invalid surrogate.
                        if (cp >= 0xD800 && cp <= 0xDBFF) {
                            if (i_ + 2 <= s_.size() && s_[i_] == '\\' && s_[i_ + 1] == 'u') {
                                i_ += 2;
                                unsigned lo = 0;
                                if (!read_hex4(lo)) return false;
                                if (lo >= 0xDC00 && lo <= 0xDFFF) {
                                    cp = 0x10000u + ((cp - 0xD800u) << 10) + (lo - 0xDC00u);
                                } else {
                                    append_utf8(out, 0xFFFD);
                                    cp = (lo >= 0xD800 && lo <= 0xDFFF) ? 0xFFFD : lo;
                                }
                            } else {
                                cp = 0xFFFD;
                            }
                        } else if (cp >= 0xDC00 && cp <= 0xDFFF) {
                            cp = 0xFFFD;
                        }
                        append_utf8(out, cp);
                        break;
                    }
                    default: return false;
                }
            } else {
                out.push_back(c);
            }
        }
        return false;  // unterminated
    }

    bool read_hex4(unsigned& cp) {
        if (i_ + 4 > s_.size()) return false;
        cp = 0;
        for (int k = 0; k < 4; ++k) {
            char h = s_[i_++];
            cp <<= 4;
            if (h >= '0' && h <= '9') cp |= static_cast<unsigned>(h - '0');
            else if (h >= 'a' && h <= 'f') cp |= static_cast<unsigned>(h - 'a' + 10);
            else if (h >= 'A' && h <= 'F') cp |= static_cast<unsigned>(h - 'A' + 10);
            else return false;
        }
        return true;
    }

    static void append_utf8(std::string& out, unsigned cp) {
        if (cp < 0x80) {
            out.push_back(static_cast<char>(cp));
        } else if (cp < 0x800) {
            out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else if (cp < 0x10000) {
            out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        } else {
            out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
            out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
        }
    }

    bool boolean(Value& out) {
        if (s_.compare(i_, 4, "true") == 0) { out.type = Value::Bool; out.b = true; i_ += 4; return true; }
        if (s_.compare(i_, 5, "false") == 0) { out.type = Value::Bool; out.b = false; i_ += 5; return true; }
        return false;
    }

    bool null_literal(Value& out) {
        if (s_.compare(i_, 4, "null") == 0) { out.type = Value::Null; i_ += 4; return true; }
        return false;
    }

    bool number(Value& out) {
        const size_t start = i_;
        if (!eof() && cur() == '-') ++i_;
        if (eof()) return false;
        auto digit = [this] { return !eof() && cur() >= '0' && cur() <= '9'; };
        if (cur() == '0') ++i_;
        else {
            if (!digit()) return false;
            while (digit()) ++i_;
        }
        if (!eof() && cur() == '.') {
            ++i_; if (!digit()) return false;
            while (digit()) ++i_;
        }
        if (!eof() && (cur() == 'e' || cur() == 'E')) {
            ++i_;
            if (!eof() && (cur() == '+' || cur() == '-')) ++i_;
            if (!digit()) return false;
            while (digit()) ++i_;
        }
        try {
            out.type = Value::Num;
            out.num = std::stod(s_.substr(start, i_ - start));
        } catch (...) { return false; }
        return std::isfinite(out.num);
    }

};

}  // namespace detail

inline bool parse(const std::string& text, Value& out) {
    return detail::Parser(text).parse(out);
}

// Escape a string for embedding in an intent line we build by hand.
inline std::string escape(const std::string& s) {
    std::string o;
    o.reserve(s.size() + 2);
    for (char c : s) {
        switch (c) {
            case '"':  o += "\\\""; break;
            case '\\': o += "\\\\"; break;
            case '\n': o += "\\n";  break;
            case '\r': o += "\\r";  break;
            case '\t': o += "\\t";  break;
            default:
                if (static_cast<unsigned char>(c) < 0x20) {
                    char escaped[7];
                    std::snprintf(escaped, sizeof(escaped), "\\u%04x", static_cast<unsigned char>(c));
                    o += escaped;
                } else o.push_back(c);
                break;
        }
    }
    return o;
}

}  // namespace json
