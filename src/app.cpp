#include "ote/app.hpp"

#include "ote/api.hpp"
#include "ote/config.hpp"
#include "ote/layers.hpp"
#include "ote/json.hpp"
#include "ote/mcp_client.hpp"
#include "ote/mcp.hpp"
#include "ote/platform.hpp"
#include "ote/runtime.hpp"
#include "ote/secrets.hpp"
#include "ote/terminal.hpp"

#include <filesystem>
#include <fstream>
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <cwchar>
#include <windows.h>
#endif

namespace ote {
namespace {

void print_banner(const std::filesystem::path& cwd) {
    std::cout << colorize(ascii_banner(cwd), "36");
}

void print_help(const std::filesystem::path& cwd) {
    print_banner(cwd);
    std::cout << "\n";
    std::cout << colorize("Usage:", "33") << "\n";
    std::cout << "  " << colorize("ote --setup", "32") << "\n";
    std::cout << "  " << colorize("ote --status", "32") << "\n";
    std::cout << "  " << colorize("ote --doctor", "32") << "\n";
    std::cout << "  " << colorize("ote --validate", "32") << "\n";
    std::cout << "  " << colorize("ote --paths", "32") << "\n";
    std::cout << "  " << colorize("ote --putpath", "32") << "\n";
    std::cout << "  " << colorize("ote update", "32") << "\n";
    std::cout << "  " << colorize("ote --migration [--profile <name>] [.env path]", "32") << "\n";
    std::cout << "  " << colorize("ote config show", "32") << "\n";
    std::cout << "  " << colorize("ote secret list", "32") << "\n";
    std::cout << "  " << colorize("ote secret describe <name>", "32") << "\n";
    std::cout << "  " << colorize("ote secret add <name> [--tag <tag>] KEY=VALUE...", "32") << "\n";
    std::cout << "  " << colorize("ote bridge manifest [profile]", "32") << "\n";
    std::cout << "  " << colorize("ote bridge materialize [profile]", "32") << "\n";
    std::cout << "  " << colorize("ote bridge env [profile]", "32") << "\n";
    std::cout << "  " << colorize("ote bridge run [--profile <name>] [shell] <command>", "32") << "\n";
    std::cout << "  " << colorize("ote api manifest", "32") << "\n";
    std::cout << "  " << colorize("ote api secrets", "32") << "\n";
    std::cout << "  " << colorize("ote exec plan <command>", "32") << "\n";
    std::cout << "  " << colorize("ote exec run <command>", "32") << "\n";
    std::cout << "  " << colorize("ote mcp manifest", "32") << "\n";
    std::cout << "  " << colorize("ote mcp config", "32") << "\n";
    std::cout << "  " << colorize("ote mcp install <target>", "32") << "\n";
    std::cout << "  " << colorize("ote mcp install --print", "32") << "\n";
    std::cout << "  " << colorize("ote mcp install --config <path>", "32") << "\n";
    std::cout << "  " << colorize("ote mcp doctor", "32") << "\n";
    std::cout << "  " << colorize("ote mcp serve", "32") << "\n";
    std::cout << "  " << colorize("ote --help", "32") << "\n";
    std::cout << "  " << colorize("ote --version", "32") << "\n";
}

void print_paths(const std::filesystem::path& cwd) {
    std::cout << "Root: " << cwd.string() << "\n";
    std::cout << "Config: " << ConfigStore::config_path(cwd).string() << "\n";
    std::cout << "Store: " << ConfigStore::store_dir(cwd).string() << "\n";
    std::cout << "Vault: " << ConfigStore::vault_dir(cwd).string() << "\n";
    std::cout << "State: " << ConfigStore::state_dir(cwd).string() << "\n";
    std::cout << "Cache: " << ConfigStore::cache_dir(cwd).string() << "\n";
    std::cout << "Logs: " << ConfigStore::logs_dir(cwd).string() << "\n";
    std::cout << "Layers: " << ConfigStore::layers_dir(cwd).string() << "\n";
    std::cout << "Lock: " << ConfigStore::lock_file(cwd).string() << "\n";
}

void print_status(const std::filesystem::path& cwd) {
    Config config;
    std::string error;
    ConfigValidation validation;

    std::cout << colorize("OTE status", "36") << "\n";
    std::cout << colorize("Path:", "33") << " " << cwd.string() << "\n";
    std::cout << colorize("Platform:", "33") << " " << platform_name() << "\n";
    std::cout << colorize("Architecture:", "33") << " " << architecture_name() << "\n";
    std::cout << colorize("Default shell:", "33") << " " << default_shell() << "\n";
    std::cout << colorize("Broker:", "33") << " " << ProcessBroker::backend().name() << "\n";
    std::cout << colorize("Config:", "33") << " " << (ConfigStore::exists(cwd) ? "present" : "missing") << "\n";

    if (ConfigStore::load(cwd, config, error) && ConfigStore::validate(config, validation) && validation.valid) {
        std::cout << colorize("Sandbox:", "33") << " " << (config.sandbox.enabled ? "enabled" : "disabled") << "\n";
        std::cout << colorize("Mode:", "33") << " " << config.runtime.execution_mode << "\n";
        std::cout << colorize("Cache dir:", "33") << " " << config.runtime.cache_dir << "\n";
        return;
    }

    std::cout << colorize("Sandbox:", "33") << " unknown\n";
    std::cout << colorize("Cache dir:", "33") << " unknown\n";
}

std::string escape_json(const std::string& value);
std::string join_args(const std::vector<std::string>& args, std::size_t begin);
bool load_bridge_env(const std::filesystem::path& cwd, const std::string& profile, std::vector<std::pair<std::string, std::string>>& env, std::string& error);

std::string read_all(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

std::string extract_profile_from_env(const std::filesystem::path& path) {
    const std::string text = read_all(path);
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        const std::size_t pos = line.find('=');
        if (pos == std::string::npos || pos == 0) {
            continue;
        }
        const std::string key = line.substr(0, pos);
        if (key == "OTE_PROFILE") {
            return line.substr(pos + 1);
        }
    }
    return "prod";
}

void print_layers_manifest(const std::filesystem::path& cwd, const std::string& profile) {
    const std::filesystem::path manifest_path = ConfigStore::layers_dir(cwd) / (profile + ".json");
    const std::string text = read_all(manifest_path);
    if (!text.empty()) {
        std::cout << text << "\n";
        return;
    }

    std::cout << layers_manifest_json(cwd, profile, (cwd / ".env").string(), "layers-" + profile, 0) << "\n";
}

void print_layers_materialize(const std::filesystem::path& cwd, const std::string& profile) {
    std::vector<std::pair<std::string, std::string>> values;
    std::string error;
    if (!materialize_layers(cwd, profile, values, error)) {
        std::cerr << "layers materialize failed: " << error << "\n";
        return;
    }

    std::cout << "{";
    std::cout << "\"profile\":\"" << escape_json(profile) << "\",";
    std::cout << "\"env\":" << to_json_object(values);
    std::cout << "}\n";
}

void print_layers_env(const std::filesystem::path& cwd, const std::string& profile) {
    std::vector<std::pair<std::string, std::string>> values;
    std::string error;
    if (!load_bridge_env(cwd, profile, values, error)) {
        std::cerr << "layers env failed: " << error << "\n";
        return;
    }

    for (const auto& entry : values) {
        std::cout << entry.first << "=" << entry.second << "\n";
    }
}

void print_config_show(const std::filesystem::path& cwd) {
    Config config;
    std::string error;
    if (!ConfigStore::load(cwd, config, error)) {
        std::cerr << "config show failed: " << error << "\n";
        return;
    }

    std::cout << "{\"version\":" << config.version
              << ",\"sandbox\":{\"enabled\":" << (config.sandbox.enabled ? "true" : "false")
              << ",\"allowed_paths\":" << to_json_array(config.sandbox.allowed_paths)
              << ",\"denied_paths\":" << to_json_array(config.sandbox.denied_paths)
              << ",\"allowed_env\":" << to_json_array(config.sandbox.allowed_env)
              << "},\"runtime\":{\"default_shell\":\"" << escape_json(config.runtime.default_shell)
              << "\",\"execution_mode\":\"" << escape_json(config.runtime.execution_mode)
              << "\",\"cache_dir\":\"" << escape_json(config.runtime.cache_dir)
              << "\"}}\n";
}

bool load_bridge_env(const std::filesystem::path& cwd, const std::string& profile, std::vector<std::pair<std::string, std::string>>& env, std::string& error) {
    if (!materialize_layers(cwd, profile, env, error)) {
        if (error != "secret not found") {
            return false;
        }

        LayerMigrationRequest request;
        request.source_path = cwd / ".env";
        request.profile = profile;
        LayerMigrationResult result;
        std::string migrate_error;
        if (!migrate_layers(cwd, request, result, migrate_error)) {
            error = migrate_error;
            return false;
        }

        if (!materialize_layers(cwd, profile, env, error)) {
            return false;
        }
    }

    if (env.empty()) {
        error = "bridge env is empty";
        return false;
    }

    env.insert(env.begin(), std::make_pair(std::string("OTE_BRIDGE"), std::string("layers-ote")));
    env.insert(env.begin(), std::make_pair(std::string("OTE_PROFILE"), profile));
    return true;
}

void run_bridge_command(const std::filesystem::path& cwd, const std::vector<std::string>& args) {
    std::string profile = "prod";
    std::string shell = default_shell();
    std::size_t index = 2;

    if (index < args.size() && args[index] == "--profile") {
        if (index + 1 >= args.size()) {
            std::cerr << "bridge run failed: missing profile value\n";
            return;
        }
        profile = args[index + 1];
        index += 2;
    }

    if (index < args.size() && (args[index] == "powershell" || args[index] == "cmd" || args[index] == "bash" || args[index] == "sh")) {
        shell = args[index];
        ++index;
    }

    const std::string command = join_args(args, index);
    if (command.empty()) {
        std::cerr << "bridge run failed: missing command\n";
        return;
    }

    std::vector<std::pair<std::string, std::string>> overlay;
    std::string error;
    if (!load_bridge_env(cwd, profile, overlay, error)) {
        std::cerr << "bridge run failed: " << error << "\n";
        return;
    }

    CommandPlan plan;
    if (!ProcessBroker::build_plan(cwd, shell, command, plan, error)) {
        std::cerr << "bridge run failed: " << error << "\n";
        return;
    }

    plan.extra_env = std::move(overlay);

    CommandResult result;
    if (!ProcessBroker::backend().run(plan, result, error)) {
        std::cerr << "bridge run failed: " << error << "\n";
        return;
    }

    if (!result.output.empty()) {
        std::cout << result.output;
        if (result.output.back() != '\n') {
            std::cout << "\n";
        }
    }
    std::cout << "exit=" << result.exit_code << "\n";
}

void print_doctor(const std::filesystem::path& cwd) {
    Config config;
    std::string error;
    ConfigValidation validation;

    std::cout << colorize("OTE doctor", "36") << "\n";
    std::cout << colorize("Version:", "33") << " " << OTE_VERSION << "\n";
    std::cout << colorize("Platform:", "33") << " " << platform_name() << "\n";
    std::cout << colorize("Architecture:", "33") << " " << architecture_name() << "\n";
    std::cout << colorize("Shell:", "33") << " " << default_shell() << "\n";
    std::cout << colorize("Broker:", "33") << " " << ProcessBroker::backend().name() << "\n";
    std::cout << colorize("MCP:", "33") << " available\n";
    std::cout << colorize("Config present:", "33") << " " << (ConfigStore::exists(cwd) ? "yes" : "no") << "\n";
    if (ConfigStore::load(cwd, config, error) && ConfigStore::validate(config, validation) && validation.valid) {
        std::cout << colorize("Config valid:", "33") << " yes\n";
        std::cout << colorize("Mode:", "33") << " " << config.runtime.execution_mode << "\n";
        std::cout << colorize("Allowed paths:", "33") << " " << config.sandbox.allowed_paths.size() << "\n";
        std::cout << colorize("Denied paths:", "33") << " " << config.sandbox.denied_paths.size() << "\n";
    } else {
        std::cout << colorize("Config valid:", "33") << " no\n";
        if (!error.empty()) {
            std::cout << colorize("Error:", "33") << " " << error << "\n";
        }
    }
}

void print_validation(const std::filesystem::path& cwd) {
    Config config;
    std::string error;
    ConfigValidation validation;

    if (!ConfigStore::load(cwd, config, error)) {
        std::cerr << "validate failed: " << error << "\n";
        return;
    }

    if (!ConfigStore::validate(config, validation) || !validation.valid) {
        std::cerr << "config invalid\n";
        for (const std::string& issue : validation.issues) {
            std::cerr << "- " << issue << "\n";
        }
        return;
    }

    std::cout << "config valid\n";
}

void print_secret_list(const std::filesystem::path& cwd) {
    std::vector<SecretProjection> secrets;
    std::string error;
    if (!SecretVault::list(cwd, secrets, error)) {
        std::cerr << "secret list failed: " << error << "\n";
        return;
    }

    for (const SecretProjection& secret : secrets) {
        std::cout << secret.name << " [" << secret.protector << "]";
        if (!secret.tags.empty()) {
            std::cout << " tags=" << to_json_array(secret.tags);
        }
        std::cout << " keys=" << to_json_array(secret.exposed_keys) << "\n";
    }
}

void print_secret_describe(const std::filesystem::path& cwd, const std::string& name) {
    SecretProjection secret;
    std::string error;
    if (!SecretVault::describe(cwd, name, secret, error)) {
        std::cerr << "secret describe failed: " << error << "\n";
        return;
    }

    std::cout << secret_projection_json(secret) << "\n";
}

void print_secret_api(const std::filesystem::path& cwd) {
    std::vector<SecretProjection> secrets;
    std::string error;
    if (!SecretVault::list(cwd, secrets, error)) {
        std::cerr << "api secrets failed: " << error << "\n";
        return;
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
    std::cout << out.str() << "\n";
}

void print_manifest(const std::filesystem::path& cwd) {
    std::cout << mcp_manifest_json(cwd, platform_name(), architecture_name(), active_secret_protector().name()) << "\n";
}

std::string read_all(const std::filesystem::path& path);

std::filesystem::path resolved_executable(const AppOptions& options) {
    if (!options.executable.empty()) {
        return std::filesystem::weakly_canonical(options.executable);
    }
    return executable_path();
}

void print_mcp_config(const AppOptions& options) {
    const std::filesystem::path exe = resolved_executable(options);
    if (exe.empty()) {
        std::cerr << "mcp config failed: unable to resolve executable path\n";
        return;
    }
    std::cout << mcp_client_config_json(exe, options.cwd) << "\n";
}

bool print_mcp_doctor(const AppOptions& options) {
    const std::filesystem::path exe = resolved_executable(options);
    if (exe.empty()) {
        std::cerr << "mcp doctor failed: unable to resolve executable path\n";
        return false;
    }

    std::string report;
    std::string error;
    if (!mcp_doctor(options.cwd, exe, report, error)) {
        std::cout << report;
        std::cerr << "mcp doctor failed: " << error << "\n";
        return false;
    }

    std::cout << report;
    return true;
}

bool run_mcp_install(const AppOptions& options) {
    std::string target;
    std::filesystem::path config_path;
    bool print_only = false;

    for (std::size_t i = 2; i < options.args.size(); ++i) {
        const std::string& token = options.args[i];
        if (token == "--print") {
            print_only = true;
            continue;
        }
        if (token == "--config") {
            if (i + 1 >= options.args.size()) {
                std::cerr << "mcp install failed: missing config path\n";
                return false;
            }
            config_path = options.args[++i];
            continue;
        }
        if (target.empty()) {
            target = token;
            continue;
        }
        std::cerr << "mcp install failed: unexpected argument " << token << "\n";
        return false;
    }

    const std::filesystem::path exe = resolved_executable(options);
    if (exe.empty()) {
        std::cerr << "mcp install failed: unable to resolve executable path\n";
        return false;
    }

    if (target.empty()) {
        target = config_path.empty() ? "custom" : "custom";
    }

    if (target == "custom" && config_path.empty()) {
        std::cerr << "mcp install failed: custom target requires --config\n";
        return false;
    }

    if (print_only) {
        std::cout << mcp_client_config_json(exe, options.cwd) << "\n";
        return true;
    }

    McpInstallRequest request;
    request.target = target;
    request.command_path = exe;
    request.cwd = options.cwd;
    request.config_path = config_path;
    request.server_name = "ote";

    std::string error;
    std::string message;
    if (!install_mcp_config(request, error, message)) {
        std::cerr << "mcp install failed: " << error << "\n";
        return false;
    }

    std::cout << message;
    return true;
}

bool putpath(const std::filesystem::path& dir, std::string& message, std::string& error) {
    if (dir.empty() || !std::filesystem::exists(dir)) {
        error = "directory not found";
        return false;
    }

#if defined(_WIN32)
    std::wstring target = dir.wstring();
    HKEY key = nullptr;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, L"Environment", 0, KEY_READ | KEY_SET_VALUE, &key) != ERROR_SUCCESS) {
        error = "unable to open user environment";
        return false;
    }

