#include "SkillMemoryReader.h"
#include "AppState.h"
#include "PatternScanner.h"
#include "CaptureQueue.h"
#include "GameThreadHook.h"
#include "Log.h"
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>
#include <cstring>
#include <string>
#include <vector>
#include <algorithm>
#include <limits>

SkillMemoryReader g_skillReader;

namespace {
constexpr const char* kGthLogFilePath = "kx_gth_install.log";
constexpr const char* kVerifiedGetContextCollectionPattern =
    "8B ? ? ? ? ? 65 ? ? ? ? ? ? ? ? BA ? ? ? ? 48 ? ? ? 48 ? ? ? C3";
constexpr unsigned long long kUiTlsRefreshIntervalMs = 250;
constexpr size_t kCharSkillbarPrimarySize = 0x188;
constexpr size_t kCharSkillbarWideSize = 0x400;
constexpr size_t kChCliSkillbarSize = 0x240;
constexpr uint32_t kIsRechargingVtableIndex = 7;
constexpr int kMaxProbeEntries = 21;

template <typename T>
bool TryReadValue(uintptr_t address, T& outValue) {
    __try {
        outValue = *reinterpret_cast<const T*>(address);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        std::memset(&outValue, 0, sizeof(outValue));
        return false;
    }
}

bool TryCopyBytes(void* dst, uintptr_t src, size_t size) {
    __try {
        std::memcpy(dst, reinterpret_cast<const void*>(src), size);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        std::memset(dst, 0, size);
        return false;
    }
}

uint32_t ReadLe32(const uint8_t* data, size_t offset) {
    return static_cast<uint32_t>(data[offset]) |
           (static_cast<uint32_t>(data[offset + 1]) << 8) |
           (static_cast<uint32_t>(data[offset + 2]) << 16) |
           (static_cast<uint32_t>(data[offset + 3]) << 24);
}

uint64_t ReadLe64(const uint8_t* data, size_t offset) {
    uint64_t value = 0;
    for (size_t i = 0; i < 8; ++i) {
        value |= (static_cast<uint64_t>(data[offset + i]) << (i * 8));
    }
    return value;
}

std::string BasenameFromPath(const char* path) {
    if (!path || !*path) {
        return {};
    }
    std::string value(path);
    const size_t slash = value.find_last_of("\\/");
    return slash == std::string::npos ? value : value.substr(slash + 1);
}

uintptr_t GetSkillbarObjectPointer(const SkillbarSnapshot& snap, SkillbarObjectKind kind) {
    switch (kind) {
    case SkillbarObjectKind::ChCliSkillbar:
        return snap.chCliSkillbarAddr;
    case SkillbarObjectKind::CharSkillbar:
        return snap.charSkillbarAddr;
    default:
        return 0;
    }
}

SkillRechargeTestResult ExecuteRechargeTestImpl(void* skillbarPtr,
                                                uint32_t slot,
                                                SkillbarObjectKind objectKind,
                                                bool onGameThread) {
    using IsRechargingFn = bool(__fastcall*)(void* skillbar, uint32_t slotIndex);

    SkillRechargeTestResult result = {};
    result.tickMs = GetTickCount64();
    result.objectPtr = reinterpret_cast<uintptr_t>(skillbarPtr);
    result.slot = slot;
    result.vtableIndex = kIsRechargingVtableIndex;
    result.objectKind = objectKind;
    result.onGameThread = onGameThread;

    if (!skillbarPtr) {
        return result;
    }

    uintptr_t* vtable = nullptr;
    if (!TryReadValue(reinterpret_cast<uintptr_t>(skillbarPtr), vtable) || !vtable) {
        return result;
    }
    result.vtablePtr = reinterpret_cast<uintptr_t>(vtable);

    uintptr_t functionPtr = 0;
    if (!TryReadValue(reinterpret_cast<uintptr_t>(&vtable[kIsRechargingVtableIndex]), functionPtr) || !functionPtr) {
        return result;
    }
    result.functionPtr = functionPtr;

    const auto fn = reinterpret_cast<IsRechargingFn>(functionPtr);
    __try {
        result.returnedValue = fn(skillbarPtr, slot);
        result.executed = true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        result.callFaulted = true;
    }

    return result;
}

SkillRechargeSweepResult ExecuteRechargeSweepImpl(void* skillbarPtr,
                                                  SkillbarObjectKind objectKind,
                                                  uint32_t slotCount,
                                                  bool onGameThread) {
    SkillRechargeSweepResult sweep = {};
    sweep.tickMs = GetTickCount64();
    sweep.objectKind = objectKind;
    sweep.onGameThread = onGameThread;
    sweep.slotCount = (std::min)(slotCount, static_cast<uint32_t>(sweep.slots.size()));

    for (uint32_t slot = 0; slot < sweep.slotCount; ++slot) {
        sweep.slots[slot] = ExecuteRechargeTestImpl(skillbarPtr, slot, objectKind, onGameThread);
    }
    return sweep;
}

VtableProbeBatchResult ExecuteVtableProbeImpl(void* objPtr,
                                              SkillbarObjectKind objectKind,
                                              uint32_t slot,
                                              int startIdx,
                                              int endIdx,
                                              bool onGameThread) {
    using FnBoolU32 = bool(__fastcall*)(void*, uint32_t);

    VtableProbeBatchResult batch = {};
    batch.tickMs = GetTickCount64();
    batch.objectPtr = reinterpret_cast<uintptr_t>(objPtr);
    batch.slot = slot;
    batch.startIndex = startIdx;
    batch.endIndex = endIdx;
    batch.objectKind = objectKind;
    batch.onGameThread = onGameThread;

    if (!objPtr || startIdx > endIdx) {
        return batch;
    }

    uintptr_t* vtable = nullptr;
    if (!TryReadValue(reinterpret_cast<uintptr_t>(objPtr), vtable) || !vtable) {
        return batch;
    }
    batch.vtablePtr = reinterpret_cast<uintptr_t>(vtable);

    for (int i = startIdx; i <= endIdx && batch.entryCount < batch.entries.size(); ++i) {
        VtableProbeResult entry = {};
        entry.index = i;
        if (!TryReadValue(reinterpret_cast<uintptr_t>(&vtable[i]), entry.fnAddr) || !entry.fnAddr) {
            batch.entries[batch.entryCount++] = entry;
            continue;
        }

        __try {
            const auto fn = reinterpret_cast<FnBoolU32>(entry.fnAddr);
            entry.returned = fn(objPtr, slot);
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            entry.faulted = true;
        }

        batch.entries[batch.entryCount++] = entry;
    }

    return batch;
}
} // namespace

