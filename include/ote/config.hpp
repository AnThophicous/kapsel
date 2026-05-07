#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ote {

struct SandboxConfig {
    bool enabled = true;
    std::vector<std::string> allowed_paths;
    std::vector<std::string> denied_paths;
    std::vector<std::string> allowed_env;
};

struct RuntimeConfig {
    std::string default_shell;
    std::string execution_mode;
    std::string cache_dir;
};

struct Config {
    int version = 1;
    SandboxConfig sandbox;
    RuntimeConfig runtime;
};

struct ConfigValidation {
    bool valid = false;
    std::vector<std::string> issues;
};

class ConfigStore {
public:
    static std::filesystem::path config_path(const std::filesystem::path& root);
    static std::filesystem::path store_dir(const std::filesystem::path& root);
    static std::filesystem::path vault_dir(const std::filesystem::path& root);
    static std::filesystem::path state_dir(const std::filesystem::path& root);
    static std::filesystem::path cache_dir(const std::filesystem::path& root);
    static std::filesystem::path logs_dir(const std::filesystem::path& root);
    static std::filesystem::path layers_dir(const std::filesystem::path& root);
    static std::filesystem::path lock_file(const std::filesystem::path& root);

    static bool exists(const std::filesystem::path& root);
    static bool ensure_layout(const std::filesystem::path& root, std::string& error);
    static bool write_default(const std::filesystem::path& root, std::string& error);
    static bool load(const std::filesystem::path& root, Config& config, std::string& error);
    static bool save(const std::filesystem::path& root, const Config& config, std::string& error);
    static bool validate(const Config& config, ConfigValidation& validation);
};

}
