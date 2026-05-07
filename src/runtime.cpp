#include "ote/runtime.hpp"

#include "ote/config.hpp"

#include <chrono>
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <ctime>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#else
#include <sys/wait.h>
#include <unistd.h>
extern char **environ;
#endif

namespace ote {
namespace {

bool is_simple_path_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '-' || c == '.' || c == '\\' || c == '/' || c == ':' || c == ' ';
}

bool contains_blocked_sequence(const std::string& command) {
    static const std::vector<std::string> blocked = {
        "&&", "||", "|", ";", "`", "$(", ">$", "<", "\n", "\r"
    };
    for (const std::string& token : blocked) {
        if (command.find(token) != std::string::npos) {
            return true;
        }
    }
    return false;
}

bool is_safe_command_text(const std::string& command) {
    if (command.empty()) {
        return false;
    }

    if (contains_blocked_sequence(command)) {
        return false;
    }

    for (char c : command) {
        if (is_simple_path_char(c)) {
            continue;
        }
        if (c == '"' || c == '\'' || c == '=' || c == ',' || c == '@' || c == '%' || c == '$' || c == '(' || c == ')' || c == '[' || c == ']' || c == '{' || c == '}' || c == '+' || c == '*' || c == '!' || c == '?') {
            continue;
        }
        return false;
    }

    return true;
}

std::string to_lower_ascii(std::string value) {
    for (char& c : value) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return value;
}

bool starts_with(const std::string& value, const std::string& prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

bool is_safe_mode_command(const std::string& command) {
    const std::string normalized = to_lower_ascii(command);
    static const std::vector<std::string> allowed = {
        "echo ",
        "pwd",
        "ls",
        "cat ",
        "dir",
        "type ",
        "where ",
        "git status",
        "git branch",
        "git rev-parse",
        "git diff",
        "git log",
        "git show",
        "cmake -s",
        "cmake -b",
        "cmake --build",
        "node --version",
        "npm --version",
        "python --version",
        "dotnet --info",
        "get-location",
        "get-childitem",
        "select-string "
    };

    for (const std::string& prefix : allowed) {
        if (starts_with(normalized, prefix)) {
            return true;
        }
    }

    return false;
}

bool contains_dangerous_verb(const std::string& command) {
    const std::string normalized = to_lower_ascii(command);
    static const std::vector<std::string> verbs = {
        "del ", "erase ", "rmdir ", "rd ", "rm ", "mv ", "move ", "cp ", "copy ",
        "remove-item", "set-item ", "format ", "diskpart", "shutdown", "reboot",
        "reboot", "poweroff", "kill ", "taskkill", "sc delete", "chmod ", "chown ",
        "dd ", "mkfs", "fdisk", "sudo ", "su ", "reg delete", "git reset", "git clean"
    };

    for (const std::string& verb : verbs) {
        if (normalized.find(verb) != std::string::npos) {
            return true;
        }
    }

    return false;
}

bool contains_high_risk_install_pattern(const std::string& command) {
    const std::string normalized = to_lower_ascii(command);
    static const std::vector<std::string> patterns = {
        "invoke-webrequest",
        "irm ",
        "iex",
        "invoke-expression",
        "curl ",
        "wget ",
        "npm install",
        "pnpm add",
        "yarn add",
        "pip install",
        "uv pip install",
        "dotnet tool install",
        "cargo install",
        "git clone",
        "git pull",
        "nuget install"
    };

    for (const std::string& pattern : patterns) {
        if (normalized.find(pattern) != std::string::npos) {
            return true;
        }
    }

    return false;
}

bool contains_remote_script_pipeline(const std::string& command) {
    const std::string normalized = to_lower_ascii(command);
    static const std::vector<std::string> patterns = {
        "| iex",
        "| invoke-expression",
        "| sh",
        "| bash",
        "curl http",
        "curl https",
        "wget http",
        "wget https"
    };

    for (const std::string& pattern : patterns) {
        if (normalized.find(pattern) != std::string::npos) {
            return true;
        }
    }

    return false;
}

void append_reason(std::vector<std::string>& reasons, const std::string& reason) {
    if (std::find(reasons.begin(), reasons.end(), reason) == reasons.end()) {
        reasons.push_back(reason);
    }
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

bool is_safe_allowed_env(const std::string& name) {
    if (name.empty()) {
        return false;
    }
    return std::all_of(name.begin(), name.end(), [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
    });
}

bool is_safe_extra_env_name(const std::string& name) {
    return is_safe_allowed_env(name);
}

std::vector<std::string> default_allowed_env() {
    return { "PATH", "SYSTEMROOT", "WINDIR", "HOME", "USERPROFILE", "TMP", "TEMP" };
}

std::vector<std::pair<std::string, std::string>> collect_extra_env(const CommandPlan& plan) {
    return plan.extra_env;
}

std::vector<std::pair<std::string, std::string>> collect_allowed_env(const CommandPlan& plan) {
    std::vector<std::pair<std::string, std::string>> env;
    for (const std::string& name : plan.allowed_env) {
        if (const char* value = std::getenv(name.c_str())) {
            env.emplace_back(name, value);
        }
    }
    for (const auto& entry : collect_extra_env(plan)) {
        auto it = std::find_if(env.begin(), env.end(), [&](const auto& existing) {
            return existing.first == entry.first;
        });
        if (it != env.end()) {
            it->second = entry.second;
        } else {
            env.push_back(entry);
        }
    }
    return env;
}

std::string escape_json(const std::string& value);

std::string current_timestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t raw = std::chrono::system_clock::to_time_t(now);
    std::tm tm {};
#if defined(_WIN32)
    localtime_s(&tm, &raw);
#else
    localtime_r(&raw, &tm);
#endif
    char buffer[32];
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S%z", &tm) == 0) {
        return "unknown";
    }
    return buffer;
}

std::string current_date_stamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t raw = std::chrono::system_clock::to_time_t(now);
    std::tm tm {};
#if defined(_WIN32)
    localtime_s(&tm, &raw);
#else
    localtime_r(&raw, &tm);
#endif
    char buffer[16];
    if (std::strftime(buffer, sizeof(buffer), "%Y-%m-%d", &tm) == 0) {
        return "unknown";
    }
    return buffer;
}

