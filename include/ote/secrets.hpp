#pragma once

#include <filesystem>
#include <string>
#include <utility>
#include <vector>

namespace ote {

struct SecretDraft {
    std::string name;
    std::vector<std::string> tags;
    std::vector<std::pair<std::string, std::string>> values;
};

struct SecretProjection {
    std::string name;
    std::vector<std::string> tags;
    std::vector<std::string> exposed_keys;
    std::string protector;
};

class ISecretProtector {
public:
    virtual ~ISecretProtector() = default;
    virtual std::string name() const = 0;
    virtual bool supported() const = 0;
    virtual bool protect(const std::string& plaintext, std::string& protected_text, std::string& error) const = 0;
    virtual bool unprotect(const std::string& protected_text, std::string& plaintext, std::string& error) const = 0;
};

class NullSecretProtector final : public ISecretProtector {
public:
    std::string name() const override;
    bool supported() const override;
    bool protect(const std::string& plaintext, std::string& protected_text, std::string& error) const override;
    bool unprotect(const std::string& protected_text, std::string& plaintext, std::string& error) const override;
};

class SecretVault {
public:
    static std::filesystem::path secrets_dir(const std::filesystem::path& root);
    static std::filesystem::path records_dir(const std::filesystem::path& root);

    static bool ensure_layout(const std::filesystem::path& root, std::string& error);
    static bool list(const std::filesystem::path& root, std::vector<SecretProjection>& secrets, std::string& error);
    static bool describe(const std::filesystem::path& root, const std::string& name, SecretProjection& secret, std::string& error);
    static bool load_values(const std::filesystem::path& root, const std::string& name, std::vector<std::pair<std::string, std::string>>& values, std::string& error);
    static bool store(const std::filesystem::path& root, const ISecretProtector& protector, const SecretDraft& draft, std::string& error);
    static bool exists(const std::filesystem::path& root, const std::string& name);
};

bool is_secret_name(const std::string& value);
bool is_secret_key(const std::string& value);
std::string to_json_array(const std::vector<std::string>& values);
std::string to_json_object(const std::vector<std::pair<std::string, std::string>>& values);
const ISecretProtector& active_secret_protector();

}
