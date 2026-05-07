#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace ote {

struct LayerMigrationRequest {
    std::filesystem::path source_path;
    std::string profile;
};

struct LayerMigrationResult {
    std::string profile;
    std::string secret_name;
    std::filesystem::path source_path;
    std::filesystem::path manifest_path;
    std::size_t imported_count = 0;
};

bool migrate_layers(const std::filesystem::path& root, const LayerMigrationRequest& request, LayerMigrationResult& result, std::string& error);
bool materialize_layers(const std::filesystem::path& root, const std::string& profile, std::vector<std::pair<std::string, std::string>>& values, std::string& error);
std::string layers_manifest_json(const std::filesystem::path& root, const std::string& profile, const std::string& source_path, const std::string& secret_name, std::size_t imported_count);

}

