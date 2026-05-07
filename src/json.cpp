#include "ote/json.hpp"

#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace ote {
namespace {

void skip_ws(const std::string& text, std::size_t& pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0) {
        ++pos;
    }
}

bool hex_value(char c, unsigned int& value) {
    if (c >= '0' && c <= '9') {
        value = static_cast<unsigned int>(c - '0');
        return true;
    }
    if (c >= 'a' && c <= 'f') {
        value = static_cast<unsigned int>(10 + (c - 'a'));
        return true;
    }
    if (c >= 'A' && c <= 'F') {
        value = static_cast<unsigned int>(10 + (c - 'A'));
        return true;
    }
    return false;
}

void append_utf8(std::string& out, unsigned int codepoint) {
    if (codepoint <= 0x7F) {
        out.push_back(static_cast<char>(codepoint));
        return;
    }
    if (codepoint <= 0x7FF) {
        out.push_back(static_cast<char>(0xC0 | ((codepoint >> 6) & 0x1F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        return;
    }
    if (codepoint <= 0xFFFF) {
        out.push_back(static_cast<char>(0xE0 | ((codepoint >> 12) & 0x0F)));
        out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
        return;
    }
    out.push_back(static_cast<char>(0xF0 | ((codepoint >> 18) & 0x07)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F)));
    out.push_back(static_cast<char>(0x80 | (codepoint & 0x3F)));
}

bool parse_string(const std::string& text, std::size_t& pos, std::string& out, std::string& error) {
    if (pos >= text.size() || text[pos] != '"') {
        error = "expected string";
        return false;
    }

    ++pos;
    std::string value;
    while (pos < text.size()) {
        const char c = text[pos++];
        if (c == '"') {
            out = std::move(value);
            return true;
        }
        if (c == '\\') {
            if (pos >= text.size()) {
                error = "unterminated escape sequence";
                return false;
            }
            const char escaped = text[pos++];
            switch (escaped) {
            case '"': value.push_back('"'); break;
            case '\\': value.push_back('\\'); break;
            case '/': value.push_back('/'); break;
            case 'b': value.push_back('\b'); break;
            case 'f': value.push_back('\f'); break;
            case 'n': value.push_back('\n'); break;
            case 'r': value.push_back('\r'); break;
            case 't': value.push_back('\t'); break;
            case 'u': {
                if (pos + 4 > text.size()) {
                    error = "invalid unicode escape";
                    return false;
                }
                unsigned int codepoint = 0;
                for (int i = 0; i < 4; ++i) {
                    unsigned int nibble = 0;
                    if (!hex_value(text[pos + static_cast<std::size_t>(i)], nibble)) {
                        error = "invalid unicode escape";
                        return false;
                    }
                    codepoint = (codepoint << 4) | nibble;
                }
                pos += 4;
                append_utf8(value, codepoint);
                break;
            }
            default:
                error = "invalid escape sequence";
                return false;
            }
            continue;
        }
        value.push_back(c);
    }

    error = "unterminated string";
    return false;
}

bool parse_number(const std::string& text, std::size_t& pos, double& out, std::string& error) {
    const std::size_t begin = pos;
    if (pos < text.size() && (text[pos] == '-' || text[pos] == '+')) {
        ++pos;
    }
    bool saw_digit = false;
    while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])) != 0) {
        saw_digit = true;
        ++pos;
    }
    if (pos < text.size() && text[pos] == '.') {
        ++pos;
        while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])) != 0) {
            saw_digit = true;
            ++pos;
        }
    }
    if (!saw_digit) {
        error = "invalid number";
        return false;
    }
    if (pos < text.size() && (text[pos] == 'e' || text[pos] == 'E')) {
        ++pos;
        if (pos < text.size() && (text[pos] == '+' || text[pos] == '-')) {
            ++pos;
        }
        bool saw_exponent_digit = false;
        while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])) != 0) {
            saw_exponent_digit = true;
            ++pos;
        }
        if (!saw_exponent_digit) {
            error = "invalid number exponent";
            return false;
        }
    }
    try {
        out = std::stod(text.substr(begin, pos - begin));
        return true;
    } catch (...) {
        error = "invalid number";
        return false;
    }
}

bool parse_value(const std::string& text, std::size_t& pos, JsonValue& value, std::string& error);

bool parse_array(const std::string& text, std::size_t& pos, JsonValue& value, std::string& error) {
    if (pos >= text.size() || text[pos] != '[') {
        error = "expected array";
        return false;
    }

    ++pos;
    value.kind = JsonValue::Kind::Array;
    value.array.clear();
    skip_ws(text, pos);
    if (pos < text.size() && text[pos] == ']') {
        ++pos;
        return true;
    }

    while (pos < text.size()) {
        JsonValue element;
        if (!parse_value(text, pos, element, error)) {
            return false;
        }
        value.array.push_back(std::move(element));
        skip_ws(text, pos);
        if (pos >= text.size()) {
            error = "unterminated array";
            return false;
        }
        if (text[pos] == ',') {
            ++pos;
            skip_ws(text, pos);
            continue;
        }
        if (text[pos] == ']') {
            ++pos;
            return true;
        }
        error = "invalid array separator";
        return false;
    }

    error = "unterminated array";
    return false;
}

