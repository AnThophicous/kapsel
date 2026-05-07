#include "ote/app.hpp"

#include "ote/api.hpp"
#include "ote/config.hpp"
#include "ote/layers.hpp"
#include "ote/mcp.hpp"
#include "ote/platform.hpp"
#include "ote/runtime.hpp"
#include "ote/secrets.hpp"
#include "ote/terminal.hpp"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

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
    std::cout << "  " << colorize("ote --migration [--profile <name>] [.env path]", "32") << "\n";
    std::cout << "  " << colorize("ote config show", "32") << "\n";
    std::cout << "  " << colorize("ote secret list", "32") << "\n";
    std::cout << "  " << colorize("ote secret describe <name>", "32") << "\n";
    std::cout << "  " << colorize("ote secret add <name> [--tag <tag>] KEY=VALUE...", "32") << "\n";
    std::cout << "  " << colorize("ote bridge manifest [profile]", "32") << "\n";
    std::cout << "  " << colorize("ote bridge materialize [profile]", "32") << "\n";
    std::cout << "  " << colorize("ote api manifest", "32") << "\n";
    std::cout << "  " << colorize("ote api secrets", "32") << "\n";
    std::cout << "  " << colorize("ote exec plan <command>", "32") << "\n";
    std::cout << "  " << colorize("ote exec run <command>", "32") << "\n";
    std::cout << "  " << colorize("ote mcp manifest", "32") << "\n";
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
            std::cerr << "Use ote bridge manifest|materialize\n";
            return 1;
        }

        const std::string profile = options.args.size() >= 3 ? options.args[2] : extract_profile_from_env(options.cwd / ".env");
        if (options.args[1] == "manifest") {
            print_layers_manifest(options.cwd, profile);
            return 0;
        }

        if (options.args[1] == "materialize") {
            print_layers_materialize(options.cwd, profile);
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
            std::cerr << "Use ote mcp manifest|serve\n";
            return 1;
        }

        if (options.args[1] == "manifest") {
            std::cout << mcp_manifest_json(options.cwd, platform_name(), architecture_name(), active_secret_protector().name()) << "\n";
            return 0;
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
