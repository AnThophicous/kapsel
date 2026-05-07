#pragma once

#include "ote/secrets.hpp"

#include <filesystem>
#include <string>

namespace ote {

std::string mcp_manifest_json(const std::filesystem::path& root, const std::string& platform_name, const std::string& architecture_name, const std::string& protector_name);
std::string secret_projection_json(const SecretProjection& secret);

}