unsigned long process_identifier() {
#if defined(_WIN32)
    return static_cast<unsigned long>(GetCurrentProcessId());
#else
    return static_cast<unsigned long>(getpid());
#endif
}

std::string json_array(const std::vector<std::string>& values) {
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

} // anonymous namespace

CommandAssessment assess_command_policy(const CommandPlan& plan) {
    CommandAssessment assessment;
    assessment.risk = "low";

    if (contains_blocked_sequence(plan.command)) {
        assessment.risk = "critical";
        assessment.blocked = true;
        append_reason(assessment.reasons, "blocked syntax");
    }

    if (contains_dangerous_verb(plan.command)) {
        assessment.risk = "critical";
        assessment.blocked = true;
        append_reason(assessment.reasons, "destructive verb");
    }

    if (contains_remote_script_pipeline(plan.command)) {
        assessment.risk = "critical";
        assessment.blocked = true;
        append_reason(assessment.reasons, "remote script pipeline");
    }

    if (contains_high_risk_install_pattern(plan.command)) {
        if (assessment.risk != "critical") {
            assessment.risk = "high";
        }
        append_reason(assessment.reasons, "networked install or fetch");
    }

    if (plan.execution_mode == "permission") {
        if (assessment.risk == "low") {
            assessment.risk = "medium";
        }
        append_reason(assessment.reasons, "interactive approval required");
    } else if (plan.execution_mode == "bypass" && assessment.risk == "low") {
        assessment.risk = "medium";
        append_reason(assessment.reasons, "approval bypassed");
    }

    if (assessment.risk == "low" && !is_safe_mode_command(plan.command)) {
        assessment.risk = "medium";
        append_reason(assessment.reasons, "not in safe allowlist");
    }

    return assessment;
}

