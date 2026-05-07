#include "ote/app.hpp"

#include <filesystem>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    ote::AppOptions options;
    options.cwd = std::filesystem::current_path();
    for (int i = 1; i < argc; ++i) {
        options.args.emplace_back(argv[i]);
    }
    return ote::run_app(options);
}