    std::wstring current;
    DWORD type = 0;
    DWORD size = 0;
    if (RegQueryValueExW(key, L"Path", nullptr, &type, nullptr, &size) == ERROR_SUCCESS && (type == REG_SZ || type == REG_EXPAND_SZ) && size > 2) {
        current.resize(size / sizeof(wchar_t) + 1);
        if (RegQueryValueExW(key, L"Path", nullptr, &type, reinterpret_cast<LPBYTE>(current.data()), &size) == ERROR_SUCCESS) {
            current.resize(wcslen(current.c_str()));
        } else {
            current.clear();
        }
    }

    const std::wstring needle = target;
    const std::wstring current_path = current;
    if (current_path.find(needle) != std::wstring::npos) {
        RegCloseKey(key);
        message = "PATH already contains OTE directory\n";
        return true;
    }

    std::wstring updated = current_path;
    if (!updated.empty() && updated.back() != L';') {
        updated.push_back(L';');
    }
    updated += target;
    if (RegSetValueExW(key, L"Path", 0, REG_EXPAND_SZ, reinterpret_cast<const BYTE*>(updated.c_str()), static_cast<DWORD>((updated.size() + 1) * sizeof(wchar_t))) != ERROR_SUCCESS) {
        RegCloseKey(key);
        error = "unable to update PATH";
        return false;
    }
    RegCloseKey(key);

