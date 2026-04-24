#pragma once
#include <string>

namespace kx {
class Log {
public:
    static void Init(const std::string& path="kx_gth.log");
    static void Info(const char* fmt, ...);
    static void Error(const char* fmt, ...);
    static void Flush();
};
}
