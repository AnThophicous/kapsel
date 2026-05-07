#include "ote/mcp_client.hpp"

#include "ote/api.hpp"
#include "ote/config.hpp"
#include "ote/json.hpp"
#include "ote/platform.hpp"
#include "ote/runtime.hpp"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <cstdlib>
#include <sstream>
#include <string>

namespace ote {
namespace {

JsonValue make_string(const std::string& value) {
    JsonValue json;
    json.kind = JsonValue::Kind::String;
    json.string = value;
    return json;
}

JsonValue make_server_config(const std::filesystem::path& command_path, const std::filesystem::path& cwd, const std::string& server_name) {
    JsonValue server;
    server.kind = JsonValue::Kind::Object;
    json_object_set(server, "command") = make_string(command_path.string());

    JsonValue& args = json_object_set(server, "args");
    args.kind = JsonValue::Kind::Array;
    args.array.push_back(make_string("mcp"));
    args.array.push_back(make_string("serve"));

    json_object_set(server, "cwd") = make_string(cwd.string());

    (void)server_name;
    return server;
}

bool read_file(const std::filesystem::path& path, std::string& text) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    text = buffer.str();
    return true;
}

bool write_atomic(const std::filesystem::path& path, const std::string& text, std::string& error) {
    const std::filesystem::path temp_path = path.parent_path() / (path.filename().string() + ".tmp");
    {
        std::ofstream file(temp_path, std::ios::binary | std::ios::trunc);
        if (!file) {
            error = "unable to write temporary config";
            return false;
        }
        file << text;
        if (!file.good()) {
            error = "unable to flush temporary config";
            return false;
        }
    }

    std::error_code ec;
    std::filesystem::remove(path, ec);
    ec.clear();
    std::filesystem::rename(temp_path, path, ec);
    if (ec) {
        std::filesystem::remove(temp_path, ec);
        error = "unable to replace config";
        return false;
    }

    return true;
}

bool ensure_parent(const std::filesystem::path& path, std::string& error) {
    const std::filesystem::path parent = path.parent_path();
    if (parent.empty()) {
        return true;
    }
    std::error_code ec;
    std::filesystem::create_directories(parent, ec);
    if (ec) {
        error = "unable to create config directory";
        return false;
    }
    return true;
}

bool parse_existing_config(const std::filesystem::path& path, JsonValue& root, std::string& error) {
    std::string text;
    if (!read_file(path, text)) {
        root.kind = JsonValue::Kind::Object;
        root.object.clear();
        return true;
    }

    if (text.empty()) {
        root.kind = JsonValue::Kind::Object;
        root.object.clear();
        return true;
    }

    if (!parse_json(text, root, error)) {
        return false;
    }

    if (root.kind != JsonValue::Kind::Object) {
        error = "config root must be an object";
        return false;
    }

    return true;
}

JsonValue& ensure_servers(JsonValue& root) {
    JsonValue& servers = json_object_set(root, "mcpServers");
    if (servers.kind != JsonValue::Kind::Object) {
        servers.kind = JsonValue::Kind::Object;
        servers.object.clear();
    }
    return servers;
}

bool is_absolute_or_empty(const std::filesystem::path& path) {
    return path.empty() || path.is_absolute();
}

std::string display_name_for_target(const std::string& target) {
    if (target == "claude") {
        return "Claude Desktop";
    }
    if (target == "cursor") {
        return "Cursor";
    }
    if (target == "vscode") {
        return "VS Code";
    }
    if (target == "windsurf") {
        return "Windsurf";
    }
    if (target == "custom") {
        return "Custom";
    }
    return "MCP client";
}

}

std::string mcp_client_config_json(const std::filesystem::path& command_path, const std::filesystem::path& cwd, const std::string& server_name) {
    JsonValue root;
    root.kind = JsonValue::Kind::Object;
    JsonValue& servers = json_object_set(root, "mcpServers");
    servers.kind = JsonValue::Kind::Object;
    servers.object.emplace_back(server_name, make_server_config(command_path, cwd, server_name));
    return stringify_json(root);
}

std::filesystem::path mcp_target_config_path(const std::string& target, const std::filesystem::path& cwd, std::string& error) {
    const std::filesystem::path home = user_home_path();

    if (target == "claude") {
#if defined(_WIN32)
        if (const char* appdata = std::getenv("APPDATA")) {
            return std::filesystem::path(appdata) / "Claude" / "claude_desktop_config.json";
        }
        if (!home.empty()) {
            return home / "AppData" / "Roaming" / "Claude" / "claude_desktop_config.json";
        }
#elif defined(__APPLE__)
        if (!home.empty()) {
            return home / "Library" / "Application Support" / "Claude" / "claude_desktop_config.json";
        }
#else
        if (!home.empty()) {
            return home / ".config" / "Claude" / "claude_desktop_config.json";
        }
#endif
        error = "unable to resolve Claude config path";
        return {};
    }

    if (target == "cursor") {
        if (!home.empty()) {
            return home / ".cursor" / "mcp.json";
        }
        error = "unable to resolve Cursor config path";
        return {};
    }

    if (target == "vscode") {
        return cwd / ".vscode" / "mcp.json";
    }

    if (target == "windsurf") {
        if (!home.empty()) {
            return home / ".codeium" / "windsurf" / "mcp_config.json";
        }
        error = "unable to resolve Windsurf config path";
        return {};
    }

    if (target == "custom") {
        error = "custom target requires --config";
        return {};
    }

    error = "unknown target";
    return {};
}