std::string command_assessment_json(const CommandAssessment& assessment) {
    std::ostringstream out;
    out << "{";
    out << "\"risk\":\"" << escape_json(assessment.risk) << "\",";
    out << "\"blocked\":" << (assessment.blocked ? "true" : "false") << ",";
    out << "\"reasons\":" << json_array(assessment.reasons);
    out << "}";
    return out.str();
}

bool append_audit_entry(const std::filesystem::path& root, const CommandPlan& plan, const CommandAssessment& assessment, const CommandResult& result, std::string& error) {
    std::error_code ec;
    const std::filesystem::path log_dir = ConfigStore::logs_dir(root) / current_date_stamp();
    std::filesystem::create_directories(log_dir, ec);
    if (ec) {
        error = "unable to create audit log directory";
        return false;
    }

    const std::filesystem::path log_file = log_dir / ("run-" + std::to_string(process_identifier()) + ".jsonl");
    std::ofstream file(log_file, std::ios::binary | std::ios::app);
    if (!file) {
        error = "unable to open audit log";
        return false;
    }

    std::ostringstream entry;
    entry << "{";
    entry << "\"time\":\"" << escape_json(current_timestamp()) << "\",";
    entry << "\"command\":\"" << escape_json(plan.command) << "\",";
    entry << "\"shell\":\"" << escape_json(plan.shell) << "\",";
    entry << "\"cwd\":\"" << escape_json(plan.working_directory.string()) << "\",";
    entry << "\"exit_code\":" << result.exit_code << ",";
    entry << "\"risk\":\"" << escape_json(assessment.risk) << "\",";
    entry << "\"blocked\":" << (assessment.blocked ? "true" : "false") << ",";
    entry << "\"reasons\":" << json_array(assessment.reasons) << ",";
    entry << "\"files_changed\":[],";
    entry << "\"network\":[]";
    entry << "}";

    file << entry.str() << "\n";
    if (!file.good()) {
        error = "unable to write audit log";
        return false;
    }

    return true;
}

#if !defined(_WIN32)
void reset_child_environment() {
    for (char** env = environ; env != nullptr && *env != nullptr; ++env) {
        const std::string entry(*env);
        const std::size_t pos = entry.find('=');
        if (pos == std::string::npos) {
            continue;
        }
        const std::string name = entry.substr(0, pos);
        unsetenv(name.c_str());
    }
}
#endif

bool path_within_prefix(const std::filesystem::path& root, const std::filesystem::path& child) {
    std::error_code ec;
    const std::filesystem::path canonical_root = std::filesystem::weakly_canonical(root, ec);
    if (ec) {
        return false;
    }
    const std::filesystem::path canonical_child = std::filesystem::weakly_canonical(child, ec);
    if (ec) {
        return false;
    }

    auto root_it = canonical_root.begin();
    auto child_it = canonical_child.begin();
    for (; root_it != canonical_root.end() && child_it != canonical_child.end(); ++root_it, ++child_it) {
        if (*root_it != *child_it) {
            return false;
        }
    }

    return root_it == canonical_root.end();
}

bool path_is_denied(const std::filesystem::path& path, const std::vector<std::string>& denied_paths) {
    for (const std::string& denied : denied_paths) {
        if (denied.empty()) {
            continue;
        }
        if (path_within_prefix(denied, path)) {
            return true;
        }
    }
    return false;
}

