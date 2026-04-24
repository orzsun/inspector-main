#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <atomic>

class GameThreadHook {
public:
    GameThreadHook();
    ~GameThreadHook();

    bool Install(uintptr_t getContextCollectionAddr);
    void Uninstall();
    bool InstallByPattern(const std::string& pattern, const char* moduleName);

    // Install a tick hook by pattern and validate the function prologue.
    bool InstallTickHookByPattern(const std::string& pattern, const char* moduleName);

    // Validate the function prologue at address for expected patterns.
    bool ValidateFunctionPrologue(uintptr_t addr) const;
    // Detailed validator returning first mismatched byte offset (or -1 on read error)
    bool ValidateFunctionPrologueDetailed(uintptr_t addr, int& outMismatchOffset) const;

    bool IsTickInstalled() const { return m_tickInstalled.load(std::memory_order_acquire); }
    uintptr_t getTickTargetAddr() const { return m_tickTargetAddr.load(std::memory_order_acquire); }

    // Statistics: after installing tick hook, track ctx validity across ticks
    void ResetTickStats();
    void OnTickObserved(bool valid);

    // Request that the next invocation on the game thread perform a capture.
    void ScheduleCapture();

    bool IsInstalled() const { return m_installed.load(std::memory_order_acquire); }
    uintptr_t getContextCollectionAddr() const { return m_targetAddr.load(std::memory_order_acquire); }

private:
    using GetContextCollection_t = void*(*)();
    GetContextCollection_t m_orig = nullptr;
    std::atomic<uintptr_t> m_targetAddr{0};
    // tick hook target
    std::atomic<uintptr_t> m_tickTargetAddr{0};
    std::atomic<int> m_tickCount{0};
    std::atomic<int> m_validCount{0};
    std::atomic<bool> m_tickInstalled{false};
    std::atomic<bool> m_installed{false};

    static void* Detour_GetContextCollection();
    std::atomic<int> m_pendingCaptures{0};
};

extern GameThreadHook g_gameThreadHook;
