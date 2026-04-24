#include "GameThreadHook.h"
#include "HookManager.h"
#include "CaptureQueue.h"
#include "SkillMemoryReader.h"
#include "PatternScanner.h"
#include <iostream>
#include <vector>
#include <sstream>
#include <cstdio>
#include <atomic>
#include "Log.h"
#include "AppState.h"

GameThreadHook g_gameThreadHook;

namespace {
    using GetContextCollection_t = void*(*)();
    // store pointer to original trampoline provided by MinHook
    static GetContextCollection_t s_orig = nullptr;
    static void(__fastcall* s_orig_tick)(void* rcx, void* rdx) = nullptr;
    static std::atomic<unsigned long long> s_captureSequence{0};
    static std::atomic<unsigned long long> s_startupDetailedLogsRemaining{3};

    bool TryReadBytes(uintptr_t addr, void* dst, size_t size) {
        __try {
            std::memcpy(dst, reinterpret_cast<const void*>(addr), size);
            return true;
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            std::memset(dst, 0, size);
            return false;
        }
    }
}

void* GameThreadHook::Detour_GetContextCollection() {
    const bool hasPendingCapture = g_gameThreadHook.m_pendingCaptures.load(std::memory_order_acquire) > 0;
    if (kx::g_globalPauseMode.load(std::memory_order_acquire) ||
        kx::g_capturePaused.load(std::memory_order_acquire) ||
        kx::g_isShuttingDown.load(std::memory_order_acquire)) {
        return s_orig ? s_orig() : nullptr;
    }

    const unsigned long long captureId = s_captureSequence.fetch_add(1, std::memory_order_acq_rel) + 1;
    bool enableDetailedLog = hasPendingCapture;
    if (!enableDetailedLog) {
        unsigned long long remaining = s_startupDetailedLogsRemaining.load(std::memory_order_acquire);
        while (remaining > 0) {
            if (s_startupDetailedLogsRemaining.compare_exchange_weak(remaining, remaining - 1, std::memory_order_acq_rel)) {
                enableDetailedLog = true;
                break;
            }
        }
    }

    void* res = nullptr;
    if (s_orig) {
        res = s_orig();
    }

    if (enableDetailedLog) {
        kx::Log::Info("[GTH][HOOK #%llu] GetContextCollection returned 0x%p",
                      captureId,
                      res);
    }

    // If we obtained a valid context pointer, perform a capture and push to queue
    if (res) {
        SkillbarSnapshot snap = g_skillReader.captureFromCtx(reinterpret_cast<uintptr_t>(res), enableDetailedLog);
        if (snap.valid) {
            // push to queue for consumers
            kx::CaptureQueue::Instance().Push(snap);

            if (enableDetailedLog) {
                kx::Log::Info("[GTH][HOOK #%llu] resolved ctx=0x%p chCtx=0x%p localChar=0x%p energies=0x%p skillbar=0x%p charSkillbar=0x%p",
                              captureId,
                              reinterpret_cast<void*>(snap.ctxAddr),
                              reinterpret_cast<void*>(snap.chCliContextAddr),
                              reinterpret_cast<void*>(snap.localCharAddr),
                              reinterpret_cast<void*>(snap.energiesAddr),
                              reinterpret_cast<void*>(snap.chCliSkillbarAddr),
                              reinterpret_cast<void*>(snap.charSkillbarAddr));
                kx::Log::Info("[GTH][HOOK #%llu] ptr40=0x%p ptr48=0x%p ptr60=0x%p",
                              captureId,
                              reinterpret_cast<void*>(snap.ptr40Addr),
                              reinterpret_cast<void*>(snap.ptr48Addr),
                              reinterpret_cast<void*>(snap.ptr60Addr));
                kx::Log::Info("[ENERGY][HOOK #%llu] current=%.2f max=%.2f delta=%.2f regen=%.2f",
                              captureId,
                              snap.currentEnergy,
                              snap.maximumEnergy,
                              snap.energyDelta,
                              snap.recoveryRate);
            }
        } else {
            kx::Log::Error("[GTH][HOOK #%llu] captureFromCtx failed for ctx=0x%p",
                           captureId,
                           res);
        }
        // satisfy any scheduled capture requests as well
        if (g_gameThreadHook.m_pendingCaptures.load(std::memory_order_acquire) > 0) {
            g_gameThreadHook.m_pendingCaptures.fetch_sub(1, std::memory_order_acq_rel);
        }
    } else {
        kx::Log::Error("[GTH][HOOK #%llu] fail: original GetContextCollection returned null", captureId);
    }

    return res;
}

GameThreadHook::GameThreadHook() {}
GameThreadHook::~GameThreadHook() { Uninstall(); }

