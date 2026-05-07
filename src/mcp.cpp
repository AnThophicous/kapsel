#include "ote/mcp.hpp"

#include "ote/api.hpp"
#include "ote/config.hpp"
#include "ote/platform.hpp"
#include "ote/runtime.hpp"
#include "ote/secrets.hpp"

#include <cctype>
#include <cstdlib>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace ote {
namespace {

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

std::string json_string(const std::string& value) {
    return "\"" + escape_json(value) + "\"";
}

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

bool capture_object(const std::string& text, std::size_t pos, std::string& out) {
    if (pos >= text.size() || text[pos] != '{') {
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
        if (c == '{') {
            ++depth;
            continue;
        }
        if (c == '}') {
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

bool extract_field_start(const std::string& object_text, const std::string& field, std::size_t& pos) {
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

bool extract_string_field(const std::string& object_text, const std::string& field, std::string& value) {
    std::size_t pos = 0;
    if (!extract_field_start(object_text, field, pos)) {
        return false;
    }
    return read_json_string(object_text, pos, value);
}

bool extract_object_field(const std::string& object_text, const std::string& field, std::string& value) {
    std::size_t pos = 0;
    if (!extract_field_start(object_text, field, pos)) {
        return false;
    }
    return capture_object(object_text, pos, value);
}

bool extract_array_field(const std::string& object_text, const std::string& field, std::vector<std::string>& values) {
    std::size_t pos = 0;
    if (!extract_field_start(object_text, field, pos) || pos >= object_text.size() || object_text[pos] != '[') {
        return false;
    }

    ++pos;
    skip_ws(object_text, pos);
    values.clear();
    if (pos < object_text.size() && object_text[pos] == ']') {
        return true;
    }

    while (pos < object_text.size()) {
        std::string value;
        if (!read_json_string(object_text, pos, value)) {
            return false;
        }
        values.push_back(value);
        skip_ws(object_text, pos);
        if (pos >= object_text.size()) {
            return false;
        }
        if (object_text[pos] == ',') {
            ++pos;
            skip_ws(object_text, pos);
            continue;
        }
        if (object_text[pos] == ']') {
            return true;
        }
        return false;
    }

    return false;
}

std::string make_ok_response(const std::string& id, const std::string& result_json) {
    std::ostringstream out;
    out << "{\"jsonrpc\":\"2.0\",\"id\":" << id << ",\"result\":" << result_json << "}";
    return out.str();
}

std::string make_error_response(const std::string& id, int code, const std::string& message) {
    std::ostringstream out;
    out << "{\"jsonrpc\":\"2.0\",\"id\":" << id << ",\"error\":{\"code\":" << code << ",\"message\":" << json_string(message) << "}}";
    return out.str();
}

std::string content_frame(const std::string& body) {
    std::ostringstream out;
    out << "Content-Length: " << body.size() << "\r\n\r\n" << body;
    return out.str();
}

bool read_message(std::istream& in, std::string& body) {
    std::string header;
    std::size_t content_length = 0;
    while (std::getline(in, header)) {
        if (!header.empty() && header.back() == '\r') {
            header.pop_back();
        }
        if (header.empty()) {
            break;
        }
        const std::string prefix = "Content-Length:";
        if (header.rfind(prefix, 0) == 0) {
            content_length = static_cast<std::size_t>(std::stoul(header.substr(prefix.size())));
        }
    }

    if (content_length == 0) {
        return false;
    }

    body.assign(content_length, '\0');
    in.read(body.data(), static_cast<std::streamsize>(content_length));
    return in.good() || in.gcount() == static_cast<std::streamsize>(content_length);
}

std::string tool_descriptor_json() {
    std::ostringstream out;
    out << "[";
    out << "{\"name\":\"secret.list\",\"description\":\"List redacted secret projections\"},";
    out << "{\"name\":\"secret.describe\",\"description\":\"Describe one redacted secret projection\"},";
    out << "{\"name\":\"secret.add\",\"description\":\"Store a protected secret locally\"},";
    out << "{\"name\":\"exec.plan\",\"description\":\"Build a policy-checked execution plan\"},";
    out << "{\"name\":\"exec.run\",\"description\":\"Run a command through the broker\"},";
    out << "{\"name\":\"status\",\"description\":\"Show runtime status\"},";
    out << "{\"name\":\"paths\",\"description\":\"Show local storage paths\"},";
    out << "{\"name\":\"config.show\",\"description\":\"Return the active config\"}";
    out << "]";
    return out.str();
}

std::string resources_json(const std::filesystem::path& root) {
    std::ostringstream out;
    out << "[";
    out << "{\"uri\":\"ote://status\",\"name\":\"status\",\"mimeType\":\"application/json\"},";
    out << "{\"uri\":\"ote://paths\",\"name\":\"paths\",\"mimeType\":\"application/json\"},";
    out << "{\"uri\":\"ote://config\",\"name\":\"config\",\"mimeType\":\"application/json\"},";
    out << "{\"uri\":\"ote://manifest\",\"name\":\"manifest\",\"mimeType\":\"application/json\"},";
    out << "{\"uri\":\"ote://root\",\"name\":\"root\",\"mimeType\":\"text/plain\"}";
    out << "]";
    (void)root;
    return out.str();
}

std::string config_snapshot_json(const std::filesystem::path& root) {
    Config config;
    std::string error;
    if (!ConfigStore::load(root, config, error)) {
        return "{\"error\":\"unable to load config\"}";
    }

    std::ostringstream out;
    out << "{";
    out << "\"version\":" << config.version << ",";
    out << "\"sandbox\":{\"enabled\":" << (config.sandbox.enabled ? "true" : "false") << ",\"allowed_paths\":" << to_json_array(config.sandbox.allowed_paths) << ",\"denied_paths\":" << to_json_array(config.sandbox.denied_paths) << ",\"allowed_env\":" << to_json_array(config.sandbox.allowed_env) << "},";
    out << "\"runtime\":{\"default_shell\":" << json_string(config.runtime.default_shell) << ",\"execution_mode\":" << json_string(config.runtime.execution_mode) << ",\"cache_dir\":" << json_string(config.runtime.cache_dir) << "}";
    out << "}";
    return out.str();
}

std::string status_snapshot_json(const std::filesystem::path& root) {
    Config config;
    std::string error;
    ConfigValidation validation;
    bool ok = ConfigStore::load(root, config, error) && ConfigStore::validate(config, validation) && validation.valid;
    std::ostringstream out;
    out << "{";
    out << "\"root\":" << json_string(root.string()) << ",";
    out << "\"platform\":" << json_string(platform_name()) << ",";
    out << "\"architecture\":" << json_string(architecture_name()) << ",";
    out << "\"shell\":" << json_string(default_shell()) << ",";
    out << "\"broker\":" << json_string(ProcessBroker::backend().name()) << ",";
    out << "\"config_present\":" << (ConfigStore::exists(root) ? "true" : "false") << ",";
    out << "\"config_valid\":" << (ok ? "true" : "false") << ",";
    out << "\"mode\":" << json_string(ok ? config.runtime.execution_mode : "unknown") << ",";
    out << "\"cache_dir\":" << json_string(ok ? config.runtime.cache_dir : "unknown");
    out << "}";
    return out.str();
}

std::string paths_snapshot_json(const std::filesystem::path& root) {
    std::ostringstream out;
    out << "{";
    out << "\"root\":" << json_string(root.string()) << ",";
    out << "\"config\":" << json_string(ConfigStore::config_path(root).string()) << ",";
    out << "\"store\":" << json_string(ConfigStore::store_dir(root).string()) << ",";
    out << "\"vault\":" << json_string(ConfigStore::vault_dir(root).string()) << ",";
    out << "\"state\":" << json_string(ConfigStore::state_dir(root).string()) << ",";
    out << "\"cache\":" << json_string(ConfigStore::cache_dir(root).string()) << ",";
    out << "\"logs\":" << json_string(ConfigStore::logs_dir(root).string()) << ",";
    out << "\"lock\":" << json_string(ConfigStore::lock_file(root).string()) << "}";
    return out.str();
}

std::string manifest_snapshot_json(const std::filesystem::path& root) {
    return mcp_manifest_json(root, platform_name(), architecture_name(), active_secret_protector().name());
}

std::string extract_id_token(const std::string& body) {
    std::size_t pos = 0;
    if (!extract_field_start(body, "id", pos)) {
        return "null";
    }

    if (pos < body.size() && body[pos] == '"') {
        std::string value;
        if (!read_json_string(body, pos, value)) {
            return "null";
        }
        return json_string(value);
    }

    std::size_t end = pos;
    while (end < body.size() && body[end] != ',' && body[end] != '}' && std::isspace(static_cast<unsigned char>(body[end])) == 0) {
        ++end;
    }
    return body.substr(pos, end - pos);
}

std::string call_tool(const std::filesystem::path& root, const std::string& tool_name, const std::string& args_json, std::string& error) {
    if (tool_name == "status") {
        return status_snapshot_json(root);
    }

    if (tool_name == "paths") {
        return paths_snapshot_json(root);
    }

    if (tool_name == "config.show") {
        return config_snapshot_json(root);
    }

    if (tool_name == "manifest") {
        return manifest_snapshot_json(root);
    }

    if (tool_name == "secret.list") {
        std::vector<SecretProjection> secrets;
        if (!SecretVault::list(root, secrets, error)) {
            return {};
        }
        std::ostringstream out;
        out << "[";
        for (std::size_t i = 0; i < secrets.size(); ++i) {
            if (i > 0) {
                out << ",";
            }
            out << secret_projection_json(secrets[i]);
        }
        out << "]";
        return out.str();
    }

    if (tool_name == "secret.describe") {
        std::string name;
        if (!extract_string_field(args_json, "name", name)) {
            error = "missing name";
            return {};
        }
        SecretProjection projection;
        if (!SecretVault::describe(root, name, projection, error)) {
            return {};
        }
        return secret_projection_json(projection);
    }

    if (tool_name == "secret.add") {
        SecretDraft draft;
        if (!extract_string_field(args_json, "name", draft.name)) {
            error = "missing name";
            return {};
        }

        std::vector<std::string> tags;
        extract_array_field(args_json, "tags", tags);
        draft.tags = tags;

        std::string values_text;
        if (!extract_object_field(args_json, "values", values_text)) {
            error = "missing values";
            return {};
        }

        std::size_t pos = 0;
        skip_ws(values_text, pos);
        while (pos < values_text.size()) {
            std::string key;
            if (!read_json_string(values_text, pos, key)) {
                error = "invalid values object";
                return {};
            }
            skip_ws(values_text, pos);
            if (pos >= values_text.size() || values_text[pos] != ':') {
                error = "invalid values object";
                return {};
            }
            ++pos;
            skip_ws(values_text, pos);
            std::string value;
            if (!read_json_string(values_text, pos, value)) {
                error = "invalid values object";
                return {};
            }
            draft.values.emplace_back(key, value);
            skip_ws(values_text, pos);
            if (pos < values_text.size() && values_text[pos] == ',') {
                ++pos;
                skip_ws(values_text, pos);
                continue;
            }
            break;
        }

        if (!SecretVault::store(root, active_secret_protector(), draft, error)) {
            return {};
        }

        return "{\"stored\":true}";
    }

    if (tool_name == "exec.plan") {
        std::string shell;
        std::string command;
        extract_string_field(args_json, "shell", shell);
        if (!extract_string_field(args_json, "command", command)) {
            error = "missing command";
            return {};
        }
        CommandPlan plan;
        if (!ProcessBroker::build_plan(root, shell, command, plan, error)) {
            return {};
        }
        std::ostringstream out;
        out << "{";
        out << "\"shell\":" << json_string(plan.shell) << ",";
        out << "\"command\":" << json_string(plan.command) << ",";
        out << "\"execution_mode\":" << json_string(plan.execution_mode) << ",";
        out << "\"sandboxed\":" << (plan.sandboxed ? "true" : "false") << ",";
        out << "\"allowed_env\":" << to_json_array(plan.allowed_env);
        out << "}";
        return out.str();
    }

    if (tool_name == "exec.run") {
        std::string shell;
        std::string command;
        extract_string_field(args_json, "shell", shell);
        if (!extract_string_field(args_json, "command", command)) {
            error = "missing command";
            return {};
        }

        CommandPlan plan;
        if (!ProcessBroker::build_plan(root, shell, command, plan, error)) {
            return {};
        }

        if (plan.execution_mode == "permission") {
            error = "interactive approval unavailable in MCP mode";
            return {};
        }

        CommandResult result;
        if (!ProcessBroker::backend().run(plan, result, error)) {
            return {};
        }

        std::ostringstream out;
        out << "{";
        out << "\"exit_code\":" << result.exit_code << ",";
        out << "\"output\":" << json_string(result.output);
        out << "}";
        return out.str();
    }

    error = "unknown tool";
    return {};
}

}

int run_mcp_server(const std::filesystem::path& root) {
    std::ostream& out = std::cout;
    std::istream& in = std::cin;

    for (;;) {
        std::string body;
        if (!read_message(in, body)) {
            break;
        }

        std::string method;
        std::string id = "null";
        std::string params;
        extract_string_field(body, "method", method);
        id = extract_id_token(body);
        extract_object_field(body, "params", params);

        std::string response;
        if (method == "initialize") {
            std::ostringstream result;
            result << "{";
            result << "\"protocolVersion\":\"2024-11-05\",";
            result << "\"serverInfo\":{\"name\":\"ote\",\"version\":\"1.0.0\"},";
            result << "\"capabilities\":{\"tools\":{},\"resources\":{},\"logging\":{}}";
            result << "}";
            response = make_ok_response(id, result.str());
        } else if (method == "tools/list") {
            std::ostringstream result;
            result << "{\"tools\":" << tool_descriptor_json() << "}";
            response = make_ok_response(id, result.str());
        } else if (method == "tools/call") {
            std::string tool_name;
            std::string arguments;
            if (!extract_string_field(params, "name", tool_name)) {
                response = make_error_response(id, -32602, "missing tool name");
            } else {
                if (!extract_object_field(params, "arguments", arguments)) {
                    arguments = "{}";
                }
                std::string error;
                const std::string result_json = call_tool(root, tool_name, arguments, error);
                if (!error.empty()) {
                    response = make_error_response(id, -32000, error);
                } else {
                    response = make_ok_response(id, result_json);
                }
            }
        } else if (method == "resources/list") {
            std::ostringstream result;
            result << "{\"resources\":" << resources_json(root) << "}";
            response = make_ok_response(id, result.str());
        } else if (method == "resources/read") {
            std::string uri;
            if (!extract_string_field(params, "uri", uri)) {
                response = make_error_response(id, -32602, "missing uri");
            } else if (uri == "ote://status") {
                response = make_ok_response(id, "{\"contents\":[{\"uri\":\"ote://status\",\"mimeType\":\"application/json\",\"text\":" + json_string(status_snapshot_json(root)) + "}]}");
            } else if (uri == "ote://paths") {
                response = make_ok_response(id, "{\"contents\":[{\"uri\":\"ote://paths\",\"mimeType\":\"application/json\",\"text\":" + json_string(paths_snapshot_json(root)) + "}]}");
            } else if (uri == "ote://config") {
                response = make_ok_response(id, "{\"contents\":[{\"uri\":\"ote://config\",\"mimeType\":\"application/json\",\"text\":" + json_string(config_snapshot_json(root)) + "}]}");
            } else if (uri == "ote://manifest") {
                response = make_ok_response(id, "{\"contents\":[{\"uri\":\"ote://manifest\",\"mimeType\":\"application/json\",\"text\":" + json_string(manifest_snapshot_json(root)) + "}]}");
            } else if (uri == "ote://root") {
                response = make_ok_response(id, "{\"contents\":[{\"uri\":\"ote://root\",\"mimeType\":\"text/plain\",\"text\":" + json_string(root.string()) + "}]}");
            } else {
                response = make_error_response(id, -32602, "unknown resource");
            }
        } else if (method == "shutdown" || method == "exit") {
            response = make_ok_response(id, "{}");
            out << content_frame(response);
            out.flush();
            break;
        } else {
            response = make_error_response(id, -32601, "method not found");
        }

        out << content_frame(response);
        out.flush();
    }

    return 0;
}

}
