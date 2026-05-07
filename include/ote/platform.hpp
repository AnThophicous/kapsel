#pragma once

#include <string>
#include <filesystem>

namespace ote {

std::string platform_name();
std::string architecture_name();
std::string default_shell();
std::filesystem::path executable_path();
std::filesystem::path user_home_path();

}
