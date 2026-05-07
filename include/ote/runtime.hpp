#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ote {

struct SecretDescriptor {
    std::string name;
    std::vector<std::string> exposed_keys;
    std::vector<std::string> tags;
};

struct CommandRequest {
    std::string shell;
    std::string command;
    std::filesystem::path working_directory;
    std::vector<std::string> allowed_env;
};

struct CommandPlan {
    std::string shell;
    std::string command;
    std::filesystem::path working_directory;
    std::vector<std::string> allowed_env;
    std::string execution_mode;
    bool sandboxed = true;
};

struct CommandResult {
    int exit_code = -1;
    std::string output;
    std::string error;
};

class ISecretCatalog {
public:
    virtual ~ISecretCatalog() = default;
    virtual std::vector<SecretDescriptor> list() const = 0;
};

class ISandboxBackend {
public:
    virtual ~ISandboxBackend() = default;
    virtual bool supported() const = 0;
    virtual std::string name() const = 0;
    virtual bool validate(const CommandPlan& plan, std::string& error) const = 0;
    virtual bool run(const CommandPlan& plan, CommandResult& result, std::string& error) const = 0;
};

class ProcessBroker {
public:
    static bool build_plan(const std::filesystem::path& root, const std::string& shell, const std::string& command, CommandPlan& plan, std::string& error);
    static bool validate_command(const CommandPlan& plan, std::string& error);
    static bool resolve_shell(const std::string& shell, std::string& resolved);
    static const ISandboxBackend& backend();
    static bool command_within_allowed_paths(const CommandPlan& plan, const std::vector<std::string>& allowed_paths, std::string& error);
    static std::vector<std::string> default_allowed_env();
};

}
