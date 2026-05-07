#include "ote/config.hpp"

#include "ote/platform.hpp"

#include <filesystem>
#include <fstream>
#include <cctype>
#include <set>
#include <sstream>
#include <string>

namespace ote {
namespace {

void skip_ws(const std::string& text, std::size_t& pos) {
    while (pos < text.size() && std::isspace(static_cast<unsigned char>(text[pos])) != 0) {
        ++pos;
    }
}

bool read_json_string(const std::string& text, std::size_t& pos, std::string& out) {
    if (pos >= text.size() || text[pos] != '"') {
        return false;
    }

    ++pos;
    std::string value;
    while (pos < text.size()) {
        const char c = text[pos++];
        if (c == '"') {
            out = value;
            return true;
        }
        if (c == '\\') {
            if (pos >= text.size()) {
                return false;
            }
            const char escaped = text[pos++];
            switch (escaped) {
            case '"': value += '"'; break;
            case '\\': value += '\\'; break;
            case '/': value += '/'; break;
            case 'b': value += '\b'; break;
            case 'f': value += '\f'; break;
            case 'n': value += '\n'; break;
            case 'r': value += '\r'; break;
            case 't': value += '\t'; break;
            default: return false;
            }
            continue;
        }
        value += c;
    }

    return false;
}

bool read_json_bool(const std::string& text, std::size_t& pos, bool& out) {
    if (text.compare(pos, 4, "true") == 0) {
        pos += 4;
        out = true;
        return true;
    }

    if (text.compare(pos, 5, "false") == 0) {
        pos += 5;
        out = false;
        return true;
    }

    return false;
}

bool read_json_int(const std::string& text, std::size_t& pos, int& out) {
    std::size_t begin = pos;
    if (pos < text.size() && (text[pos] == '-' || text[pos] == '+')) {
        ++pos;
    }
    while (pos < text.size() && std::isdigit(static_cast<unsigned char>(text[pos])) != 0) {
        ++pos;
    }
    if (begin == pos) {
        return false;
    }
    try {
        out = std::stoi(text.substr(begin, pos - begin));
        return true;
    } catch (...) {
        return false;
    }
}

bool read_json_string_array(const std::string& text, std::size_t& pos, std::vector<std::string>& out) {
    if (pos >= text.size() || text[pos] != '[') {
        return false;
    }

    ++pos;
    skip_ws(text, pos);
    out.clear();

    if (pos < text.size() && text[pos] == ']') {
        ++pos;
        return true;
    }

    while (pos < text.size()) {
        std::string value;
        if (!read_json_string(text, pos, value)) {
            return false;
        }
        out.push_back(value);
        skip_ws(text, pos);
        if (pos >= text.size()) {
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
        return false;
    }

    return false;
}

bool capture_enclosed(const std::string& text, std::size_t pos, char open, char close, std::string& out) {
    if (pos >= text.size() || text[pos] != open) {
        return false;
    }

    std::size_t depth = 0;
    bool in_string = false;
    bool escaped = false;
    for (std::size_t i = pos; i < text.size(); ++i) {
        const char c = text[i];
        if (in_string) {
            if (escaped) {
                escaped = false;
                continue;
            }
            if (c == '\\') {
                escaped = true;
                continue;
            }
            if (c == '"') {
                in_string = false;
            }
            continue;
        }
        if (c == '"') {
            in_string = true;
            continue;
        }
        if (c == open) {
            ++depth;
            continue;
        }
        if (c == close) {
            if (depth == 0) {
                return false;
            }
            --depth;
            if (depth == 0) {
                out = text.substr(pos, i - pos + 1);
                return true;
            }
        }
    }

    return false;
}

bool find_field_value(const std::string& object_text, const std::string& field, std::size_t& pos) {
    const std::string needle = "\"" + field + "\"";
    const std::size_t key_pos = object_text.find(needle);
    if (key_pos == std::string::npos) {
        return false;
    }

    pos = object_text.find(':', key_pos + needle.size());
    if (pos == std::string::npos) {
        return false;
    }

    ++pos;
    skip_ws(object_text, pos);
    return pos < object_text.size();
}

bool extract_object_field(const std::string& object_text, const std::string& field, std::string& value) {
    std::size_t pos = 0;
    if (!find_field_value(object_text, field, pos)) {
        return false;
    }
    if (object_text[pos] != '{') {
        return false;
    }
    return capture_enclosed(object_text, pos, '{', '}', value);
}

bool extract_array_field(const std::string& object_text, const std::string& field, std::vector<std::string>& value) {
    std::size_t pos = 0;
    if (!find_field_value(object_text, field, pos)) {
        return false;
    }
    return read_json_string_array(object_text, pos, value);
}

bool extract_bool_field(const std::string& object_text, const std::string& field, bool& value) {
    std::size_t pos = 0;
    if (!find_field_value(object_text, field, pos)) {
        return false;
    }
    return read_json_bool(object_text, pos, value);
}

bool extract_int_field(const std::string& object_text, const std::string& field, int& value) {
    std::size_t pos = 0;
    if (!find_field_value(object_text, field, pos)) {
        return false;
    }
    return read_json_int(object_text, pos, value);
}

bool extract_string_field(const std::string& object_text, const std::string& field, std::string& value) {
    std::size_t pos = 0;
    if (!find_field_value(object_text, field, pos)) {
        return false;
    }
    return read_json_string(object_text, pos, value);
}

std::string escape_json(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (char c : value) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default: out += c; break;
        }
    }
    return out;
}

std::string join_json_array(const std::vector<std::string>& values) {
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << "\"" << escape_json(values[i]) << "\"";
    }
    out << "]";
    return out.str();
}

