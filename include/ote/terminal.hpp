#pragma once

#include <filesystem>
#include <string>

namespace ote {

void initialize_terminal();
bool terminal_supports_color();
std::string colorize(const std::string& text, const char* code);
std::string ascii_banner(const std::filesystem::path& root);

}
