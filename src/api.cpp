#include "ote/api.hpp"

#include "ote/platform.hpp"

#include <sstream>

namespace ote {
namespace {

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

}

std::string secret_projection_json(const SecretProjection& secret) {
    std::ostringstream out;
    out << "{";
    out << "\"name\":\"" << escape_json(secret.name) << "\",";
    out << "\"tags\":" << to_json_array(secret.tags) << ",";
    out << "\"exposed_keys\":" << to_json_array(secret.exposed_keys) << ",";
    out << "\"protector\":\"" << escape_json(secret.protector) << "\"";
    out << "}";
    return out.str();
}

std::string mcp_manifest_json(const std::filesystem::path& root, const std::string& platform_name_value, const std::string& architecture_name_value, const std::string& protector_name) {
    std::ostringstream out;
    out << "{";
    out << "\"name\":\"ote\",";
    out << "\"root\":\"" << escape_json(root.string()) << "\",";
    out << "\"platform\":\"" << escape_json(platform_name_value) << "\",";
    out << "\"architecture\":\"" << escape_json(architecture_name_value) << "\",";
    out << "\"protector\":\"" << escape_json(protector_name) << "\",";
    out << "\"tools\":[";
    out << "{\"name\":\"secret.list\",\"mode\":\"readonly\",\"description\":\"List redacted secret projections\"},";
    out << "{\"name\":\"secret.describe\",\"mode\":\"readonly\",\"description\":\"Describe one redacted secret projection\"},";
    out << "{\"name\":\"secret.add\",\"mode\":\"trusted\",\"description\":\"Store a protected secret locally\"},";
    out << "{\"name\":\"exec.plan\",\"mode\":\"readonly\",\"description\":\"Build a policy-checked execution plan\"},";
    out << "{\"name\":\"exec.run\",\"mode\":\"trusted\",\"description\":\"Run a command through the broker\"},";
    out << "{\"name\":\"status\",\"mode\":\"readonly\",\"description\":\"Return runtime status\"},";
    out << "{\"name\":\"paths\",\"mode\":\"readonly\",\"description\":\"Return local OTE paths\"},";
    out << "{\"name\":\"config.show\",\"mode\":\"readonly\",\"description\":\"Return the active config snapshot\"}";
    out << "]";
    out << "}";
    return out.str();
}

}