bool SkillMemoryReader::init() {
    kx::Log::Init(kGthLogFilePath);
    kx::Log::Info("[GTH][INIT] SkillMemoryReader::init begin");
    const SkillbarPointerOffsets offsets = m_skillbarOffsets;
    kx::Log::Info("[GTH][INIT] SKWATCH offsets charSkillbar=0x%zX skillbarSkillsArray=0x%zX skillbarPressedSkill=0x%zX vtSkillDown=0x%zX vtSkillUp=0x%zX legacyCharSkillbarFromSkillbar=0x%zX",
                  static_cast<size_t>(offsets.charSkillbar),
                  static_cast<size_t>(offsets.skillbarSkillsArray),
                  static_cast<size_t>(offsets.skillbarPressedSkill),
                  static_cast<size_t>(offsets.skillbarVtSkillDown),
                  static_cast<size_t>(offsets.skillbarVtSkillUp),
                  static_cast<size_t>(offsets.legacyCharSkillbarFromSkillbar));

    m_fnGetCtx = nullptr;
    m_cachedCtxAddr = 0;
    m_isInitialized = false;

    HMODULE hMod = GetModuleHandleA("Gw2-64.exe");
    std::vector<std::string> moduleCandidates;
    if (hMod) {
        moduleCandidates.emplace_back("Gw2-64.exe");
    }
    hMod = GetModuleHandleA("Gw2.exe");
    if (hMod) {
        moduleCandidates.emplace_back("Gw2.exe");
    }

    HMODULE currentModule = GetModuleHandleA(nullptr);
    if (currentModule) {
        char modulePath[MAX_PATH] = {};
        if (GetModuleFileNameA(currentModule, modulePath, MAX_PATH)) {
            std::string basename = BasenameFromPath(modulePath);
            if (!basename.empty() &&
                std::find(moduleCandidates.begin(), moduleCandidates.end(), basename) == moduleCandidates.end()) {
                moduleCandidates.push_back(basename);
            }
        }
    }

    if (moduleCandidates.empty()) {
        kx::Log::Error("[GTH][INIT] fail: unable to resolve any module candidate for GetContextCollection scan");
        return false;
    }

    std::optional<uintptr_t> res;
    std::string resolvedModuleName;
    for (const std::string& candidate : moduleCandidates) {
        kx::Log::Info("[GTH][INIT] scanning module=%s pattern=%s", candidate.c_str(), kVerifiedGetContextCollectionPattern);
        res = kx::PatternScanner::FindPattern(kVerifiedGetContextCollectionPattern, candidate);
        if (res) {
            resolvedModuleName = candidate;
            break;
        }
    }

    if (!res) {
        m_fnGetCtx = nullptr;
        kx::Log::Error("[GTH][INIT] fail: GetContextCollection pattern not found in any candidate module");
        return false;
    }

    m_fnGetCtx = reinterpret_cast<GetContextCollection_t>(res.value());
    kx::Log::Info("[GTH][INIT] GetContextCollection @ 0x%p module=%s",
                  reinterpret_cast<void*>(res.value()),
                  resolvedModuleName.c_str());

    // Install a hook on GetContextCollection so we can capture on the game logic thread
    extern GameThreadHook g_gameThreadHook;
    const bool getCtxHookInstalled = g_gameThreadHook.Install(reinterpret_cast<uintptr_t>(m_fnGetCtx));
    kx::Log::Info("[GTH][INIT] GetContextCollection hook install result=%s",
                  getCtxHookInstalled ? "success" : "fail");

    // Attempt to install tick hook automatically using recommended default pattern.
    // Default tick pattern (from kx-vision docs). If your kx-vision doc specifies another pattern,
    // replace this string accordingly.
    const std::string defaultTickPattern = "48 89 5C 24 ? 57 48 83 EC ? 48 8B ?? ?? ?? ?? 48 8B ?? ?? ??";
    bool tickPatternHit = false;
    uintptr_t tickAddr = 0;
    std::optional<uintptr_t> tickRes = kx::PatternScanner::FindPattern(defaultTickPattern, resolvedModuleName);
    if (tickRes) {
        tickPatternHit = true;
        tickAddr = tickRes.value();
    }
    if (!tickPatternHit) {
        kx::Log::Error("[GTH] pattern hit: 0x0");
        kx::Log::Error("[GTH] prologue ok: no");
        kx::Log::Error("[GTH] install result: fail");
        kx::Log::Error("[GTH] reason: pattern not found");
    } else {
        kx::Log::Info("[GTH] pattern hit: 0x%p", reinterpret_cast<void*>(tickAddr));

        int mismatch = -1;
        bool protoOk = g_gameThreadHook.ValidateFunctionPrologueDetailed(tickAddr, mismatch);
        kx::Log::Info("[GTH] prologue ok: %s", protoOk ? "yes" : "no");

        if (!protoOk) {
            kx::Log::Error("[GTH] reason: prologue mismatch at offset %d", mismatch);
        } else {
            bool installed = g_gameThreadHook.InstallTickHookByPattern(defaultTickPattern, resolvedModuleName.c_str());
            kx::Log::Info("[GTH] install result: %s", installed ? "success" : "fail");
            if (!installed) {
                kx::Log::Error("[GTH] reason: hook manager failed to create/enable hook");
            }
        }
    }
    kx::Log::Info("[GTH][INIT] SkillMemoryReader::init done m_fnGetCtx=%s", m_fnGetCtx ? "set" : "null");
    m_isInitialized = (m_fnGetCtx != nullptr);
    return m_fnGetCtx != nullptr;
}

