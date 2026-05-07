#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace ote {

struct AppOptions {
    std::vector<std::string> args;
    std::filesystem::path cwd;
    std::filesystem::path executable;
};

int run_app(const AppOptions& options);

}
