#include "ote/terminal.hpp"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#if defined(_WIN32)
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#endif

namespace ote {
namespace {

void enable_windows_utf8() {
#if defined(_WIN32)
    static bool attempted = false;
    if (attempted) {
        return;
    }
    attempted = true;

    const HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle != INVALID_HANDLE_VALUE && handle != nullptr) {
        DWORD mode = 0;
        if (GetConsoleMode(handle, &mode) != 0) {
            mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
            SetConsoleMode(handle, mode);
        }
    }

    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    if (_isatty(_fileno(stdout)) != 0) {
        _setmode(_fileno(stdout), _O_BINARY);
    }
#endif
}

bool enable_windows_ansi() {
#if defined(_WIN32)
    static bool attempted = false;
    static bool enabled = false;
    if (attempted) {
        return enabled;
    }
    attempted = true;

    const HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle == INVALID_HANDLE_VALUE || handle == nullptr) {
        return false;
    }

    DWORD mode = 0;
    if (!GetConsoleMode(handle, &mode)) {
        return false;
    }

    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    if (!SetConsoleMode(handle, mode)) {
        return false;
    }

    enabled = true;
    return true;
#else
    return false;
#endif
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) {
        return {};
    }
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

}

void initialize_terminal() {
    enable_windows_utf8();
}

bool terminal_supports_color() {
    initialize_terminal();

    static int cached = -1;
    if (cached != -1) {
        return cached == 1;
    }

    if (std::getenv("NO_COLOR") != nullptr) {
        cached = 0;
        return false;
    }

#if defined(_WIN32)
    if (!enable_windows_ansi()) {
        cached = 0;
        return false;
    }
#endif

    cached = 1;
    return true;
}

std::string colorize(const std::string& text, const char* code) {
    if (!terminal_supports_color()) {
        return text;
    }
    return std::string("\x1b[") + code + "m" + text + "\x1b[0m";
}

std::string ascii_banner(const std::filesystem::path& root) {
    initialize_terminal();

    const std::string banner = read_file(root / "ascii.txt");
    if (!banner.empty()) {
        return banner;
    }

    const std::string art =
        " ██████╗ ████████╗███████╗\n"
        "██╔═══██╗╚══██╔══╝██╔════╝\n"
        "██║   ██║   ██║   █████╗  \n"
        "██║   ██║   ██║   ██╔══╝  \n"
        "╚██████╔╝   ██║   ███████╗\n"
        " ╚═════╝    ╚═╝   ╚══════╝\n"
        "                          \n";

    return colorize(art, "36;1") + colorize("Kapsel\n", "37;1");
}
}