std::string base64_encode(const std::vector<unsigned char>& data) {
    static constexpr char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    std::string out;
    out.reserve(((data.size() + 2U) / 3U) * 4U);

    std::size_t i = 0;
    while (i + 3U <= data.size()) {
        const unsigned int n = (static_cast<unsigned int>(data[i]) << 16U) |
                               (static_cast<unsigned int>(data[i + 1]) << 8U) |
                               static_cast<unsigned int>(data[i + 2]);
        out.push_back(alphabet[(n >> 18U) & 0x3FU]);
        out.push_back(alphabet[(n >> 12U) & 0x3FU]);
        out.push_back(alphabet[(n >> 6U) & 0x3FU]);
        out.push_back(alphabet[n & 0x3FU]);
        i += 3U;
    }

    const std::size_t remaining = data.size() - i;
    if (remaining == 1U) {
        const unsigned int n = static_cast<unsigned int>(data[i]) << 16U;
        out.push_back(alphabet[(n >> 18U) & 0x3FU]);
        out.push_back(alphabet[(n >> 12U) & 0x3FU]);
        out.push_back('=');
        out.push_back('=');
    } else if (remaining == 2U) {
        const unsigned int n = (static_cast<unsigned int>(data[i]) << 16U) |
                               (static_cast<unsigned int>(data[i + 1]) << 8U);
        out.push_back(alphabet[(n >> 18U) & 0x3FU]);
        out.push_back(alphabet[(n >> 12U) & 0x3FU]);
        out.push_back(alphabet[(n >> 6U) & 0x3FU]);
        out.push_back('=');
    }

    return out;
}

std::string powershell_encoded_command(const std::string& command) {
#if defined(_WIN32)
    const int required = MultiByteToWideChar(CP_UTF8, 0, command.c_str(), -1, nullptr, 0);
    if (required <= 0) {
        return {};
    }

    std::wstring wide(static_cast<std::size_t>(required - 1), L'\0');
    if (MultiByteToWideChar(CP_UTF8, 0, command.c_str(), -1, wide.data(), required) <= 0) {
        return {};
    }

    std::vector<unsigned char> bytes;
    bytes.reserve(wide.size() * 2U);
    for (wchar_t ch : wide) {
        bytes.push_back(static_cast<unsigned char>(ch & 0xFF));
        bytes.push_back(static_cast<unsigned char>((static_cast<unsigned int>(ch) >> 8U) & 0xFF));
    }

    return base64_encode(bytes);
#else
    (void)command;
    return {};
#endif
}

#if defined(_WIN32)
std::string decode_powershell_output(const std::string& raw) {
    if (raw.empty()) {
        return {};
    }

    std::vector<wchar_t> wide;
    wide.reserve(raw.size() / 2U);
    for (std::size_t i = 0; i + 1U < raw.size(); i += 2U) {
        const unsigned char lo = static_cast<unsigned char>(raw[i]);
        const unsigned char hi = static_cast<unsigned char>(raw[i + 1]);
        wide.push_back(static_cast<wchar_t>(static_cast<unsigned short>(lo | (static_cast<unsigned short>(hi) << 8U))));
    }

    if (wide.empty()) {
        return {};
    }

    const int required = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    if (required <= 0) {
        return raw;
    }

    std::string utf8(static_cast<std::size_t>(required), '\0');
    if (WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), required, nullptr, nullptr) <= 0) {
        return raw;
    }

    return utf8;
}
#endif

#if !defined(_WIN32)
std::string read_pipe_output(int fd) {
    std::string output;
    char buffer[4096];
    ssize_t bytes = 0;
    while ((bytes = ::read(fd, buffer, sizeof(buffer))) > 0) {
        output.append(buffer, buffer + bytes);
    }
    return output;
}
#endif

class SystemSandboxBackend final : public ISandboxBackend {
public:
    bool supported() const override {
        return true;
    }

    std::string name() const override {
        return "system";
    }

    bool validate(const CommandPlan& plan, std::string& error) const override {
        return ProcessBroker::validate_command(plan, error);
    }

