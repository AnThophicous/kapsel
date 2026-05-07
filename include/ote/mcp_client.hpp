#pragma once

#include <filesystem>
#include <string>

namespace ote {

struct McpInstallRequest {
    std::string target;
    std::filesystem::path command_path;
    std::filesystem::path cwd;
    std::filesystem::path config_path;
    std::string server_name = "ote";
};

std::string mcp_client_config_json(const std::filesystem::path& command_path, const std::filesystem::path& cwd, const std::string& server_name = "ote");
bool install_mcp_config(const McpInstallRequest& request, std::string& error, std::string& installed_message);
bool mcp_doctor(const std::filesystem::path& cwd, const std::filesystem::path& command_path, std::string& report, std::string& error);
std::filesystem::path mcp_target_config_path(const std::string& target, const std::filesystem::path& cwd, std::string& error);
std::string mcp_target_display_name(const std::string& target);

}