uintptr_t SkillMemoryReader::findContextCollection() {
    uintptr_t chk = 0;
    if (m_cachedCtxAddr && TryReadValue(m_cachedCtxAddr + 0x98, chk) && chk) {
        return m_cachedCtxAddr;
    }

    m_cachedCtxAddr = 0;

    using NtQIT_t = LONG (NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    static auto ntQueryInformationThread =
        reinterpret_cast<NtQIT_t>(GetProcAddress(GetModuleHandleA("ntdll.dll"), "NtQueryInformationThread"));
    if (!ntQueryInformationThread) return 0;

    const DWORD pid = GetCurrentProcessId();
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return 0;

    THREADENTRY32 te = {};
    te.dwSize = sizeof(te);
    if (!Thread32First(hSnap, &te)) {
        CloseHandle(hSnap);
        return 0;
    }

    struct ClientId {
        HANDLE UniqueProcess;
        HANDLE UniqueThread;
    };

    struct ThreadBasicInformation {
        LONG ExitStatus;
        PVOID TebBaseAddress;
        ClientId ClientId;
        ULONG_PTR AffinityMask;
        LONG Priority;
        LONG BasePriority;
    };

    uintptr_t result = 0;
    do {
        if (te.th32OwnerProcessID != pid) continue;

        HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION | THREAD_QUERY_LIMITED_INFORMATION, FALSE, te.th32ThreadID);
        if (!hThread) continue;

        ThreadBasicInformation tbi = {};
        LONG status = ntQueryInformationThread(hThread, 0, &tbi, sizeof(tbi), nullptr);
        CloseHandle(hThread);
        if (status < 0 || !tbi.TebBaseAddress) continue;

        uintptr_t tlsArrayPtr = 0;
        if (!TryReadValue(reinterpret_cast<uintptr_t>(tbi.TebBaseAddress) + 0x58, tlsArrayPtr) || !tlsArrayPtr) continue;

        uintptr_t tlsSlot0 = 0;
        if (!TryReadValue(tlsArrayPtr, tlsSlot0) || !tlsSlot0) continue;

        uintptr_t candidate = 0;
        if (!TryReadValue(tlsSlot0 + 0x8, candidate) || !candidate) continue;

        uintptr_t chCliCtx = 0;
        if (!TryReadValue(candidate + 0x98, chCliCtx) || !chCliCtx) continue;

        result = candidate;
        break;
    } while (Thread32Next(hSnap, &te));

    CloseHandle(hSnap);
    m_cachedCtxAddr = result;
    return result;
}

uintptr_t SkillMemoryReader::refreshContextCollection() {
    return findContextCollection();
}

SkillbarSnapshot SkillMemoryReader::read() {
    SkillbarSnapshot snap = {};

    // Prefer captures produced on the game logic thread
    if (kx::CaptureQueue::Instance().TryPop(snap)) {
        storeSnapshot(snap);
        return snap;
    }

    // If no capture available, request one on the game thread and wait briefly
    extern GameThreadHook g_gameThreadHook;
    g_gameThreadHook.ScheduleCapture();
    const int waitMs = 50;
    const int stepMs = 2;
    int waited = 0;
    while (waited < waitMs) {
        if (kx::CaptureQueue::Instance().TryPop(snap)) {
            storeSnapshot(snap);
            return snap;
        }
        Sleep(stepMs);
        waited += stepMs;
    }

    SkillbarSnapshot cached = getCachedSnapshot();
    if (cached.valid) {
        return cached;
    }

    // Fallback: try to find the context collection via thread TLS scan
    const uintptr_t ctx = findContextCollection();
    if (!ctx) return snap;
    if (resolveSnapshotFromContext(ctx, snap, false, "TLS")) {
        storeSnapshot(snap);
    }
    return snap;
}

SkillbarSnapshot SkillMemoryReader::readNoWait() {
    SkillbarSnapshot snap = {};

    // For UI/render-thread callers, never block waiting on a future game-thread capture.
    if (kx::CaptureQueue::Instance().TryPop(snap)) {
        storeSnapshot(snap);
        return snap;
    }

    SkillbarSnapshot cached = getCachedSnapshot();
    if (cached.valid) {
        return cached;
    }

    if (!m_isInitialized) {
        return snap;
    }

    const unsigned long long now = GetTickCount64();
    if (now - m_lastUiRefreshTickMs < kUiTlsRefreshIntervalMs) {
        return snap;
    }
    m_lastUiRefreshTickMs = now;

    const uintptr_t ctx = findContextCollection();
    if (!ctx) {
        return snap;
    }

    if (resolveSnapshotFromContext(ctx, snap, false, "TLS")) {
        storeSnapshot(snap);
    }
    return snap;
}

SkillbarSnapshot SkillMemoryReader::captureFromCtx(uintptr_t ctxAddr, bool enableLog) {
    SkillbarSnapshot snap = {};
    if (resolveSnapshotFromContext(ctxAddr, snap, enableLog, "GTH")) {
        processPendingRechargeTestFromSnapshot(snap);
        processPendingVtableProbeFromSnapshot(snap);
        storeSnapshot(snap);
    }
    return snap;
}

void SkillMemoryReader::setSkillbarPointerOffsets(const SkillbarPointerOffsets& offsets) {
    m_skillbarOffsets = offsets;
}

SkillbarPointerOffsets SkillMemoryReader::getSkillbarPointerOffsets() const {
    return m_skillbarOffsets;
}

void SkillMemoryReader::storeSnapshot(const SkillbarSnapshot& snap) {
    if (!snap.valid) {
        return;
    }
    std::lock_guard<std::mutex> lock(m_snapshotMutex);
    m_lastSnapshot = snap;
}

SkillbarSnapshot SkillMemoryReader::getCachedSnapshot() {
    std::lock_guard<std::mutex> lock(m_snapshotMutex);
    return m_lastSnapshot;
}

const char* SkillMemoryReader::GetSkillbarObjectKindName(SkillbarObjectKind kind) {
    switch (kind) {
    case SkillbarObjectKind::ChCliSkillbar:
        return "ChCliSkillbar";
    case SkillbarObjectKind::CharSkillbar:
        return "CharSkillbar";
    default:
        return "Unknown";
    }
}

bool SkillMemoryReader::TestIsRecharging(void* skillbarPtr, uint32_t slot) {
    return ExecuteRechargeTestImpl(skillbarPtr, slot, SkillbarObjectKind::ChCliSkillbar, false).returnedValue;
}

