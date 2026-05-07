#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ote {

struct AppOptions {
    std::vector<std::string> args;
    std::filesystem::path cwd;
};

int run_app(const AppOptions& options);

}