    bool run(const CommandPlan& plan, CommandResult& result, std::string& error) const override {
#if defined(_WIN32)
        if (plan.shell != "powershell" && plan.shell != "cmd") {
            error = "unsupported shell";
            return false;
        }

        std::ostringstream cmdline;
        if (plan.shell == "powershell") {
            const std::string encoded = powershell_encoded_command(plan.command);
            if (encoded.empty()) {
                error = "unable to encode command";
                return false;
            }
            cmdline << "powershell -NoProfile -NonInteractive -ExecutionPolicy Bypass -EncodedCommand " << encoded;
        } else {
            cmdline << "cmd /d /s /c " << plan.command;
        }

        std::string environment_block;
        for (const auto& entry : collect_allowed_env(plan)) {
            environment_block.append(entry.first);
            environment_block.push_back('=');
            environment_block.append(entry.second);
            environment_block.push_back('\0');
        }
        environment_block.push_back('\0');

        SECURITY_ATTRIBUTES sa {};
        sa.nLength = sizeof(sa);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = nullptr;

        HANDLE read_pipe = nullptr;
        HANDLE write_pipe = nullptr;
        if (!CreatePipe(&read_pipe, &write_pipe, &sa, 0)) {
            error = "failed to create output pipe";
            return false;
        }
        SetHandleInformation(read_pipe, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = write_pipe;
        si.hStdError = write_pipe;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        PROCESS_INFORMATION pi {};

        std::string command_line = cmdline.str();
        std::string current_dir = plan.working_directory.string();
        if (current_dir.empty()) {
            current_dir = ".";
        }

        if (!CreateProcessA(
                nullptr,
                command_line.data(),
                nullptr,
                nullptr,
                TRUE,
                CREATE_NO_WINDOW,
                environment_block.empty() ? nullptr : environment_block.data(),
                current_dir.c_str(),
                &si,
                &pi)) {
            CloseHandle(write_pipe);
            CloseHandle(read_pipe);
            error = "process launch failed";
            return false;
        }

        CloseHandle(write_pipe);
        WaitForSingleObject(pi.hProcess, INFINITE);
        DWORD exit_code = 1;
        if (!GetExitCodeProcess(pi.hProcess, &exit_code)) {
            exit_code = 1;
        }
        std::string output;
        char buffer[4096];
        DWORD bytes_read = 0;
        while (ReadFile(read_pipe, buffer, sizeof(buffer), &bytes_read, nullptr) && bytes_read > 0) {
            output.append(buffer, buffer + bytes_read);
        }
        CloseHandle(read_pipe);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        result.exit_code = static_cast<int>(exit_code);
        if (plan.shell == "powershell") {
            result.output = decode_powershell_output(output);
        } else {
            result.output = output;
        }
        return true;
#else
        int stdout_pipe[2];
        if (pipe(stdout_pipe) != 0) {
            error = "failed to create output pipe";
            return false;
        }

        pid_t pid = fork();
        if (pid < 0) {
            close(stdout_pipe[0]);
            close(stdout_pipe[1]);
            error = "fork failed";
            return false;
        }

        if (pid == 0) {
            reset_child_environment();
            for (const auto& entry : collect_allowed_env(plan)) {
                setenv(entry.first.c_str(), entry.second.c_str(), 1);
            }
            dup2(stdout_pipe[1], STDOUT_FILENO);
            dup2(stdout_pipe[1], STDERR_FILENO);
            close(stdout_pipe[0]);
            close(stdout_pipe[1]);

            if (plan.shell == "bash") {
                execlp("bash", "bash", "-lc", plan.command.c_str(), static_cast<char*>(nullptr));
            } else {
                execlp("sh", "sh", "-lc", plan.command.c_str(), static_cast<char*>(nullptr));
            }
            _exit(127);
        }

        close(stdout_pipe[1]);
        std::string output = read_pipe_output(stdout_pipe[0]);
        close(stdout_pipe[0]);

        int status = 0;
        if (waitpid(pid, &status, 0) < 0) {
            error = "waitpid failed";
            return false;
        }

        result.exit_code = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        result.output = output;
        return true;
#endif
    }
};

bool ProcessBroker::resolve_shell(const std::string& shell, std::string& resolved) {
    if (shell.empty()) {
        return false;
    }

#if defined(_WIN32)
    if (shell != "powershell" && shell != "cmd") {
        return false;
    }
#else
    if (shell != "bash" && shell != "sh") {
        return false;
    }
#endif

    resolved = shell;
    return true;
}

bool ProcessBroker::validate_command(const CommandPlan& plan, std::string& error) {
    std::string resolved_shell;
    if (!resolve_shell(plan.shell, resolved_shell)) {
        error = "invalid shell";
        return false;
    }

    if (!std::filesystem::exists(plan.working_directory)) {
        error = "working directory does not exist";
        return false;
    }

    if (!is_safe_command_text(plan.command)) {
        error = "command contains blocked syntax";
        return false;
    }

    const CommandAssessment assessment = assess_command_policy(plan);
    if (assessment.blocked) {
        if (std::find(assessment.reasons.begin(), assessment.reasons.end(), "remote script pipeline") != assessment.reasons.end()) {
            error = "command uses a blocked remote script pipeline";
            return false;
        }
        if (std::find(assessment.reasons.begin(), assessment.reasons.end(), "destructive verb") != assessment.reasons.end()) {
            error = "command uses a blocked destructive verb";
            return false;
        }
        if (std::find(assessment.reasons.begin(), assessment.reasons.end(), "blocked syntax") != assessment.reasons.end()) {
            error = "command contains blocked syntax";
            return false;
        }
        error = "command blocked by policy";
        return false;
    }

    if (plan.execution_mode == "safe" && !is_safe_mode_command(plan.command)) {
        error = "command not allowed in safe mode";
        return false;
    }

    for (const std::string& name : plan.allowed_env) {
        if (!is_safe_allowed_env(name)) {
            error = "invalid allowed env entry";
            return false;
        }
    }

    for (const auto& entry : plan.extra_env) {
        if (!is_safe_extra_env_name(entry.first)) {
            error = "invalid extra env entry";
            return false;
        }
    }

    return true;
}

bool ProcessBroker::build_plan(const std::filesystem::path& root, const std::string& shell, const std::string& command, CommandPlan& plan, std::string& error) {
    Config config;
    if (!ConfigStore::load(root, config, error)) {
        return false;
    }

    if (!config.sandbox.enabled) {
        error = "sandbox disabled";
        return false;
    }

    if (!resolve_shell(shell.empty() ? config.runtime.default_shell : shell, plan.shell)) {
        error = "shell not permitted";
        return false;
    }

    plan.command = command;
    plan.working_directory = root;
    plan.allowed_env = config.sandbox.allowed_env.empty() ? default_allowed_env() : config.sandbox.allowed_env;
    plan.execution_mode = config.runtime.execution_mode.empty() ? "safe" : config.runtime.execution_mode;
    plan.sandboxed = true;

    if (path_is_denied(plan.working_directory, config.sandbox.denied_paths)) {
        error = "working directory denied";
        return false;
    }

    if (!command_within_allowed_paths(plan, config.sandbox.allowed_paths, error)) {
        return false;
    }

    if (!validate_command(plan, error)) {
        return false;
    }

    return true;
}

bool ProcessBroker::command_within_allowed_paths(const CommandPlan& plan, const std::vector<std::string>& allowed_paths, std::string& error) {
    if (allowed_paths.empty()) {
        error = "no allowed paths configured";
        return false;
    }

    for (const std::string& entry : allowed_paths) {
        if (entry.empty()) {
            continue;
        }
        if (path_within_prefix(entry, plan.working_directory)) {
            return true;
        }
    }

    error = "working directory not allowed";
    return false;
}

const ISandboxBackend& ProcessBroker::backend() {
    static SystemSandboxBackend instance;
    return instance;
}

std::vector<std::string> ProcessBroker::default_allowed_env() {
    return ote::default_allowed_env();
}

}