SkillRechargeTestResult SkillMemoryReader::testIsRechargingFromSnapshot(const SkillbarSnapshot& snap,
                                                                        SkillbarObjectKind objectKind,
                                                                        uint32_t slot,
                                                                        bool onGameThread) {
    return ExecuteRechargeTestImpl(reinterpret_cast<void*>(GetSkillbarObjectPointer(snap, objectKind)),
                                   slot,
                                   objectKind,
                                   onGameThread);
}

SkillRechargeTestResult SkillMemoryReader::testIsRechargingCurrentThread(SkillbarObjectKind objectKind, uint32_t slot) {
    const SkillbarSnapshot snap = readNoWait();
    return testIsRechargingFromSnapshot(snap, objectKind, slot, false);
}

SkillRechargeTestResult SkillMemoryReader::requestIsRechargingOnGameThread(SkillbarObjectKind objectKind,
                                                                           uint32_t slot,
                                                                           unsigned long long timeoutMs) {
    if (kx::g_isShuttingDown.load(std::memory_order_acquire)) {
        SkillRechargeTestResult aborted = {};
        aborted.tickMs = GetTickCount64();
        aborted.objectKind = objectKind;
        aborted.slot = slot;
        aborted.onGameThread = true;
        return aborted;
    }

    unsigned long long requestId = 0;
    {
        std::lock_guard<std::mutex> lock(m_rechargeTestMutex);
        requestId = m_nextRechargeRequestId++;
        m_hasPendingRechargeTest = true;
        m_pendingRechargeRequestId = requestId;
        m_pendingRechargeObjectKind = objectKind;
        m_pendingRechargeSlot = slot;
    }

    kx::Log::Info("[RECHARGE_REQ][single] queued request_id=%llu object=%s slot=%u timeout_ms=%llu",
                  requestId,
                  GetSkillbarObjectKindName(objectKind),
                  slot,
                  timeoutMs);

    extern GameThreadHook g_gameThreadHook;
    g_gameThreadHook.ScheduleCapture();

    const unsigned long long start = GetTickCount64();
    while (GetTickCount64() - start <= timeoutMs) {
        if (kx::g_isShuttingDown.load(std::memory_order_acquire)) {
            std::lock_guard<std::mutex> lock(m_rechargeTestMutex);
            if (m_hasPendingRechargeTest && m_pendingRechargeRequestId == requestId) {
                m_hasPendingRechargeTest = false;
            }
            break;
        }
        {
            std::lock_guard<std::mutex> lock(m_rechargeTestMutex);
            if (m_lastGameThreadRechargeTest.requestId == requestId) {
                kx::Log::Info("[RECHARGE_REQ][single] completed request_id=%llu object=%s slot=%u executed=%s returned=%s faulted=%s",
                              requestId,
                              GetSkillbarObjectKindName(m_lastGameThreadRechargeTest.objectKind),
                              m_lastGameThreadRechargeTest.slot,
                              m_lastGameThreadRechargeTest.executed ? "yes" : "no",
                              m_lastGameThreadRechargeTest.returnedValue ? "true" : "false",
                              m_lastGameThreadRechargeTest.callFaulted ? "yes" : "no");
                return m_lastGameThreadRechargeTest;
            }
        }
        Sleep(2);
    }

    SkillRechargeTestResult timeoutResult = {};
    timeoutResult.requestId = requestId;
    timeoutResult.tickMs = GetTickCount64();
    timeoutResult.objectKind = objectKind;
    timeoutResult.slot = slot;
    timeoutResult.onGameThread = true;
    kx::Log::Error("[RECHARGE_REQ][single] timeout request_id=%llu object=%s slot=%u timeout_ms=%llu",
                   requestId,
                   GetSkillbarObjectKindName(objectKind),
                   slot,
                   timeoutMs);
    return timeoutResult;
}

SkillRechargeTestResult SkillMemoryReader::getLastGameThreadRechargeTest() {
    std::lock_guard<std::mutex> lock(m_rechargeTestMutex);
    return m_lastGameThreadRechargeTest;
}

SkillRechargeSweepResult SkillMemoryReader::requestRechargeSweepOnGameThread(SkillbarObjectKind objectKind,
                                                                             uint32_t slotCount,
                                                                             unsigned long long timeoutMs) {
    if (kx::g_isShuttingDown.load(std::memory_order_acquire)) {
        SkillRechargeSweepResult aborted = {};
        aborted.tickMs = GetTickCount64();
        aborted.objectKind = objectKind;
        aborted.onGameThread = true;
        aborted.slotCount = (std::min)(slotCount, static_cast<uint32_t>(aborted.slots.size()));
        return aborted;
    }

    const uint32_t clampedSlotCount = (std::min)(slotCount, static_cast<uint32_t>(SkillRechargeSweepResult{}.slots.size()));
    unsigned long long requestId = 0;
    {
        std::lock_guard<std::mutex> lock(m_rechargeTestMutex);
        requestId = m_nextRechargeRequestId++;
        m_hasPendingRechargeSweep = true;
        m_pendingRechargeSweepRequestId = requestId;
        m_pendingRechargeSweepObjectKind = objectKind;
        m_pendingRechargeSweepSlotCount = clampedSlotCount;
    }

    kx::Log::Info("[RECHARGE_REQ][sweep] queued request_id=%llu object=%s slot_count=%u timeout_ms=%llu",
                  requestId,
                  GetSkillbarObjectKindName(objectKind),
                  clampedSlotCount,
                  timeoutMs);

    extern GameThreadHook g_gameThreadHook;
    g_gameThreadHook.ScheduleCapture();

    const unsigned long long start = GetTickCount64();
    while (GetTickCount64() - start <= timeoutMs) {
        if (kx::g_isShuttingDown.load(std::memory_order_acquire)) {
            std::lock_guard<std::mutex> lock(m_rechargeTestMutex);
            if (m_hasPendingRechargeSweep && m_pendingRechargeSweepRequestId == requestId) {
                m_hasPendingRechargeSweep = false;
            }
            break;
        }
        {
            std::lock_guard<std::mutex> lock(m_rechargeTestMutex);
            if (m_lastGameThreadRechargeSweep.requestId == requestId) {
                kx::Log::Info("[RECHARGE_REQ][sweep] completed request_id=%llu object=%s slot_count=%u",
                              requestId,
                              GetSkillbarObjectKindName(m_lastGameThreadRechargeSweep.objectKind),
                              static_cast<unsigned>(m_lastGameThreadRechargeSweep.slotCount));
                return m_lastGameThreadRechargeSweep;
            }
        }
        Sleep(2);
    }

    SkillRechargeSweepResult timeoutResult = {};
    timeoutResult.requestId = requestId;
    timeoutResult.tickMs = GetTickCount64();
    timeoutResult.objectKind = objectKind;
    timeoutResult.onGameThread = true;
    timeoutResult.slotCount = clampedSlotCount;
    kx::Log::Error("[RECHARGE_REQ][sweep] timeout request_id=%llu object=%s slot_count=%u timeout_ms=%llu",
                   requestId,
                   GetSkillbarObjectKindName(objectKind),
                   clampedSlotCount,
                   timeoutMs);
    return timeoutResult;
}