    const std::wstring env = L"Environment";
    SendMessageTimeoutW(HWND_BROADCAST, WM_SETTINGCHANGE, 0, reinterpret_cast<LPARAM>(env.c_str()), SMTO_ABORTIFHUNG, 1000, nullptr);
    message = "PATH updated for current user\n";
    return true;
#else
    const std::filesystem::path profile = user_home_path() / ".profile";
    std::ostringstream block;
    block << "\n# OTE PATH START\n";
    block << "case \":$PATH:\" in\n";
    block << "  *\":";
    block << dir.string();
    block << ":\"*) ;;\n";
    block << "  *) export PATH=\"";
    block << dir.string();
    block << ":$PATH\" ;;\n";
    block << "esac\n";
    block << "# OTE PATH END\n";

    std::string existing;
    if (std::filesystem::exists(profile)) {
        existing = read_all(profile);
        if (existing.find("# OTE PATH START") != std::string::npos) {
            message = "PATH profile already configured\n";
            return true;
        }
    }

    std::ofstream file(profile, std::ios::app | std::ios::binary);
    if (!file) {
        error = "unable to update PATH profile";
        return false;
    }
    file << block.str();
    if (!file.good()) {
        error = "unable to write PATH profile";
        return false;
    }
    message = "PATH profile updated\n";
    return true;
#endif
}

std::string run_capture(const std::string& command) {
    std::string output;
#if defined(_WIN32)
    FILE* pipe = _popen(command.c_str(), "r");
#else
    FILE* pipe = popen(command.c_str(), "r");
#endif
    if (!pipe) {
        return {};
    }

    char buffer[4096];
    while (std::fgets(buffer, sizeof(buffer), pipe) != nullptr) {
        output += buffer;
    }

#if defined(_WIN32)
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return output;
}

int version_part(const std::string& version, std::size_t index) {
    std::size_t start = 0;
    for (std::size_t i = 0; i < index; ++i) {
        start = version.find('.', start);
        if (start == std::string::npos) {
            return 0;
        }
        ++start;
    }
    std::size_t end = version.find('.', start);
    const std::string token = version.substr(start, end == std::string::npos ? std::string::npos : end - start);
    try {
        return std::stoi(token);
    } catch (...) {
        return 0;
    }
}

bool version_is_newer(const std::string& current, const std::string& latest) {
    for (std::size_t i = 0; i < 3; ++i) {
        const int a = version_part(current, i);
        const int b = version_part(latest, i);
        if (b > a) {
            return true;
        }
        if (b < a) {
            return false;
        }
    }
    return false;
}

std::string github_repo() {
    return "AnThophicous/ote";
}

std::string latest_release_json(std::string& error) {
#if defined(_WIN32)
    const std::string command =
        "powershell -NoProfile -Command \"$ProgressPreference='SilentlyContinue'; "
        "$headers=@{'User-Agent'='ote'}; "
        "(Invoke-RestMethod -Headers $headers -Uri 'https://api.github.com/repos/" + github_repo() + "/releases/latest') | ConvertTo-Json -Depth 8\"";
    std::string output = run_capture(command);
    if (output.empty()) {
        error = "unable to query GitHub releases";
    }
    return output;
#else
    const std::string command = "curl -fsSL -H 'User-Agent: ote' https://api.github.com/repos/" + github_repo() + "/releases/latest";
    std::string output = run_capture(command);
    if (output.empty()) {
        error = "unable to query GitHub releases";
    }
    return output;
#endif
}

std::string release_asset_name(const std::string& version) {
#if defined(_WIN32)
    return "ote-" + version + "-win64.zip";
#elif defined(__APPLE__)
    return "ote-" + version + "-macos-" + architecture_name() + ".tar.gz";
#else
    return "ote-" + version + "-linux-" + architecture_name() + ".tar.gz";
#endif
}

bool download_release_asset(const std::string& url, const std::filesystem::path& output_path, std::string& error) {
    if (!output_path.parent_path().empty()) {
        std::error_code ec;
        std::filesystem::create_directories(output_path.parent_path(), ec);
        if (ec) {
            error = "unable to create update cache";
            return false;
        }
    }

#if defined(_WIN32)
    const std::string command =
        "powershell -NoProfile -Command \"$ProgressPreference='SilentlyContinue'; "
        "Invoke-WebRequest -UseBasicParsing -Headers @{'User-Agent'='ote'} -Uri '" + url + "' -OutFile '" + output_path.string() + "'\"";
#else
    const std::string command = "curl -fL -H 'User-Agent: ote' -o '" + output_path.string() + "' '" + url + "'";
#endif
    const std::string output = run_capture(command);
    if (!std::filesystem::exists(output_path)) {
        if (output.empty()) {
            error = "download failed";
        } else {
            error = output;
        }
        return false;
    }
    return true;
}

bool extract_release_asset(const std::filesystem::path& asset_path, const std::filesystem::path& extract_dir, std::string& error) {
    std::error_code ec;
    std::filesystem::remove_all(extract_dir, ec);
    ec.clear();
    std::filesystem::create_directories(extract_dir, ec);
    if (ec) {
        error = "unable to create staging directory";
        return false;
    }

#if defined(_WIN32)
    const std::string command =
        "powershell -NoProfile -Command \"Expand-Archive -Path '" + asset_path.string() + "' -DestinationPath '" + extract_dir.string() + "' -Force\"";
#else
    const std::string command = "tar -xzf '" + asset_path.string() + "' -C '" + extract_dir.string() + "'";
#endif
    const std::string output = run_capture(command);
    if (!std::filesystem::exists(extract_dir)) {
        error = output.empty() ? "unable to extract release asset" : output;
        return false;
    }
    return true;
}

bool perform_update(const AppOptions& options) {
    std::string error;
    const std::string json = latest_release_json(error);
    if (json.empty()) {
        std::cerr << "update failed: " << error << "\n";
        return false;
    }

    JsonValue root;
    std::string parse_error;
    if (!parse_json(json, root, parse_error)) {
        std::cerr << "update failed: invalid release payload\n";
        return false;
    }

    const JsonValue* tag = json_object_get(root, "tag_name");
    if (tag == nullptr || tag->kind != JsonValue::Kind::String) {
        std::cerr << "update failed: missing tag_name\n";
        return false;
    }

    std::string latest_version = tag->string;
    if (!latest_version.empty() && (latest_version.front() == 'v' || latest_version.front() == 'V')) {
        latest_version.erase(latest_version.begin());
    }

    if (!version_is_newer(OTE_VERSION, latest_version)) {
        std::cout << "OTE is already up to date (" << OTE_VERSION << ")\n";
        return true;
    }

    const JsonValue* assets = json_object_get(root, "assets");
    if (assets == nullptr || assets->kind != JsonValue::Kind::Array) {
        std::cerr << "update failed: missing assets\n";
        return false;
    }

    std::string asset_url;
    std::string asset_name = release_asset_name(latest_version);
    for (const JsonValue& asset : assets->array) {
        const JsonValue* name = json_object_get(asset, "name");
        const JsonValue* url = json_object_get(asset, "browser_download_url");
        if (name != nullptr && url != nullptr && name->kind == JsonValue::Kind::String && url->kind == JsonValue::Kind::String && name->string == asset_name) {
            asset_url = url->string;
            break;
        }
    }

    if (asset_url.empty()) {
        std::cerr << "update failed: matching asset not found for " << asset_name << "\n";
        return false;
    }

    const std::filesystem::path cache_dir = ConfigStore::cache_dir(options.cwd) / "update";
    const std::filesystem::path download_path = cache_dir / asset_name;
    if (!download_release_asset(asset_url, download_path, error)) {
        std::cerr << "update failed: " << error << "\n";
        return false;
    }

    const std::filesystem::path stage_dir = cache_dir / latest_version;
    if (!extract_release_asset(download_path, stage_dir, error)) {
        std::cerr << "update failed: " << error << "\n";
        return false;
    }

    std::cout << "Downloaded " << latest_version << " to " << download_path.string() << "\n";
    std::cout << "Staged: " << stage_dir.string() << "\n";
    std::cout << "Current version: " << OTE_VERSION << "\n";
    return true;
}

std::string join_args(const std::vector<std::string>& args, std::size_t begin) {
    std::ostringstream out;
    for (std::size_t i = begin; i < args.size(); ++i) {
        if (i > begin) {
            out << ' ';
        }
        out << args[i];
    }
    return out.str();
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

bool confirm_permission(const CommandPlan& plan) {
    std::cout << "Allow execution?\n";
    std::cout << "  shell: " << plan.shell << "\n";
    std::cout << "  mode: " << plan.execution_mode << "\n";
    std::cout << "  command: " << plan.command << "\n";
    std::cout << "  cwd: " << plan.working_directory.string() << "\n";
    std::cout << "Type yes to continue: ";
    std::cout.flush();
    std::string answer;
    if (!std::getline(std::cin, answer)) {
        return false;
    }
    return answer == "yes" || answer == "YES" || answer == "y" || answer == "Y";
}

void print_plan(const std::filesystem::path& cwd, const std::string& shell, const std::string& command) {
    CommandPlan plan;
    std::string error;
    if (!ProcessBroker::build_plan(cwd, shell, command, plan, error)) {
        std::cerr << "plan failed: " << error << "\n";
        return;
    }

    std::ostringstream out;
    out << "{";
    out << "\"shell\":\"" << escape_json(shell) << "\",";
    out << "\"command\":\"" << escape_json(command) << "\",";
    out << "\"working_directory\":\"" << escape_json(cwd.string()) << "\",";
    out << "\"execution_mode\":\"" << escape_json(plan.execution_mode) << "\",";
    out << "\"sandboxed\":" << (plan.sandboxed ? "true" : "false") << ",";
    out << "\"allowed_env\":" << to_json_array(plan.allowed_env);
    out << "}";
    std::cout << out.str() << "\n";
}

void run_plan(const std::filesystem::path& cwd, const std::string& shell, const std::string& command) {
    CommandPlan plan;
    std::string error;
    if (!ProcessBroker::build_plan(cwd, shell, command, plan, error)) {
        std::cerr << "run failed: " << error << "\n";
        return;
    }

    if (plan.execution_mode == "permission" && !confirm_permission(plan)) {
        std::cerr << "run cancelled\n";
        return;
    }

    CommandResult result;
    if (!ProcessBroker::backend().run(plan, result, error)) {
        std::cerr << "run failed: " << error << "\n";
        return;
    }

    if (!result.output.empty()) {
        std::cout << result.output;
        if (result.output.back() != '\n') {
            std::cout << "\n";
        }
    }
    std::cout << "exit=" << result.exit_code << "\n";
}

bool parse_secret_values(const std::vector<std::string>& args, std::size_t begin, SecretDraft& draft, std::string& error) {
    for (std::size_t i = begin; i < args.size(); ++i) {
        const std::string& token = args[i];
        if (token == "--tag") {
            if (i + 1 >= args.size()) {
                error = "missing tag value";
                return false;
            }
            draft.tags.push_back(args[++i]);
            continue;
        }

        const std::size_t pos = token.find('=');
        if (pos == std::string::npos) {
            error = "expected KEY=VALUE";
            return false;
        }

        draft.values.emplace_back(token.substr(0, pos), token.substr(pos + 1));
    }

    return true;
}

void add_secret(const std::filesystem::path& cwd, const std::vector<std::string>& args) {
    if (args.size() < 3) {
        std::cerr << "secret add requires a name and at least one KEY=VALUE pair\n";
        return;
    }

    SecretDraft draft;
    draft.name = args[2];

    std::string error;
    if (!parse_secret_values(args, 3, draft, error)) {
        std::cerr << "secret add failed: " << error << "\n";
        return;
    }

    if (!SecretVault::ensure_layout(cwd, error)) {
        std::cerr << "secret add failed: " << error << "\n";
        return;
    }

    if (!SecretVault::store(cwd, active_secret_protector(), draft, error)) {
        std::cerr << "secret add failed: " << error << "\n";
        return;
    }

    std::cout << "secret stored\n";
}

void run_migration(const std::filesystem::path& cwd, const std::vector<std::string>& args) {
    std::filesystem::path source = cwd / ".env";
    std::string profile;

    for (std::size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--profile") {
            if (i + 1 >= args.size()) {
                std::cerr << "migration failed: missing profile value\n";
                return;
            }
            profile = args[++i];
            continue;
        }

        if (!args[i].empty() && args[i][0] != '-') {
            source = std::filesystem::path(args[i]);
            if (source.is_relative()) {
                source = cwd / source;
            }
            continue;
        }
    }

    if (profile.empty()) {
        profile = extract_profile_from_env(source);
    }

    LayerMigrationRequest request;
    request.source_path = source;
    request.profile = profile;

    LayerMigrationResult result;
    std::string error;
    if (!migrate_layers(cwd, request, result, error)) {
        std::cerr << "migration failed: " << error << "\n";
        return;
    }

    std::cout << "layers migrated\n";
    std::cout << "profile=" << result.profile << "\n";
    std::cout << "secret=" << result.secret_name << "\n";
    std::cout << "source=" << result.source_path.string() << "\n";
    std::cout << "manifest=" << result.manifest_path.string() << "\n";
    std::cout << "imported=" << result.imported_count << "\n";
}

}

