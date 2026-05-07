#include "ote/secrets.hpp"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <set>
#include <sstream>
#include <string>
#include <system_error>

#if defined(_WIN32)
#include <windows.h>
#include <wincrypt.h>
#endif

namespace ote {
namespace {

bool is_identifier_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_' || c == '-' || c == '.';
}

bool is_env_char(char c) {
    return std::isalnum(static_cast<unsigned char>(c)) != 0 || c == '_';
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

std::string join_json_array(const std::vector<std::string>& values) {
    std::ostringstream out;
    out << "[";
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << "\"" << escape_json(values[i]) << "\"";
    }
    out << "]";
    return out.str();
}

std::string join_json_object(const std::vector<std::pair<std::string, std::string>>& values) {
    std::ostringstream out;
    out << "{";
    for (std::size_t i = 0; i < values.size(); ++i) {
        if (i > 0) {
            out << ",";
        }
        out << "\"" << escape_json(values[i].first) << "\":\"" << escape_json(values[i].second) << "\"";
    }
    out << "}";
    return out.str();
}

bool read_text_file(const std::filesystem::path& path, std::string& text, std::string& error) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        error = "unable to open secret record";
        return false;
    }

    std::ostringstream buffer;
    buffer << file.rdbuf();
    text = buffer.str();
    return true;
}

std::string safe_blob_name(const std::string& name) {
    std::string out = name;
    for (char& c : out) {
        if (!is_identifier_char(c)) {
            c = '_';
        }
    }
    if (out.empty()) {
        out = "secret";
    }
    return out;
}

std::vector<std::string> split_lines(const std::string& text) {
    std::vector<std::string> lines;
    std::istringstream stream(text);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(line);
    }
    return lines;
}

std::string encode_hex(const std::string& input) {
    static constexpr char hex[] = "0123456789abcdef";
    std::string out;
    out.reserve(input.size() * 2);
    for (unsigned char c : input) {
        out.push_back(hex[(c >> 4) & 0x0F]);
        out.push_back(hex[c & 0x0F]);
    }
    return out;
}

bool decode_hex(const std::string& input, std::string& output) {
    if ((input.size() % 2U) != 0U) {
        return false;
    }
    output.clear();
    output.reserve(input.size() / 2U);
    for (std::size_t i = 0; i < input.size(); i += 2) {
        const char hi = input[i];
        const char lo = input[i + 1];
        const auto to_val = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
            if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
            return -1;
        };
        const int high = to_val(hi);
        const int low = to_val(lo);
        if (high < 0 || low < 0) {
            return false;
        }
        output.push_back(static_cast<char>((high << 4) | low));
    }
    return true;
}

bool parse_secret_record(const std::string& text, SecretProjection& projection, std::string& payload_hex, std::string& error) {
    projection = {};
    payload_hex.clear();
    const std::vector<std::string> lines = split_lines(text);
    for (const std::string& line : lines) {
        if (line.rfind("name=", 0) == 0) {
            projection.name = line.substr(5);
            continue;
        }
        if (line.rfind("tag=", 0) == 0) {
            projection.tags.push_back(line.substr(4));
            continue;
        }
        if (line.rfind("key=", 0) == 0) {
            projection.exposed_keys.push_back(line.substr(4));
            continue;
        }
        if (line.rfind("protector=", 0) == 0) {
            projection.protector = line.substr(10);
            continue;
        }
        if (line.rfind("payload=", 0) == 0) {
            payload_hex = line.substr(8);
            continue;
        }
    }

    if (projection.name.empty()) {
        error = "secret record is missing a name";
        return false;
    }

    if (payload_hex.empty()) {
        error = "secret record is missing a payload";
        return false;
    }

    return true;
}

std::string make_record_text(const SecretDraft& draft, const std::string& protector_name, const std::string& payload_hex) {
    std::ostringstream out;
    out << "name=" << draft.name << "\n";
    for (const std::string& tag : draft.tags) {
        out << "tag=" << tag << "\n";
    }
    for (const auto& value : draft.values) {
        out << "key=" << value.first << "\n";
    }
    out << "protector=" << protector_name << "\n";
    out << "payload=" << payload_hex << "\n";
    return out.str();
}