SkillRechargeSweepResult SkillMemoryReader::getLastGameThreadRechargeSweep() {
    std::lock_guard<std::mutex> lock(m_rechargeTestMutex);
    return m_lastGameThreadRechargeSweep;
}

VtableProbeBatchResult SkillMemoryReader::probeVtableFromSnapshot(const SkillbarSnapshot& snap,
                                                                  SkillbarObjectKind objectKind,
                                                                  uint32_t slot,
                                                                  int startIdx,
                                                                  int endIdx,
                                                                  bool onGameThread) {
    return ExecuteVtableProbeImpl(reinterpret_cast<void*>(GetSkillbarObjectPointer(snap, objectKind)),
                                  objectKind,
                                  slot,
                                  startIdx,
                                  endIdx,
                                  onGameThread);
}

VtableProbeBatchResult SkillMemoryReader::probeVtableCurrentThread(SkillbarObjectKind objectKind,
                                                                   uint32_t slot,
                                                                   int startIdx,
                                                                   int endIdx) {
    const SkillbarSnapshot snap = readNoWait();
    return probeVtableFromSnapshot(snap, objectKind, slot, startIdx, endIdx, false);
}

VtableProbeBatchResult SkillMemoryReader::requestVtableProbeOnGameThread(SkillbarObjectKind objectKind,
                                                                         uint32_t slot,
                                                                         int startIdx,
                                                                         int endIdx,
                                                                         unsigned long long timeoutMs) {
    if (kx::g_isShuttingDown.load(std::memory_order_acquire)) {
        VtableProbeBatchResult aborted = {};
        aborted.tickMs = GetTickCount64();
        aborted.objectKind = objectKind;
        aborted.slot = slot;
        aborted.startIndex = startIdx;
        aborted.endIndex = endIdx;
        aborted.onGameThread = true;
        return aborted;
    }

    unsigned long long requestId = 0;
    {
        std::lock_guard<std::mutex> lock(m_rechargeTestMutex);
        requestId = m_nextRechargeRequestId++;
        m_hasPendingVtableProbe = true;
        m_pendingVtableProbeRequestId = requestId;
        m_pendingVtableProbeObjectKind = objectKind;
        m_pendingVtableProbeSlot = slot;
        m_pendingVtableProbeStartIndex = startIdx;
        m_pendingVtableProbeEndIndex = endIdx;
    }

    extern GameThreadHook g_gameThreadHook;
    g_gameThreadHook.ScheduleCapture();

    const unsigned long long start = GetTickCount64();
    while (GetTickCount64() - start <= timeoutMs) {
        if (kx::g_isShuttingDown.load(std::memory_order_acquire)) {
            std::lock_guard<std::mutex> lock(m_rechargeTestMutex);
            if (m_hasPendingVtableProbe && m_pendingVtableProbeRequestId == requestId) {
                m_hasPendingVtableProbe = false;
            }
            break;
        }
        {
            std::lock_guard<std::mutex> lock(m_rechargeTestMutex);
            if (m_lastGameThreadVtableProbe.requestId == requestId) {
                return m_lastGameThreadVtableProbe;
            }
        }
        Sleep(2);
    }

    VtableProbeBatchResult timeoutResult = {};
    timeoutResult.requestId = requestId;
    timeoutResult.tickMs = GetTickCount64();
    timeoutResult.objectKind = objectKind;
    timeoutResult.slot = slot;
    timeoutResult.startIndex = startIdx;
    timeoutResult.endIndex = endIdx;
    timeoutResult.onGameThread = true;
    return timeoutResult;
}

VtableProbeBatchResult SkillMemoryReader::getLastGameThreadVtableProbe() {
    std::lock_guard<std::mutex> lock(m_rechargeTestMutex);
    return m_lastGameThreadVtableProbe;
}