bool GameThreadHook::Install(uintptr_t getContextCollectionAddr) {
    if (!getContextCollectionAddr) return false;
    if (m_installed.load(std::memory_order_acquire)) return true;

    m_targetAddr.store(getContextCollectionAddr, std::memory_order_release);
    kx::Log::Info("[GTH][INSTALL] installing GetContextCollection hook at 0x%p",
                  reinterpret_cast<void*>(m_targetAddr.load(std::memory_order_acquire)));
    // create hook via HookManager
    if (!kx::Hooking::HookManager::CreateHook(reinterpret_cast<LPVOID>(m_targetAddr.load(std::memory_order_acquire)), reinterpret_cast<LPVOID>(&Detour_GetContextCollection), reinterpret_cast<LPVOID*>(&s_orig))) {
        kx::Log::Error("[GTH][INSTALL] CreateHook failed for 0x%p", reinterpret_cast<void*>(m_targetAddr.load(std::memory_order_acquire)));
        std::cerr << "[GameThreadHook] CreateHook failed" << std::endl;
        return false;
    }
    if (!kx::Hooking::HookManager::EnableHook(reinterpret_cast<LPVOID>(m_targetAddr.load(std::memory_order_acquire)))) {
        kx::Log::Error("[GTH][INSTALL] EnableHook failed for 0x%p", reinterpret_cast<void*>(m_targetAddr.load(std::memory_order_acquire)));
        std::cerr << "[GameThreadHook] EnableHook failed" << std::endl;
        kx::Hooking::HookManager::RemoveHook(reinterpret_cast<LPVOID>(m_targetAddr.load(std::memory_order_acquire)));
        return false;
    }

    m_installed.store(true, std::memory_order_release);
    kx::Log::Info("[GTH][INSTALL] GetContextCollection hook installed at 0x%p",
                  reinterpret_cast<void*>(m_targetAddr.load(std::memory_order_acquire)));
    std::cout << "[GameThreadHook] Installed hook at "
              << std::hex << m_targetAddr.load(std::memory_order_acquire)
              << std::dec << std::endl;
    return true;
}

bool GameThreadHook::InstallByPattern(const std::string& pattern, const char* moduleName) {
    auto res = kx::PatternScanner::FindPattern(pattern, moduleName);
    if (!res) return false;
    return Install(res.value());
}

void GameThreadHook::ScheduleCapture() {
    m_pendingCaptures.fetch_add(1, std::memory_order_release);
}

// Simple prologue validator: checks first bytes against common x86_64 function prologues.
bool GameThreadHook::ValidateFunctionPrologue(uintptr_t addr) const {
    if (!addr) return false;
    uint8_t buf[8] = {};
    if (!TryReadBytes(addr, buf, sizeof(buf))) {
        return false;
    }

    // Known prologues to accept
    const std::vector<std::vector<uint8_t>> prologues = {
        // push rbp; mov rbp, rsp
        {0x55, 0x48, 0x89, 0xE5},
        // mov rcx, qword ptr [rsp+..] or mov [rsp+..]
        {0x48, 0x89, 0x5C, 0x24},
        // push rbx; mov rbx, rax (less common)
        {0x53, 0x48, 0x89, 0xD8},
        // sub rsp, imm
        {0x48, 0x83, 0xEC}
    };

    for (const auto &p : prologues) {
        bool ok = true;
        for (size_t i = 0; i < p.size(); ++i) {
            if (buf[i] != p[i]) { ok = false; break; }
        }
        if (ok) return true;
    }
    return false;
}

bool GameThreadHook::ValidateFunctionPrologueDetailed(uintptr_t addr, int& outMismatchOffset) const {
    outMismatchOffset = -1;
    if (!addr) return false;
    uint8_t buf[16] = {};
    if (!TryReadBytes(addr, buf, sizeof(buf))) {
        outMismatchOffset = -1;
        return false;
    }

    const std::vector<std::vector<uint8_t>> prologues = {
        {0x55, 0x48, 0x89, 0xE5},
        {0x48, 0x89, 0x5C, 0x24},
        {0x53, 0x48, 0x89, 0xD8},
        {0x48, 0x83, 0xEC}
    };

    for (const auto &p : prologues) {
        bool ok = true;
        for (size_t i = 0; i < p.size(); ++i) {
            if (buf[i] != p[i]) { ok = false; outMismatchOffset = static_cast<int>(i); break; }
        }
        if (ok) { outMismatchOffset = -2; return true; }
    }
    return false;
}

void GameThreadHook::ResetTickStats() {
    m_tickCount.store(0, std::memory_order_release);
    m_validCount.store(0, std::memory_order_release);
}