bool write_atomic(const std::filesystem::path& path, const std::string& text, std::string& error) {
    const std::filesystem::path temp = path.parent_path() / (path.filename().string() + ".tmp");
    {
        std::ofstream file(temp, std::ios::binary | std::ios::trunc);
        if (!file) {
            error = "unable to open temp secret record";
            return false;
        }
        file << text;
        if (!file.good()) {
            error = "unable to flush temp secret record";
            return false;
        }
    }

    std::error_code ec;
    std::filesystem::rename(temp, path, ec);
    if (ec) {
        std::filesystem::remove(path, ec);
        ec.clear();
        std::filesystem::rename(temp, path, ec);
        if (ec) {
            error = "unable to commit secret record";
            return false;
        }
    }

    return true;
}

bool load_record(const std::filesystem::path& path, SecretProjection& projection, std::string& payload_hex, std::string& error) {
    std::string text;
    if (!read_text_file(path, text, error)) {
        return false;
    }
    return parse_secret_record(text, projection, payload_hex, error);
}

bool validate_secret_draft(const SecretDraft& draft, std::string& error) {
    if (!is_secret_name(draft.name)) {
        error = "invalid secret name";
        return false;
    }

    if (draft.values.empty()) {
        error = "secret has no values";
        return false;
    }

    std::set<std::string> seen;
    for (const std::string& tag : draft.tags) {
        if (!is_secret_name(tag)) {
            error = "invalid tag";
            return false;
        }
    }

    for (const auto& item : draft.values) {
        if (!is_secret_key(item.first)) {
            error = "invalid secret key";
            return false;
        }
        if (item.second.empty()) {
            error = "secret value is empty";
            return false;
        }
        if (!seen.insert(item.first).second) {
            error = "duplicate secret key";
            return false;
        }
    }

    return true;
}

class DpapiSecretProtector final : public ISecretProtector {
public:
    std::string name() const override {
        return "dpapi";
    }

    bool supported() const override {
#if defined(_WIN32)
        return true;
#else
        return false;
#endif
    }

    bool protect(const std::string& plaintext, std::string& protected_text, std::string& error) const override {
#if defined(_WIN32)
        DATA_BLOB input {};
        input.cbData = static_cast<DWORD>(plaintext.size());
        input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plaintext.data()));

        DATA_BLOB output {};
        if (!CryptProtectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) {
            error = "dpapi protect failed";
            return false;
        }

        std::string raw(reinterpret_cast<const char*>(output.pbData), output.cbData);
        LocalFree(output.pbData);
        protected_text = encode_hex(raw);
        return true;
#else
        (void)plaintext;
        (void)protected_text;
        error = "dpapi unavailable";
        return false;
#endif
    }

    bool unprotect(const std::string& protected_text, std::string& plaintext, std::string& error) const override {
#if defined(_WIN32)
        std::string raw;
        if (!decode_hex(protected_text, raw)) {
            error = "invalid protected payload";
            return false;
        }

        DATA_BLOB input {};
        input.cbData = static_cast<DWORD>(raw.size());
        input.pbData = reinterpret_cast<BYTE*>(raw.data());

        DATA_BLOB output {};
        if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr, CRYPTPROTECT_UI_FORBIDDEN, &output)) {
            error = "dpapi unprotect failed";
            return false;
        }

        plaintext.assign(reinterpret_cast<const char*>(output.pbData), output.cbData);
        LocalFree(output.pbData);
        return true;
#else
        (void)protected_text;
        (void)plaintext;
        error = "dpapi unavailable";
        return false;
#endif
    }
};

}

std::string NullSecretProtector::name() const {
    return "null";
}

bool NullSecretProtector::supported() const {
    return false;
}

bool NullSecretProtector::protect(const std::string& plaintext, std::string& protected_text, std::string& error) const {
    (void)plaintext;
    (void)protected_text;
    error = "secret protection unavailable";
    return false;
}