int run_app(const AppOptions& options) {
    if (options.args.empty()) {
        print_help(options.cwd);
        return 0;
    }

    const std::string& command = options.args.front();

    if (command == "--help" || command == "-h" || command == "help") {
        print_help(options.cwd);
        return 0;
    }

    if (command == "--version" || command == "-v" || command == "version") {
        std::cout << "ote " << OTE_VERSION << "\n";
        return 0;
    }

    if (command == "--status") {
        print_status(options.cwd);
        return 0;
    }

    if (command == "--doctor") {
        print_doctor(options.cwd);
        return 0;
    }

    if (command == "--validate") {
        print_validation(options.cwd);
        return 0;
    }

    if (command == "--paths") {
        print_paths(options.cwd);
        return 0;
    }

    if (command == "--putpath") {
        const std::filesystem::path exe = resolved_executable(options);
        if (exe.empty()) {
            std::cerr << "putpath failed: unable to resolve executable path\n";
            return 1;
        }

        std::string message;
        std::string error;
        if (!putpath(exe.parent_path(), message, error)) {
            std::cerr << "putpath failed: " << error << "\n";
            return 1;
        }
        std::cout << message;
        return 0;
    }

    if (command == "update") {
        if (!perform_update(options)) {
            return 1;
        }
        return 0;
    }

    if (command == "--migration" || command == "migration") {
        run_migration(options.cwd, options.args);
        return 0;
    }

    if (command == "secret") {
        if (options.args.size() < 2) {
            std::cerr << "Use ote secret list|describe|add\n";
            return 1;
        }

        const std::string& subcommand = options.args[1];
        if (subcommand == "list") {
            print_secret_list(options.cwd);
            return 0;
        }

        if (subcommand == "describe") {
            if (options.args.size() < 3) {
                std::cerr << "secret describe requires a name\n";
                return 1;
            }
            print_secret_describe(options.cwd, options.args[2]);
            return 0;
        }

        if (subcommand == "add") {
            add_secret(options.cwd, options.args);
            return 0;
        }

        std::cerr << "Unknown secret command: " << subcommand << "\n";
        return 1;
    }

    if (command == "bridge") {
        if (options.args.size() < 2) {
            std::cerr << "Use ote bridge manifest|materialize|env|run\n";
            return 1;
        }

        const std::string profile = (options.args[1] == "run") ? extract_profile_from_env(options.cwd / ".env") : (options.args.size() >= 3 ? options.args[2] : extract_profile_from_env(options.cwd / ".env"));
        if (options.args[1] == "manifest") {
            print_layers_manifest(options.cwd, profile);
            return 0;
        }

        if (options.args[1] == "materialize") {
            print_layers_materialize(options.cwd, profile);
            return 0;
        }

        if (options.args[1] == "env") {
            print_layers_env(options.cwd, profile);
            return 0;
        }

        if (options.args[1] == "run") {
            run_bridge_command(options.cwd, options.args);
            return 0;
        }

        std::cerr << "Unknown bridge command: " << options.args[1] << "\n";
        return 1;
    }

    if (command == "api") {
        if (options.args.size() < 2) {
            std::cerr << "Use ote api manifest|secrets\n";
            return 1;
        }

        const std::string& subcommand = options.args[1];
        if (subcommand == "manifest") {
            print_manifest(options.cwd);
            return 0;
        }

        if (subcommand == "secrets") {
            print_secret_api(options.cwd);
            return 0;
        }

        std::cerr << "Unknown api command: " << subcommand << "\n";
        return 1;
    }

    if (command == "config") {
        if (options.args.size() < 2) {
            std::cerr << "Use ote config show\n";
            return 1;
        }
        if (options.args[1] == "show") {
            print_config_show(options.cwd);
            return 0;
        }
        std::cerr << "Unknown config command: " << options.args[1] << "\n";
        return 1;
    }

    if (command == "exec") {
        if (options.args.size() < 3) {
            std::cerr << "Use ote exec plan|run [shell] <command>\n";
            return 1;
        }

        const std::string& subcommand = options.args[1];
        std::string shell = default_shell();
        std::size_t command_index = 2;
        if (options.args.size() >= 4 && (options.args[2] == "powershell" || options.args[2] == "cmd" || options.args[2] == "bash" || options.args[2] == "sh")) {
            shell = options.args[2];
            command_index = 3;
        }

        const std::string payload = join_args(options.args, command_index);
        if (payload.empty()) {
            std::cerr << "exec requires a command\n";
            return 1;
        }

        if (subcommand == "plan") {
            print_plan(options.cwd, shell, payload);
            return 0;
        }

        if (subcommand == "run") {
            run_plan(options.cwd, shell, payload);
            return 0;
        }

        std::cerr << "Unknown exec command: " << subcommand << "\n";
        return 1;
    }

    if (command == "mcp") {
        if (options.args.size() < 2) {
            std::cerr << "Use ote mcp manifest|config|install|doctor|serve\n";
            return 1;
        }

        if (options.args[1] == "manifest") {
            std::cout << mcp_manifest_json(options.cwd, platform_name(), architecture_name(), active_secret_protector().name()) << "\n";
            return 0;
        }

        if (options.args[1] == "config") {
            print_mcp_config(options);
            return 0;
        }

        if (options.args[1] == "install") {
            return run_mcp_install(options) ? 0 : 1;
        }

        if (options.args[1] == "doctor") {
            return print_mcp_doctor(options) ? 0 : 1;
        }

        if (options.args[1] == "serve") {
            return run_mcp_server(options.cwd);
        }

        std::cerr << "Unknown mcp command: " << options.args[1] << "\n";
        return 1;
    }

    if (command == "--setup") {
        std::string error;
        if (!ConfigStore::ensure_layout(options.cwd, error)) {
            std::cerr << "setup failed: " << error << "\n";
            return 1;
        }

        if (ConfigStore::exists(options.cwd)) {
            Config config;
            if (!ConfigStore::load(options.cwd, config, error)) {
                std::cerr << "setup failed: " << error << "\n";
                return 1;
            }

            ConfigValidation validation;
            if (!ConfigStore::validate(config, validation) || !validation.valid) {
                std::cerr << "setup failed: existing config invalid\n";
                for (const std::string& issue : validation.issues) {
                    std::cerr << "- " << issue << "\n";
                }
                return 1;
            }

            std::cout << "OTE already initialized at " << options.cwd.string() << "\n";
            return 0;
        }

        if (!ConfigStore::write_default(options.cwd, error)) {
            std::cerr << "setup failed: " << error << "\n";
            return 1;
        }
        std::cout << "OTE initialized at " << options.cwd.string() << "\n";
        return 0;
    }

    std::cerr << "Unknown command: " << command << "\n";
    std::cerr << "Use ote --help\n";
    return 1;
}

}