void SkillMemoryReader::processPendingRechargeTestFromSnapshot(const SkillbarSnapshot& snap) {
    bool hasPendingSingle = false;
    SkillbarObjectKind objectKind = SkillbarObjectKind::ChCliSkillbar;
    uint32_t slot = 0;
    unsigned long long requestId = 0;
    bool hasPendingSweep = false;
    SkillbarObjectKind sweepObjectKind = SkillbarObjectKind::ChCliSkillbar;
    uint32_t sweepSlotCount = 0;
    unsigned long long sweepRequestId = 0;
    {
        std::lock_guard<std::mutex> lock(m_rechargeTestMutex);
        if (m_hasPendingRechargeTest) {
            hasPendingSingle = true;
            objectKind = m_pendingRechargeObjectKind;
            slot = m_pendingRechargeSlot;
            requestId = m_pendingRechargeRequestId;
            m_hasPendingRechargeTest = false;
        }
        if (m_hasPendingRechargeSweep) {
            hasPendingSweep = true;
            sweepObjectKind = m_pendingRechargeSweepObjectKind;
            sweepSlotCount = m_pendingRechargeSweepSlotCount;
            sweepRequestId = m_pendingRechargeSweepRequestId;
            m_hasPendingRechargeSweep = false;
        }
    }

    if (hasPendingSingle) {
        SkillRechargeTestResult result = testIsRechargingFromSnapshot(snap, objectKind, slot, true);
        result.requestId = requestId;

        {
            std::lock_guard<std::mutex> lock(m_rechargeTestMutex);
            m_lastGameThreadRechargeTest = result;
        }

        kx::Log::Info("[RECHARGE][%s][slot=%u][thread=%s] object=0x%p vtable=0x%p fn[7]=0x%p executed=%s returned=%s faulted=%s",
                      GetSkillbarObjectKindName(objectKind),
                      slot,
                      result.onGameThread ? "game" : "other",
                      reinterpret_cast<void*>(result.objectPtr),
                      reinterpret_cast<void*>(result.vtablePtr),
                      reinterpret_cast<void*>(result.functionPtr),
                      result.executed ? "yes" : "no",
                      result.returnedValue ? "true" : "false",
                      result.callFaulted ? "yes" : "no");
    }

    if (!hasPendingSingle && !hasPendingSweep) {
        return;
    }

    if (hasPendingSweep) {
        SkillRechargeSweepResult sweep = ExecuteRechargeSweepImpl(reinterpret_cast<void*>(GetSkillbarObjectPointer(snap, sweepObjectKind)),
                                                                  sweepObjectKind,
                                                                  sweepSlotCount,
                                                                  true);
        sweep.requestId = sweepRequestId;

        {
            std::lock_guard<std::mutex> lock(m_rechargeTestMutex);
            m_lastGameThreadRechargeSweep = sweep;
        }

        std::ostringstream summary;
        summary << "[RECHARGE_SWEEP][" << GetSkillbarObjectKindName(sweepObjectKind) << "][thread=game]"
                << " request_id=" << sweepRequestId
                << " slots=";
        for (size_t i = 0; i < sweep.slotCount; ++i) {
            const auto& entry = sweep.slots[i];
            if (i > 0) {
                summary << ",";
            }
            summary << i << ":";
            if (!entry.executed) {
                summary << (entry.callFaulted ? "fault" : "na");
            } else {
                summary << (entry.returnedValue ? "1" : "0");
            }
        }
        kx::Log::Info("%s", summary.str().c_str());
    }
}

void SkillMemoryReader::processPendingVtableProbeFromSnapshot(const SkillbarSnapshot& snap) {
    SkillbarObjectKind objectKind = SkillbarObjectKind::ChCliSkillbar;
    uint32_t slot = 0;
    int startIdx = 0;
    int endIdx = 0;
    unsigned long long requestId = 0;
    {
        std::lock_guard<std::mutex> lock(m_rechargeTestMutex);
        if (!m_hasPendingVtableProbe) {
            return;
        }
        objectKind = m_pendingVtableProbeObjectKind;
        slot = m_pendingVtableProbeSlot;
        startIdx = m_pendingVtableProbeStartIndex;
        endIdx = m_pendingVtableProbeEndIndex;
        requestId = m_pendingVtableProbeRequestId;
        m_hasPendingVtableProbe = false;
    }

    VtableProbeBatchResult batch = probeVtableFromSnapshot(snap, objectKind, slot, startIdx, endIdx, true);
    batch.requestId = requestId;

    {
        std::lock_guard<std::mutex> lock(m_rechargeTestMutex);
        m_lastGameThreadVtableProbe = batch;
    }
}

