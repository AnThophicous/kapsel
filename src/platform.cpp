#include "ote/platform.hpp"

#include <cstdlib>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <unistd.h>
#else
#include <unistd.h>
#endif

namespace ote {

std::string platform_name() {
#if defined(_WIN32)
    return "windows";
#elif defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#else
    return "unknown";
#endif
}

std::string architecture_name() {
#if defined(_M_ARM64) || defined(__aarch64__)
    return "arm64";
#elif defined(_M_ARM) || defined(__arm__)
    return "arm32";
#elif defined(_M_X64) || defined(__x86_64__)
    return "x64";
#elif defined(_M_IX86) || defined(__i386__)
    return "x86";
#else
    return "unknown";
#endif
}

std::string default_shell() {
#if defined(_WIN32)
    return "cmd";
#else
    return "bash";
#endif
}

std::filesystem::path executable_path() {
#if defined(_WIN32)
    std::vector<wchar_t> buffer(MAX_PATH);
    DWORD size = 0;
    for (;;) {
        size = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (size == 0) {
            return {};
        }
        if (size < buffer.size() - 1) {
            break;
        }
        buffer.resize(buffer.size() * 2);
    }
    return std::filesystem::path(std::wstring(buffer.data(), size));
#elif defined(__APPLE__)
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    std::vector<char> buffer(size + 1);
    if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
        return {};
    }
    buffer[size] = '\0';
    return std::filesystem::weakly_canonical(std::filesystem::path(buffer.data()));
#else
    std::vector<char> buffer(4096);
    ssize_t size = ::readlink("/proc/self/exe", buffer.data(), buffer.size() - 1);
    if (size <= 0) {
        return {};
    }
    buffer[static_cast<std::size_t>(size)] = '\0';
    return std::filesystem::weakly_canonical(std::filesystem::path(buffer.data()));
#endif
}

std::filesystem::path user_home_path() {
#if defined(_WIN32)
    if (const char* userprofile = std::getenv("USERPROFILE")) {
        return std::filesystem::path(userprofile);
    }
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home);
    }
    return {};
#else
    if (const char* home = std::getenv("HOME")) {
        return std::filesystem::path(home);
    }
    return {};
#endif
}

}
