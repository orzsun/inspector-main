#include "Log.h"
#include <mutex>
#include <fstream>
#include <cstdarg>
#include <cstdio>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace kx {
static std::mutex g_logMtx;
static std::ofstream g_logFile;

void Log::Init(const std::string& path) {
    std::lock_guard<std::mutex> lk(g_logMtx);
    if (g_logFile.is_open()) return;
    g_logFile.open(path, std::ios::out | std::ios::app);
    if (!g_logFile.is_open()) {
        return;
    }
}

static void LogWrite(const char* level, const char* fmt, va_list ap) {
    char buf[1024];
    vsnprintf(buf, sizeof(buf), fmt, ap);
    std::ostringstream oss;
    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);
    std::tm localTm = {};
    localtime_s(&localTm, &t);
    oss << "[" << level << "] " << std::put_time(&localTm, "%F %T") << " " << buf << "\n";

    std::lock_guard<std::mutex> lk(g_logMtx);
    if (g_logFile.is_open()) {
        g_logFile << oss.str();
        g_logFile.flush();
    }
}

void Log::Info(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    LogWrite("INFO", fmt, ap);
    va_end(ap);
}

void Log::Error(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    LogWrite("ERROR", fmt, ap);
    va_end(ap);
}

void Log::Flush() {
    std::lock_guard<std::mutex> lk(g_logMtx);
    if (g_logFile.is_open()) g_logFile.flush();
}

} // namespace kx