bool SkillMemoryReader::resolveSnapshotFromContext(uintptr_t ctxAddr, SkillbarSnapshot& snap, bool enableLog, const char* origin) {
    const char* label = origin ? origin : "CTX";
    auto logInfo = [&](const char* fmt, auto... args) {
        if (enableLog) {
            kx::Log::Info(fmt, args...);
        }
    };
    auto logError = [&](const char* fmt, auto... args) {
        if (enableLog) {
            kx::Log::Error(fmt, args...);
        }
    };

    snap.ctxAddr = ctxAddr;
    logInfo("[%s] begin ctx=0x%p", label, reinterpret_cast<void*>(ctxAddr));
    if (!ctxAddr) {
        logError("[%s][CTX] fail: GetContextCollection returned null", label);
        return false;
    }

    uintptr_t chCtxAddr = 0;
    if (!TryReadValue(ctxAddr + 0x98, chCtxAddr)) {
        logError("[%s][CTX] fail: read pChCliContext at ctx+0x98", label);
        return false;
    }
    if (!chCtxAddr) {
        logError("[%s][CTX] fail: pChCliContext null at ctx+0x98", label);
        return false;
    }
    snap.chCliContextAddr = chCtxAddr;
    logInfo("[%s][CTX] context resolved: pChCliContext = 0x%p from [ctx+0x98]", label, reinterpret_cast<void*>(chCtxAddr));

    uintptr_t localChar = 0;
    if (!TryReadValue(chCtxAddr + 0x98, localChar)) {
        logError("[%s][LOCAL] fail: read pLocalChCliCharacter at pChCliContext+0x98", label);
        return false;
    }
    if (!localChar) {
        logError("[%s][LOCAL] fail: pLocalChCliCharacter null at pChCliContext+0x98", label);
        return false;
    }
    snap.localCharAddr = localChar;
    logInfo("[%s][LOCAL] local character resolved: pLocalChCliCharacter = 0x%p from [pChCliContext+0x98]",
            label,
            reinterpret_cast<void*>(localChar));

    std::memset(snap.position, 0, sizeof(snap.position));
    if (!TryCopyBytes(snap.position, localChar + 0x480, sizeof(snap.position))) {
        logError("[%s] fail: read position at pLocalChCliCharacter+0x480", label);
    } else {
        logInfo("[%s] step: position = {%.3f, %.3f, %.3f}",
                label,
                snap.position[0],
                snap.position[1],
                snap.position[2]);
    }

    uintptr_t pEnergies = 0;
    if (!TryReadValue(localChar + 0x3D8, pEnergies)) {
        logError("[%s][ENERGY] fail: read pEnergies at pLocalChCliCharacter+0x3D8", label);
    } else if (!pEnergies) {
        logError("[%s][ENERGY] fail: pEnergies null at pLocalChCliCharacter+0x3D8", label);
    } else {
        snap.energiesAddr = pEnergies;
        float fcur = 0.0f;
        float fmax = 0.0f;
        float fdelta = 0.0f;
        float frec = 0.0f;
        TryReadValue(pEnergies + 0x0C, fcur);
        TryReadValue(pEnergies + 0x10, fmax);
        TryReadValue(pEnergies + 0x14, fdelta);
        TryReadValue(pEnergies + 0x18, frec);
        snap.currentEnergy = fcur;
        snap.maximumEnergy = fmax;
        snap.energyDelta = fdelta;
        snap.recoveryRate = frec;
        logInfo("[%s][ENERGY] step: pEnergies = 0x%p current=%.2f max=%.2f delta=%.2f regen=%.2f",
                label,
                reinterpret_cast<void*>(pEnergies),
                snap.currentEnergy,
                snap.maximumEnergy,
                snap.energyDelta,
                snap.recoveryRate);
    }

    const SkillbarPointerOffsets offsets = m_skillbarOffsets;
    uintptr_t chCliSkillbar = 0;
    if (!TryReadValue(localChar + offsets.charSkillbar, chCliSkillbar)) {
        logError("[%s][SKILLBAR] fail: read pChCliSkillbar at pLocalChCliCharacter+0x%zX",
                 label,
                 static_cast<size_t>(offsets.charSkillbar));
        return false;
    }
    if (!chCliSkillbar) {
        logError("[%s][SKILLBAR] fail: pChCliSkillbar null at pLocalChCliCharacter+0x%zX",
                 label,
                 static_cast<size_t>(offsets.charSkillbar));
        return false;
    }
    snap.chCliSkillbarAddr = chCliSkillbar;
    logInfo("[%s][SKILLBAR] pChCliSkillbar resolved = 0x%p from [pLocalChCliCharacter+0x%zX]",
            label,
            reinterpret_cast<void*>(chCliSkillbar),
            static_cast<size_t>(offsets.charSkillbar));
    std::memset(snap.rawChCliSkillbar, 0, sizeof(snap.rawChCliSkillbar));
    if (!TryCopyBytes(snap.rawChCliSkillbar, chCliSkillbar, sizeof(snap.rawChCliSkillbar))) {
        logError("[%s][SKILLBAR] fail: copy ChCliSkillbar bytes from 0x%p",
                 label,
                 reinterpret_cast<void*>(chCliSkillbar));
    }

    if (!TryReadValue(chCliSkillbar + 0x80, snap.asContextAddr)) {
        logError("[%s] fail: read asContext at pChCliSkillbar+0x80", label);
    } else {
        logInfo("[%s] step: asContext = 0x%p from [pChCliSkillbar+0x80]",
                label,
                reinterpret_cast<void*>(snap.asContextAddr));
    }

    uintptr_t candidateSkillbar = 0;
    bool haveSkillbarViaArrayOffset = false;
    if (offsets.skillbarSkillsArray != 0 &&
        TryReadValue(chCliSkillbar + offsets.skillbarSkillsArray, candidateSkillbar) &&
        candidateSkillbar) {
        uint8_t probe[kCharSkillbarPrimarySize] = {};
        if (TryCopyBytes(probe, candidateSkillbar, sizeof(probe))) {
            snap.charSkillbarAddr = candidateSkillbar;
            snap.skillbarSkillsArrayAddr = chCliSkillbar + offsets.skillbarSkillsArray;
            haveSkillbarViaArrayOffset = true;
        }
    }

    if (!haveSkillbarViaArrayOffset) {
        if (!TryReadValue(chCliSkillbar + offsets.legacyCharSkillbarFromSkillbar, snap.charSkillbarAddr)) {
            logError("[%s][SKILLBAR] fail: read pCharSkillbar at pChCliSkillbar+0x%zX",
                     label,
                     static_cast<size_t>(offsets.legacyCharSkillbarFromSkillbar));
            return false;
        }
        if (!snap.charSkillbarAddr) {
            logError("[%s][SKILLBAR] fail: pCharSkillbar null at pChCliSkillbar+0x%zX",
                     label,
                     static_cast<size_t>(offsets.legacyCharSkillbarFromSkillbar));
            return false;
        }
    }
    if (haveSkillbarViaArrayOffset) {
        logInfo("[%s][SKILLBAR] pCharSkillbar resolved = 0x%p from [pChCliSkillbar+0x%zX] (skills_array)",
                label,
                reinterpret_cast<void*>(snap.charSkillbarAddr),
                static_cast<size_t>(offsets.skillbarSkillsArray));
    } else {
        logInfo("[%s][SKILLBAR] pCharSkillbar resolved = 0x%p from [pChCliSkillbar+0x%zX] (legacy)",
                label,
                reinterpret_cast<void*>(snap.charSkillbarAddr),
                static_cast<size_t>(offsets.legacyCharSkillbarFromSkillbar));
    }

    std::memset(snap.rawCharSkillbar, 0, sizeof(snap.rawCharSkillbar));
    if (!TryCopyBytes(snap.rawCharSkillbar, snap.charSkillbarAddr, sizeof(snap.rawCharSkillbar))) {
        logError("[%s][SNAPSHOT] fail: copy raw CharSkillbar bytes from 0x%p",
                 label,
                 reinterpret_cast<void*>(snap.charSkillbarAddr));
        return false;
    }
    std::memset(snap.rawCharSkillbarWide, 0, sizeof(snap.rawCharSkillbarWide));
    if (!TryCopyBytes(snap.rawCharSkillbarWide, snap.charSkillbarAddr, sizeof(snap.rawCharSkillbarWide))) {
        logError("[%s][SNAPSHOT] fail: copy wide CharSkillbar bytes from 0x%p",
                 label,
                 reinterpret_cast<void*>(snap.charSkillbarAddr));
    }

    const uintptr_t ptr40 = ReadLe64(snap.rawCharSkillbar, 0x40);
    const uintptr_t ptr48 = ReadLe64(snap.rawCharSkillbar, 0x48);
    const uintptr_t ptr60 = ReadLe64(snap.rawCharSkillbar, 0x60);
    snap.ptr40Addr = ptr40;
    snap.ptr48Addr = ptr48;
    snap.ptr60Addr = ptr60;

    if (ptr40 && !TryCopyBytes(snap.rawPtr40, ptr40, sizeof(snap.rawPtr40))) {
        logError("[%s] fail: copy ptr40 region from 0x%p", label, reinterpret_cast<void*>(ptr40));
    }
    if (ptr48 && !TryCopyBytes(snap.rawPtr48, ptr48, sizeof(snap.rawPtr48))) {
        logError("[%s] fail: copy ptr48 region from 0x%p", label, reinterpret_cast<void*>(ptr48));
    }
    if (ptr60 && !TryCopyBytes(snap.rawPtr60, ptr60, sizeof(snap.rawPtr60))) {
        logError("[%s] fail: copy ptr60 region from 0x%p", label, reinterpret_cast<void*>(ptr60));
    }

    logInfo("[%s] step: ptr40=0x%p ptr48=0x%p ptr60=0x%p",
            label,
            reinterpret_cast<void*>(ptr40),
            reinterpret_cast<void*>(ptr48),
            reinterpret_cast<void*>(ptr60));

    for (int i = 0; i < 10; ++i) {
        auto& slot = snap.slots[i];
        slot.slotIndex = i;
        uintptr_t slotAddr = snap.charSkillbarAddr + i * 0x28;
        slot.slotAddr = slotAddr;
        slot.valid = TryCopyBytes(slot.rawBytes, slotAddr, sizeof(slot.rawBytes));
        if (!slot.valid) {
            logError("[%s][SLOT] fail: copy slot[%d] bytes from 0x%p",
                     label,
                     i,
                     reinterpret_cast<void*>(slotAddr));
        }
    }

    if (offsets.skillbarPressedSkill != 0) {
        uint8_t pressedSkill = 0;
        if (TryReadValue(chCliSkillbar + offsets.skillbarPressedSkill, pressedSkill)) {
            snap.skillbarPressedSkillRaw = static_cast<uint32_t>(pressedSkill);
            if (pressedSkill < snap.slots.size()) {
                snap.focusSlotIndex = static_cast<uint32_t>(pressedSkill);
                snap.focusSlotAddr = snap.slots[pressedSkill].slotAddr;
            }
            logInfo("[%s][SKWATCH] skillbar_pressed_raw=%u offset=0x%zX focus_slot=%s",
                    label,
                    static_cast<unsigned>(pressedSkill),
                    static_cast<size_t>(offsets.skillbarPressedSkill),
                    (pressedSkill < snap.slots.size()) ? "resolved" : "na");
        } else {
            logError("[%s][SKWATCH] fail: read skillbarPressedSkill at pChCliSkillbar+0x%zX",
                     label,
                     static_cast<size_t>(offsets.skillbarPressedSkill));
        }
    } else {
        snap.skillbarPressedSkillRaw = (std::numeric_limits<uint32_t>::max)();
    }

    snap.valid = true;
    logInfo("[%s][SNAPSHOT] snapshot captured: ctx=0x%p pChCliContext=0x%p pLocalChCliCharacter=0x%p pChCliSkillbar=0x%p pCharSkillbar=0x%p wide_size=0x%zX",
            label,
            reinterpret_cast<void*>(snap.ctxAddr),
            reinterpret_cast<void*>(snap.chCliContextAddr),
            reinterpret_cast<void*>(snap.localCharAddr),
            reinterpret_cast<void*>(snap.chCliSkillbarAddr),
            reinterpret_cast<void*>(snap.charSkillbarAddr),
            sizeof(snap.rawCharSkillbarWide));
    return true;
}

