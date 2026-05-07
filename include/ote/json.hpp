#pragma once

#include <string>
#include <utility>
#include <vector>

namespace ote {

struct JsonValue {
    enum class Kind {
        Null,
        Bool,
        Number,
        String,
        Array,
        Object
    };

    using Array = std::vector<JsonValue>;
    using Object = std::vector<std::pair<std::string, JsonValue>>;

    Kind kind = Kind::Null;
    bool boolean = false;
    double number = 0.0;
    std::string string;
    Array array;
    Object object;
};

bool parse_json(const std::string& text, JsonValue& value, std::string& error);
std::string stringify_json(const JsonValue& value);
JsonValue* json_object_get(JsonValue& value, const std::string& key);
const JsonValue* json_object_get(const JsonValue& value, const std::string& key);
JsonValue& json_object_set(JsonValue& value, const std::string& key);

}