bool NullSecretProtector::unprotect(const std::string& protected_text, std::string& plaintext, std::string& error) const {
    (void)protected_text;
    (void)plaintext;
    error = "secret protection unavailable";
    return false;
}

std::filesystem::path SecretVault::secrets_dir(const std::filesystem::path& root) {
    return root / ".ote" / "secrets";
}

std::filesystem::path SecretVault::records_dir(const std::filesystem::path& root) {
    return secrets_dir(root) / "records";
}

bool SecretVault::ensure_layout(const std::filesystem::path& root, std::string& error) {
    std::error_code ec;
    std::filesystem::create_directories(records_dir(root), ec);
    if (ec) {
        error = "unable to create secret vault";
        return false;
    }
    return true;
}

bool SecretVault::exists(const std::filesystem::path& root, const std::string& name) {
    const std::filesystem::path path = records_dir(root) / (safe_blob_name(name) + ".secret");
    return std::filesystem::exists(path);
}

bool SecretVault::list(const std::filesystem::path& root, std::vector<SecretProjection>& secrets, std::string& error) {
    secrets.clear();
    const std::filesystem::path dir = records_dir(root);
    std::error_code ec;
    if (!std::filesystem::exists(dir, ec)) {
        return true;
    }

    for (const auto& entry : std::filesystem::directory_iterator(dir, ec)) {
        if (ec) {
            error = "unable to enumerate secret vault";
            return false;
        }
        if (!entry.is_regular_file()) {
            continue;
        }
        if (entry.path().extension() != ".secret") {
            continue;
        }
        SecretProjection projection;
        std::string payload_hex;
        if (!load_record(entry.path(), projection, payload_hex, error)) {
            return false;
        }
        secrets.push_back(std::move(projection));
    }

    std::sort(secrets.begin(), secrets.end(), [](const SecretProjection& a, const SecretProjection& b) {
        return a.name < b.name;
    });

    return true;
}

bool SecretVault::describe(const std::filesystem::path& root, const std::string& name, SecretProjection& secret, std::string& error) {
    const std::filesystem::path path = records_dir(root) / (safe_blob_name(name) + ".secret");
    if (!std::filesystem::exists(path)) {
        error = "secret not found";
        return false;
    }
    std::string payload_hex;
    return load_record(path, secret, payload_hex, error);
}

bool SecretVault::store(const std::filesystem::path& root, const ISecretProtector& protector, const SecretDraft& draft, std::string& error) {
    if (!protector.supported()) {
        error = "secret protection unavailable";
        return false;
    }

    if (!validate_secret_draft(draft, error)) {
        return false;
    }

    if (!ensure_layout(root, error)) {
        return false;
    }

    std::ostringstream plaintext;
    for (const auto& item : draft.values) {
        plaintext << item.first << "=" << item.second << "\n";
    }

    std::string protected_text;
    if (!protector.protect(plaintext.str(), protected_text, error)) {
        return false;
    }

    const std::filesystem::path path = records_dir(root) / (safe_blob_name(draft.name) + ".secret");
    if (std::filesystem::exists(path)) {
        error = "secret already exists";
        return false;
    }

    const std::string record_text = make_record_text(draft, protector.name(), protected_text);
    return write_atomic(path, record_text, error);
}

bool is_secret_name(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    if (value.front() == '-' || value.back() == '-') {
        return false;
    }
    return std::all_of(value.begin(), value.end(), is_identifier_char);
}

bool is_secret_key(const std::string& value) {
    if (value.empty()) {
        return false;
    }
    if (!std::isalpha(static_cast<unsigned char>(value.front())) && value.front() != '_') {
        return false;
    }
    return std::all_of(value.begin(), value.end(), is_env_char);
}

std::string to_json_array(const std::vector<std::string>& values) {
    return join_json_array(values);
}

std::string to_json_object(const std::vector<std::pair<std::string, std::string>>& values) {
    return join_json_object(values);
}

const ISecretProtector& active_secret_protector() {
    static DpapiSecretProtector protector;
    if (protector.supported()) {
        return protector;
    }

    static NullSecretProtector fallback;
    return fallback;
}

}