std::string read_file(const std::filesystem::path& path, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "unable to open config";
        return {};
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

bool write_file(const std::filesystem::path& path, const std::string& text, std::string& error) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = "unable to write config";
        return false;
    }

    file << text;
    if (!file.good()) {
        error = "unable to flush config";
        return false;
    }

    return true;
}

bool is_safe_entry(const std::string& value) {
    return !value.empty();
}

void append_issue(ConfigValidation& validation, const std::string& issue) {
    validation.issues.push_back(issue);
}

std::string make_default_json(const Config& config) {
    std::ostringstream out;
    out << "{\n";
    out << "  \"version\": " << config.version << ",\n";
    out << "  \"sandbox\": {\n";
    out << "    \"enabled\": " << (config.sandbox.enabled ? "true" : "false") << ",\n";
    out << "    \"allowed_paths\": " << join_json_array(config.sandbox.allowed_paths) << ",\n";
    out << "    \"denied_paths\": " << join_json_array(config.sandbox.denied_paths) << ",\n";
    out << "    \"allowed_env\": " << join_json_array(config.sandbox.allowed_env) << "\n";
    out << "  },\n";
    out << "  \"runtime\": {\n";
    out << "    \"default_shell\": \"" << escape_json(config.runtime.default_shell) << "\",\n";
    out << "    \"execution_mode\": \"" << escape_json(config.runtime.execution_mode) << "\",\n";
    out << "    \"cache_dir\": \"" << escape_json(config.runtime.cache_dir) << "\"\n";
    out << "  }\n";
    out << "}\n";
    return out.str();
}

Config default_config(const std::filesystem::path& root) {
    Config config;
    config.sandbox.enabled = true;
    config.sandbox.allowed_paths = { root.string() };
    config.sandbox.denied_paths = {};
    config.sandbox.allowed_env = { "PATH", "SYSTEMROOT", "WINDIR", "HOME", "USERPROFILE", "TMP", "TEMP" };
    config.runtime.default_shell = default_shell();
    config.runtime.execution_mode = "safe";
    config.runtime.cache_dir = ConfigStore::cache_dir(root).string();
    return config;
}

bool ensure_directories(const std::filesystem::path& root, std::string& error) {
    std::error_code ec;
    std::filesystem::create_directories(ConfigStore::store_dir(root), ec);
    if (ec) {
        error = "unable to create store directory";
        return false;
    }

    std::filesystem::create_directories(ConfigStore::vault_dir(root), ec);
    if (ec) {
        error = "unable to create vault directory";
        return false;
    }

    std::filesystem::create_directories(ConfigStore::cache_dir(root), ec);
    if (ec) {
        error = "unable to create cache directory";
        return false;
    }

    std::filesystem::create_directories(ConfigStore::logs_dir(root), ec);
    if (ec) {
        error = "unable to create logs directory";
        return false;
    }

    return true;
}

}

std::filesystem::path ConfigStore::config_path(const std::filesystem::path& root) {
    return root / "ote-config.config";
}

std::filesystem::path ConfigStore::store_dir(const std::filesystem::path& root) {
    return root / ".ote";
}

std::filesystem::path ConfigStore::vault_dir(const std::filesystem::path& root) {
    return store_dir(root) / "vault";
}

std::filesystem::path ConfigStore::state_dir(const std::filesystem::path& root) {
    return store_dir(root) / "state";
}

std::filesystem::path ConfigStore::cache_dir(const std::filesystem::path& root) {
    return store_dir(root) / "cache";
}

std::filesystem::path ConfigStore::logs_dir(const std::filesystem::path& root) {
    return store_dir(root) / "logs";
}