std::string mcp_target_display_name(const std::string& target) {
    return display_name_for_target(target);
}

bool install_mcp_config(const McpInstallRequest& request, std::string& error, std::string& installed_message) {
    if (!std::filesystem::exists(request.command_path)) {
        error = "Kapsel executable not found";
        return false;
    }

    if (!std::filesystem::exists(request.cwd)) {
        error = "project root not found";
        return false;
    }

    if (request.server_name.empty()) {
        error = "server name is empty";
        return false;
    }

    std::filesystem::path config_path = request.config_path;
    if (config_path.empty()) {
        config_path = mcp_target_config_path(request.target, request.cwd, error);
        if (config_path.empty()) {
            return false;
        }
    }

    if (!is_absolute_or_empty(config_path)) {
        config_path = std::filesystem::weakly_canonical(request.cwd / config_path);
    }

    JsonValue root;
    if (!parse_existing_config(config_path, root, error)) {
        return false;
    }

    JsonValue& servers = ensure_servers(root);
    servers.object.erase(
        std::remove_if(servers.object.begin(), servers.object.end(), [&](const auto& entry) {
            return entry.first == request.server_name;
        }),
        servers.object.end());
    servers.object.emplace_back(request.server_name, make_server_config(request.command_path, request.cwd, request.server_name));

    if (!ensure_parent(config_path, error)) {
        return false;
    }

    if (!write_atomic(config_path, stringify_json(root), error)) {
        return false;
    }

    std::string doctor_report;
    if (!mcp_doctor(request.cwd, request.command_path, doctor_report, error)) {
        return false;
    }

    std::ostringstream out;
    out << "Kapsel MCP installed for " << display_name_for_target(request.target) << ".\n\n";
    out << "Server:\n";
    out << "  name: " << request.server_name << "\n";
    out << "  command: " << request.command_path.string() << "\n";
    out << "  args: mcp serve\n";
    out << "  cwd: " << request.cwd.string() << "\n\n";
    out << "Next:\n";
    if (request.target == "claude") {
        out << "  restart Claude Desktop\n";
    } else if (request.target == "cursor") {
        out << "  restart Cursor\n";
    } else if (request.target == "vscode") {
        out << "  reopen VS Code\n";
    } else if (request.target == "windsurf") {
        out << "  restart Windsurf\n";
    } else {
        out << "  reload your MCP client\n";
    }
    out << "\n";
    out << doctor_report;
    installed_message = out.str();
    return true;
}

bool mcp_doctor(const std::filesystem::path& cwd, const std::filesystem::path& command_path, std::string& report, std::string& error) {
    Config config;
    ConfigValidation validation;
    std::string config_error;
    const bool config_ok = ConfigStore::load(cwd, config, config_error) && ConfigStore::validate(config, validation) && validation.valid;

    JsonValue manifest;
    std::string manifest_error;
    const std::string manifest_text = mcp_manifest_json(cwd, platform_name(), architecture_name(), active_secret_protector().name());
    const bool manifest_ok = parse_json(manifest_text, manifest, manifest_error);

    std::ostringstream out;
    out << "Doctor:\n";
    out << "  version: " << OTE_VERSION << "\n";
    out << "  executable: " << command_path.string() << "\n";
    out << "  root: " << cwd.string() << "\n";
    out << "  config: " << (config_ok ? "ok" : "invalid") << "\n";
    out << "  manifest: " << (manifest_ok ? "ok" : "invalid") << "\n";
    out << "  broker: " << ProcessBroker::backend().name() << "\n";
    report = out.str();

    if (!std::filesystem::exists(command_path)) {
        error = "Kapsel executable not found";
        return false;
    }

    if (!config_ok) {
        error = config_error.empty() ? "config invalid" : config_error;
        if (!validation.issues.empty()) {
            for (const std::string& issue : validation.issues) {
                report += "  issue: " + issue + "\n";
            }
        }
        return false;
    }

    if (!manifest_ok) {
        error = manifest_error.empty() ? "manifest invalid" : manifest_error;
        return false;
    }

    error.clear();
    return true;
}

}