void GameThreadHook::OnTickObserved(bool valid) {
    int ticks = m_tickCount.fetch_add(1, std::memory_order_acq_rel) + 1;
    if (valid) m_validCount.fetch_add(1, std::memory_order_acq_rel);
    if (ticks == 10) {
        int validCount = m_validCount.load(std::memory_order_acquire);
        kx::Log::Info("[GTH] GameTickFn found @ 0x%p (validated) - 10 ticks: %d valid, %d invalid",
                      reinterpret_cast<void*>(m_tickTargetAddr.load(std::memory_order_acquire)),
                      validCount,
                      10 - validCount);
        ResetTickStats();
    }
}

// Detour for game tick function; minimal signature assumed: __fastcall(this, arg)
static void __fastcall Detour_GameTick(void* rcx, void* rdx) {
    // Call original first (if any)
    if (s_orig_tick) {
        s_orig_tick(rcx, rdx);
    }

    if (kx::g_globalPauseMode.load(std::memory_order_acquire) ||
        kx::g_capturePaused.load(std::memory_order_acquire) ||
        kx::g_isShuttingDown.load(std::memory_order_acquire)) {
        return;
    }

    // After original, attempt to call GetContextCollection and gather stats
    if (g_gameThreadHook.IsInstalled() && g_gameThreadHook.getContextCollectionAddr() != 0) {
        auto fn = reinterpret_cast<GetContextCollection_t>(g_gameThreadHook.getContextCollectionAddr());
        if (fn) {
            void* ctx = nullptr;
            __try {
                ctx = fn();
            } __except (EXCEPTION_EXECUTE_HANDLER) { ctx = nullptr; }

            bool valid = (ctx != nullptr);
            g_gameThreadHook.OnTickObserved(valid);
        }
    }
}

bool GameThreadHook::InstallTickHookByPattern(const std::string& pattern, const char* moduleName) {
    if (m_tickInstalled.load(std::memory_order_acquire)) {
        return true;
    }
    auto res = kx::PatternScanner::FindPattern(pattern, moduleName);
    if (!res) return false;
    uintptr_t addr = res.value();

    if (!ValidateFunctionPrologue(addr)) return false;

    // Install hook
    m_tickTargetAddr.store(addr, std::memory_order_release);
    if (!kx::Hooking::HookManager::CreateHook(reinterpret_cast<LPVOID>(m_tickTargetAddr.load(std::memory_order_acquire)), reinterpret_cast<LPVOID>(&Detour_GameTick), reinterpret_cast<LPVOID*>(&s_orig_tick))) {
        return false;
    }
    if (!kx::Hooking::HookManager::EnableHook(reinterpret_cast<LPVOID>(m_tickTargetAddr.load(std::memory_order_acquire)))) {
        kx::Hooking::HookManager::RemoveHook(reinterpret_cast<LPVOID>(m_tickTargetAddr.load(std::memory_order_acquire)));
        return false;
    }

    // Log validated
    kx::Log::Info("[GTH] GameTickFn found @ 0x%p (validated)", reinterpret_cast<void*>(m_tickTargetAddr.load(std::memory_order_acquire)));

    // initialize counters
    m_tickInstalled.store(true, std::memory_order_release);
    ResetTickStats();
    return true;
}

void GameThreadHook::Uninstall() {
    const uintptr_t tickTargetAddr = m_tickTargetAddr.load(std::memory_order_acquire);
    const uintptr_t targetAddr = m_targetAddr.load(std::memory_order_acquire);
    if (m_tickInstalled.load(std::memory_order_acquire) && tickTargetAddr != 0) {
        kx::Hooking::HookManager::DisableHook(reinterpret_cast<LPVOID>(tickTargetAddr));
        kx::Hooking::HookManager::RemoveHook(reinterpret_cast<LPVOID>(tickTargetAddr));
    }
    if (m_installed.load(std::memory_order_acquire) && targetAddr != 0) {
        kx::Hooking::HookManager::DisableHook(reinterpret_cast<LPVOID>(targetAddr));
        kx::Hooking::HookManager::RemoveHook(reinterpret_cast<LPVOID>(targetAddr));
    }
    m_tickInstalled.store(false, std::memory_order_release);
    m_installed.store(false, std::memory_order_release);
    s_orig_tick = nullptr;
    s_orig = nullptr;
    m_tickTargetAddr.store(0, std::memory_order_release);
    m_targetAddr.store(0, std::memory_order_release);
    m_pendingCaptures.store(0, std::memory_order_release);
    std::cout << "[GameThreadHook] Uninstalled" << std::endl;
}
