#include "ote/layers.hpp"

#include "ote/config.hpp"
#include "ote/secrets.hpp"

#include <cctype>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <utility>

namespace ote {
namespace {

struct ParsedEnvEntry {
    std::string key;
    std::string value;
};

bool starts_with(const std::string& value, const std::string& prefix) {
    return value.size() >= prefix.size() && value.compare(0, prefix.size(), prefix) == 0;
}

std::string trim(const std::string& value) {
    std::size_t begin = 0;
    while (begin < value.size() && std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }
    std::size_t end = value.size();
    while (end > begin && std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return value.substr(begin, end - begin);
}

bool parse_quoted_value(const std::string& text, std::string& value) {
    if (text.size() < 2) {
        return false;
    }

    const char quote = text.front();
    if (quote != '"' && quote != '\'') {
        return false;
    }
    if (text.back() != quote) {
        return false;
    }

    std::string out;
    out.reserve(text.size() - 2);
    bool escaped = false;
    for (std::size_t i = 1; i + 1 < text.size(); ++i) {
        const char c = text[i];
        if (escaped) {
            switch (c) {
            case 'n': out += '\n'; break;
            case 'r': out += '\r'; break;
            case 't': out += '\t'; break;
            case '\\': out += '\\'; break;
            case '"': out += '"'; break;
            case '\'': out += '\''; break;
            default: out += c; break;
            }
            escaped = false;
            continue;
        }
        if (c == '\\') {
            escaped = true;
            continue;
        }
        out += c;
    }
    if (escaped) {
        return false;
    }
    value = out;
    return true;
}

bool parse_env_line(const std::string& line, ParsedEnvEntry& entry) {
    const std::string text = trim(line);
    if (text.empty() || text.front() == '#') {
        return false;
    }

    const std::size_t pos = text.find('=');
    if (pos == std::string::npos || pos == 0) {
        return false;
    }

    entry.key = trim(text.substr(0, pos));
    entry.value = trim(text.substr(pos + 1));
    if (entry.key.empty()) {
        return false;
    }

    if (!entry.value.empty() && (entry.value.front() == '"' || entry.value.front() == '\'')) {
        std::string parsed;
        if (parse_quoted_value(entry.value, parsed)) {
            entry.value = parsed;
        }
    }

    return true;
}

std::vector<ParsedEnvEntry> parse_env_file(const std::filesystem::path& path, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "unable to open env file";
        return {};
    }

    std::vector<ParsedEnvEntry> entries;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        ParsedEnvEntry entry;
        if (parse_env_line(line, entry)) {
            entries.push_back(std::move(entry));
        }
    }

    return entries;
}

bool write_text(const std::filesystem::path& path, const std::string& text, std::string& error) {
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        error = "unable to write layer file";
        return false;
    }

    file << text;
    if (!file.good()) {
        error = "unable to flush layer file";
        return false;
    }

    return true;
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

std::string proxy_env_text(const std::string& profile) {
    std::ostringstream out;
    out << "# Managed by OTE\n";
    out << "OTE_PROFILE=" << profile << "\n";
    return out.str();
}

std::filesystem::path manifest_path_for(const std::filesystem::path& root, const std::string& profile) {
    return ConfigStore::layers_dir(root) / (profile + ".json");
}

}

bool migrate_layers(const std::filesystem::path& root, const LayerMigrationRequest& request, LayerMigrationResult& result, std::string& error) {
    const std::filesystem::path source = request.source_path.empty() ? (root / ".env") : request.source_path;
    const std::string profile = request.profile.empty() ? "prod" : request.profile;
    const std::string secret_name = "layers-" + profile;

    std::vector<ParsedEnvEntry> entries = parse_env_file(source, error);
    if (entries.empty() && !std::filesystem::exists(source)) {
        return false;
    }

    SecretDraft draft;
    draft.name = secret_name;
    draft.tags = { "layers", "env", profile };
    for (const ParsedEnvEntry& entry : entries) {
        if (entry.key == "OTE_PROFILE" || starts_with(entry.key, "OTE_")) {
            continue;
        }
        draft.values.emplace_back(entry.key, entry.value);
    }

    if (draft.values.empty()) {
        error = "no migratable env entries found";
        return false;
    }

    if (!SecretVault::store(root, active_secret_protector(), draft, error)) {
        return false;
    }

    std::error_code ec;
    std::filesystem::create_directories(ConfigStore::layers_dir(root), ec);
    if (ec) {
        error = "unable to create layers directory";
        return false;
    }

    const std::filesystem::path manifest = manifest_path_for(root, profile);
    std::ostringstream manifest_text;
    manifest_text << "{";
    manifest_text << "\"version\":1,";
    manifest_text << "\"profile\":\"" << escape_json(profile) << "\",";
    manifest_text << "\"source\":\"" << escape_json(source.string()) << "\",";
    manifest_text << "\"secret_name\":\"" << escape_json(secret_name) << "\",";
    manifest_text << "\"imported_count\":" << draft.values.size() << ",";
    manifest_text << "\"managed_by\":\"OTE\"";
    manifest_text << "}";

    if (!write_text(manifest, manifest_text.str(), error)) {
        return false;
    }

    if (!write_text(source, proxy_env_text(profile), error)) {
        return false;
    }

    result.profile = profile;
    result.secret_name = secret_name;
    result.source_path = source;
    result.manifest_path = manifest;
    result.imported_count = draft.values.size();
    return true;
}

bool materialize_layers(const std::filesystem::path& root, const std::string& profile, std::vector<std::pair<std::string, std::string>>& values, std::string& error) {
    std::vector<std::pair<std::string, std::string>> loaded;
    if (!SecretVault::load_values(root, "layers-" + profile, loaded, error)) {
        return false;
    }

    values = std::move(loaded);
    return true;
}

std::string layers_manifest_json(const std::filesystem::path& root, const std::string& profile, const std::string& source_path, const std::string& secret_name, std::size_t imported_count) {
    std::ostringstream out;
    out << "{";
    out << "\"root\":\"" << escape_json(root.string()) << "\",";
    out << "\"profile\":\"" << escape_json(profile) << "\",";
    out << "\"source\":\"" << escape_json(source_path) << "\",";
    out << "\"secret_name\":\"" << escape_json(secret_name) << "\",";
    out << "\"imported_count\":" << imported_count << ",";
    out << "\"bridge\":\"layers-ote\"";
    out << "}";
    return out.str();
}

}