bool parse_object(const std::string& text, std::size_t& pos, JsonValue& value, std::string& error) {
    if (pos >= text.size() || text[pos] != '{') {
        error = "expected object";
        return false;
    }

    ++pos;
    value.kind = JsonValue::Kind::Object;
    value.object.clear();
    skip_ws(text, pos);
    if (pos < text.size() && text[pos] == '}') {
        ++pos;
        return true;
    }

    while (pos < text.size()) {
        std::string key;
        if (!parse_string(text, pos, key, error)) {
            return false;
        }
        skip_ws(text, pos);
        if (pos >= text.size() || text[pos] != ':') {
            error = "expected object separator";
            return false;
        }
        ++pos;
        skip_ws(text, pos);
        JsonValue entry;
        if (!parse_value(text, pos, entry, error)) {
            return false;
        }
        value.object.emplace_back(std::move(key), std::move(entry));
        skip_ws(text, pos);
        if (pos >= text.size()) {
            error = "unterminated object";
            return false;
        }
        if (text[pos] == ',') {
            ++pos;
            skip_ws(text, pos);
            continue;
        }
        if (text[pos] == '}') {
            ++pos;
            return true;
        }
        error = "invalid object separator";
        return false;
    }

    error = "unterminated object";
    return false;
}

bool parse_value(const std::string& text, std::size_t& pos, JsonValue& value, std::string& error) {
    skip_ws(text, pos);
    if (pos >= text.size()) {
        error = "unexpected end of input";
        return false;
    }

    const char c = text[pos];
    if (c == '"') {
        value.kind = JsonValue::Kind::String;
        return parse_string(text, pos, value.string, error);
    }
    if (c == '{') {
        return parse_object(text, pos, value, error);
    }
    if (c == '[') {
        return parse_array(text, pos, value, error);
    }
    if (c == 't' && text.compare(pos, 4, "true") == 0) {
        pos += 4;
        value.kind = JsonValue::Kind::Bool;
        value.boolean = true;
        return true;
    }
    if (c == 'f' && text.compare(pos, 5, "false") == 0) {
        pos += 5;
        value.kind = JsonValue::Kind::Bool;
        value.boolean = false;
        return true;
    }
    if (c == 'n' && text.compare(pos, 4, "null") == 0) {
        pos += 4;
        value.kind = JsonValue::Kind::Null;
        return true;
    }

    value.kind = JsonValue::Kind::Number;
    return parse_number(text, pos, value.number, error);
}

void stringify_string(const std::string& value, std::ostringstream& out) {
    out << '"';
    for (const char c : value) {
        switch (c) {
        case '\\': out << "\\\\"; break;
        case '"': out << "\\\""; break;
        case '\b': out << "\\b"; break;
        case '\f': out << "\\f"; break;
        case '\n': out << "\\n"; break;
        case '\r': out << "\\r"; break;
        case '\t': out << "\\t"; break;
        default:
            if (static_cast<unsigned char>(c) < 0x20U) {
                out << "\\u";
                out << std::hex << std::setw(4) << std::setfill('0') << static_cast<int>(static_cast<unsigned char>(c));
                out << std::dec << std::setfill(' ');
            } else {
                out << c;
            }
            break;
        }
    }
    out << '"';
}

void stringify_value(const JsonValue& value, std::ostringstream& out) {
    switch (value.kind) {
    case JsonValue::Kind::Null:
        out << "null";
        return;
    case JsonValue::Kind::Bool:
        out << (value.boolean ? "true" : "false");
        return;
    case JsonValue::Kind::Number:
        if (std::isfinite(value.number)) {
            out << std::setprecision(17) << value.number;
        } else {
            out << "null";
        }
        return;
    case JsonValue::Kind::String:
        stringify_string(value.string, out);
        return;
    case JsonValue::Kind::Array:
        out << '[';
        for (std::size_t i = 0; i < value.array.size(); ++i) {
            if (i > 0) {
                out << ',';
            }
            stringify_value(value.array[i], out);
        }
        out << ']';
        return;
    case JsonValue::Kind::Object:
        out << '{';
        for (std::size_t i = 0; i < value.object.size(); ++i) {
            if (i > 0) {
                out << ',';
            }
            stringify_string(value.object[i].first, out);
            out << ':';
            stringify_value(value.object[i].second, out);
        }
        out << '}';
        return;
    }
}

}

bool parse_json(const std::string& text, JsonValue& value, std::string& error) {
    std::size_t pos = 0;
    if (!parse_value(text, pos, value, error)) {
        return false;
    }
    skip_ws(text, pos);
    if (pos != text.size()) {
        error = "unexpected trailing data";
        return false;
    }
    return true;
}

std::string stringify_json(const JsonValue& value) {
    std::ostringstream out;
    stringify_value(value, out);
    return out.str();
}

JsonValue* json_object_get(JsonValue& value, const std::string& key) {
    if (value.kind != JsonValue::Kind::Object) {
        return nullptr;
    }
    for (auto& entry : value.object) {
        if (entry.first == key) {
            return &entry.second;
        }
    }
    return nullptr;
}

const JsonValue* json_object_get(const JsonValue& value, const std::string& key) {
    if (value.kind != JsonValue::Kind::Object) {
        return nullptr;
    }
    for (const auto& entry : value.object) {
        if (entry.first == key) {
            return &entry.second;
        }
    }
    return nullptr;
}

JsonValue& json_object_set(JsonValue& value, const std::string& key) {
    if (value.kind != JsonValue::Kind::Object) {
        value.kind = JsonValue::Kind::Object;
        value.object.clear();
    }
    for (auto& entry : value.object) {
        if (entry.first == key) {
            return entry.second;
        }
    }
    value.object.emplace_back(key, JsonValue{});
    return value.object.back().second;
}

}