void SkillMemoryReader::diffDump(const SkillbarSnapshot& before,
                                 const SkillbarSnapshot& after,
                                 char* outBuf, size_t bufSize) {
    if (!outBuf || bufSize == 0) return;
    size_t used = 0;
    used += snprintf(outBuf + used, (used < bufSize) ? bufSize - used : 0, "Diffs (offset: before -> after)\n");
    std::vector<size_t> changedOffsets;
    for (size_t i = 0; i < sizeof(before.rawCharSkillbar) && i < sizeof(after.rawCharSkillbar); ++i) {
        uint8_t b = before.rawCharSkillbar[i];
        uint8_t a = after.rawCharSkillbar[i];
        if (b != a) {
            changedOffsets.push_back(i);
            int n = snprintf(outBuf + used, (used < bufSize) ? bufSize - used : 0, "%04zX: %02X -> %02X\n", i, b, a);
            if (n <= 0) break;
            used += (size_t)n;
            if (used >= bufSize) break;
        }
    }

    if (changedOffsets.empty() || used >= bufSize) return;

    used += snprintf(outBuf + used, (used < bufSize) ? bufSize - used : 0, "\nGrouped fields\n");
    if (used >= bufSize) return;

    size_t idx = 0;
    while (idx < changedOffsets.size() && used < bufSize) {
        size_t start = changedOffsets[idx];
        size_t end = start;
        while (idx + 1 < changedOffsets.size() && changedOffsets[idx + 1] == end + 1) {
            ++idx;
            end = changedOffsets[idx];
        }

        int n = snprintf(outBuf + used,
                         (used < bufSize) ? bufSize - used : 0,
                         "[%04zX..%04zX] len=%zu\n",
                         start, end, (end - start + 1));
        if (n <= 0) break;
        used += static_cast<size_t>(n);
        if (used >= bufSize) break;

        if (start + 4 <= sizeof(before.rawCharSkillbar)) {
            uint32_t before32 = ReadLe32(before.rawCharSkillbar, start);
            uint32_t after32 = ReadLe32(after.rawCharSkillbar, start);
            n = snprintf(outBuf + used,
                         (used < bufSize) ? bufSize - used : 0,
                         "  u32 @ %04zX : %08X -> %08X\n",
                         start, before32, after32);
            if (n <= 0) break;
            used += static_cast<size_t>(n);
            if (used >= bufSize) break;
        }

        if (start + 8 <= sizeof(before.rawCharSkillbar)) {
            uint64_t before64 = ReadLe64(before.rawCharSkillbar, start);
            uint64_t after64 = ReadLe64(after.rawCharSkillbar, start);
            n = snprintf(outBuf + used,
                         (used < bufSize) ? bufSize - used : 0,
                         "  u64 @ %04zX : %016llX -> %016llX\n",
                         start,
                         static_cast<unsigned long long>(before64),
                         static_cast<unsigned long long>(after64));
            if (n <= 0) break;
            used += static_cast<size_t>(n);
            if (used >= bufSize) break;
        }

        ++idx;
    }
}