std::filesystem::path ConfigStore::lock_file(const std::filesystem::path& root) {
    return store_dir(root) / "ote.lock";
}

bool ConfigStore::exists(const std::filesystem::path& root) {
    return std::filesystem::exists(config_path(root));
}

bool ConfigStore::ensure_layout(const std::filesystem::path& root, std::string& error) {
    if (!ensure_directories(root, error)) {
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(state_dir(root), ec);
    if (ec) {
        error = "unable to create state directory";
        return false;
    }

    return true;
}

bool ConfigStore::write_default(const std::filesystem::path& root, std::string& error) {
    if (!ensure_layout(root, error)) {
        return false;
    }

    const Config config = default_config(root);
    return save(root, config, error);
}

bool ConfigStore::load(const std::filesystem::path& root, Config& config, std::string& error) {
    const std::filesystem::path path = config_path(root);
    const std::string text = read_file(path, error);
    if (text.empty()) {
        return false;
    }

    config = default_config(root);
    std::string sandbox_text;
    std::string runtime_text;

    if (!extract_int_field(text, "version", config.version)) {
        error = "missing version";
        return false;
    }

    if (!extract_object_field(text, "sandbox", sandbox_text)) {
        error = "missing sandbox section";
        return false;
    }

    if (!extract_bool_field(sandbox_text, "enabled", config.sandbox.enabled)) {
        error = "missing sandbox.enabled";
        return false;
    }

    if (!extract_array_field(sandbox_text, "allowed_paths", config.sandbox.allowed_paths)) {
        error = "missing sandbox.allowed_paths";
        return false;
    }

    if (!extract_array_field(sandbox_text, "denied_paths", config.sandbox.denied_paths)) {
        error = "missing sandbox.denied_paths";
        return false;
    }

    if (!extract_array_field(sandbox_text, "allowed_env", config.sandbox.allowed_env)) {
        error = "missing sandbox.allowed_env";
        return false;
    }

    if (!extract_object_field(text, "runtime", runtime_text)) {
        error = "missing runtime section";
        return false;
    }

    if (!extract_string_field(runtime_text, "default_shell", config.runtime.default_shell)) {
        error = "missing runtime.default_shell";
        return false;
    }

    extract_string_field(runtime_text, "execution_mode", config.runtime.execution_mode);

    if (!extract_string_field(runtime_text, "cache_dir", config.runtime.cache_dir)) {
        error = "missing runtime.cache_dir";
        return false;
    }

    return true;
}

bool ConfigStore::save(const std::filesystem::path& root, const Config& config, std::string& error) {
    std::error_code ec;
    std::filesystem::create_directories(root, ec);
    if (ec) {
        error = "unable to ensure project root";
        return false;
    }

    const std::string json = make_default_json(config);
    return write_file(config_path(root), json, error);
}

bool ConfigStore::validate(const Config& config, ConfigValidation& validation) {
    validation.valid = false;
    validation.issues.clear();

    if (config.version != 1) {
        append_issue(validation, "unsupported version");
    }

    if (config.runtime.default_shell.empty()) {
        append_issue(validation, "default shell is empty");
    }

    if (config.runtime.execution_mode.empty()) {
        append_issue(validation, "execution mode is empty");
    } else if (config.runtime.execution_mode != "safe" && config.runtime.execution_mode != "permission" && config.runtime.execution_mode != "bypass") {
        append_issue(validation, "execution mode must be safe, permission, or bypass");
    }

    if (config.runtime.cache_dir.empty()) {
        append_issue(validation, "cache dir is empty");
    }

    if (config.sandbox.allowed_paths.empty()) {
        append_issue(validation, "allowed paths is empty");
    }

    for (const std::string& path : config.sandbox.allowed_paths) {
        if (!is_safe_entry(path)) {
            append_issue(validation, "allowed paths contains an empty entry");
        }
    }

    for (const std::string& path : config.sandbox.denied_paths) {
        if (!is_safe_entry(path)) {
            append_issue(validation, "denied paths contains an empty entry");
        }
    }

    for (const std::string& env : config.sandbox.allowed_env) {
        if (!is_safe_entry(env)) {
            append_issue(validation, "allowed env contains an empty entry");
        }
    }

    std::set<std::string> allowed(config.sandbox.allowed_paths.begin(), config.sandbox.allowed_paths.end());
    for (const std::string& denied : config.sandbox.denied_paths) {
        if (allowed.find(denied) != allowed.end()) {
            append_issue(validation, "a path appears in both allowed and denied lists");
            break;
        }
    }

    validation.valid = validation.issues.empty();
    return validation.valid;
}

}
