#include "log.hpp"
#include <cstdarg>

namespace lacz::log {

static std::mutex g_mutex;

static std::string log_path() {
    char appdata[MAX_PATH]{};
    if (!GetEnvironmentVariableA("APPDATA", appdata, MAX_PATH))
        return "lacz.log";
    return std::string(appdata) + "\\LaczDLC\\lacz.log";
}

void write(const char* fmt, ...) {
    char message[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    std::lock_guard lock(g_mutex);

    std::string path = log_path();
    // Ensure the directory exists.
    size_t slash = path.find_last_of('\\');
    if (slash != std::string::npos)
        CreateDirectoryA(path.substr(0, slash).c_str(), nullptr);

    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "a") == 0 && f) {
        SYSTEMTIME st{};
        GetLocalTime(&st);
        fprintf(f, "[%02d:%02d:%02d.%03d] %s\n",
                st.wHour, st.wMinute, st.wSecond, st.wMilliseconds, message);
        fclose(f);
    }
}

} // namespace lacz::log