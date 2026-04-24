#pragma once

#include <cstdint>
#include <array>
#include <cstddef>
#include <mutex>

struct SkillSlotRaw {
    uintptr_t slotAddr = 0;
    uint8_t  rawBytes[0x40];
    uint32_t slotIndex;
    bool     valid = false;
};

struct SkillbarSnapshot {
    uintptr_t            ctxAddr = 0;
    uintptr_t            chCliContextAddr = 0;
    uintptr_t            localCharAddr = 0;
    uintptr_t            energiesAddr = 0;
    uintptr_t            chCliSkillbarAddr = 0;
    uintptr_t            charSkillbarAddr = 0;
    uintptr_t            asContextAddr    = 0;
    uintptr_t            ptr40Addr = 0;
    uintptr_t            ptr48Addr = 0;
    uintptr_t            ptr60Addr = 0;
    uintptr_t            focusSlotAddr = 0;
    uint32_t             focusSlotIndex = 0xFFFFFFFFu;
    uint32_t             skillbarPressedSkillRaw = 0xFFFFFFFFu;
    uintptr_t            skillbarSkillsArrayAddr = 0;
    std::array<SkillSlotRaw, 10> slots;
    uint8_t  rawCharSkillbar[0x188];
    uint8_t  rawCharSkillbarWide[0x400] = {};
    uint8_t  rawChCliSkillbar[0x240] = {};
    uint8_t  rawPtr40[0x80] = {};
    uint8_t  rawPtr48[0x80] = {};
    uint8_t  rawPtr60[0x80] = {};
    float    position[3] = {0.0f, 0.0f, 0.0f};
    // Energy/resource snapshot
    float    currentEnergy = 0.0f;
    float    maximumEnergy = 0.0f;
    float    energyDelta = 0.0f;
    float    recoveryRate = 0.0f;
    bool     valid = false;
};

struct SkillbarPointerOffsets {
    uintptr_t charSkillbar = 0x520;              // GW2LIB::Mems::charSkillbar
    uintptr_t skillbarSkillsArray = 0x0;         // GW2LIB::Mems::skillbarSkillsArray (scan result if available)
    uintptr_t skillbarPressedSkill = 0x0;        // GW2LIB::Mems::skillbarPressedSkill (scan result if available)
    uintptr_t skillbarVtSkillDown = 0x1A8;       // GW2LIB::Mems::skillbarVtSkillDown
    uintptr_t skillbarVtSkillUp = 0x1B8;         // GW2LIB::Mems::skillbarVtSkillUp
    uintptr_t legacyCharSkillbarFromSkillbar = 0x150; // Legacy fallback path
};

enum class SkillbarObjectKind {
    ChCliSkillbar,
    CharSkillbar,
};

struct SkillRechargeTestResult {
    unsigned long long requestId = 0;
    unsigned long long tickMs = 0;
    uintptr_t objectPtr = 0;
    uintptr_t vtablePtr = 0;
    uintptr_t functionPtr = 0;
    uint32_t slot = 0;
    uint32_t vtableIndex = 7;
    SkillbarObjectKind objectKind = SkillbarObjectKind::ChCliSkillbar;
    bool executed = false;
    bool returnedValue = false;
    bool callFaulted = false;
    bool onGameThread = false;
};

struct SkillRechargeSweepResult {
    unsigned long long requestId = 0;
    unsigned long long tickMs = 0;
    SkillbarObjectKind objectKind = SkillbarObjectKind::ChCliSkillbar;
    bool onGameThread = false;
    size_t slotCount = 0;
    std::array<SkillRechargeTestResult, 10> slots = {};
};

struct VtableProbeResult {
    int index = -1;
    uintptr_t fnAddr = 0;
    bool faulted = false;
    bool returned = false;
};

struct VtableProbeBatchResult {
    unsigned long long requestId = 0;
    unsigned long long tickMs = 0;
    uintptr_t objectPtr = 0;
    uintptr_t vtablePtr = 0;
    uint32_t slot = 0;
    int startIndex = 0;
    int endIndex = -1;
    SkillbarObjectKind objectKind = SkillbarObjectKind::ChCliSkillbar;
    bool onGameThread = false;
    std::array<VtableProbeResult, 21> entries = {};
    size_t entryCount = 0;
};

class SkillMemoryReader {
public:
    bool init();
    SkillbarSnapshot read();
    SkillbarSnapshot readNoWait();
    // Capture directly from a known ContextCollection address (called on game logic thread)
    SkillbarSnapshot captureFromCtx(uintptr_t ctxAddr, bool enableLog = true);
    uintptr_t getCtxFnAddr() const { return reinterpret_cast<uintptr_t>(m_fnGetCtx); }
    uintptr_t getCachedCtxAddr() const { return m_cachedCtxAddr; }
    uintptr_t refreshContextCollection();
    static const char* GetSkillbarObjectKindName(SkillbarObjectKind kind);
    static bool TestIsRecharging(void* skillbarPtr, uint32_t slot);
    SkillRechargeTestResult testIsRechargingFromSnapshot(const SkillbarSnapshot& snap,
                                                         SkillbarObjectKind objectKind,
                                                         uint32_t slot,
                                                         bool onGameThread);
    SkillRechargeTestResult testIsRechargingCurrentThread(SkillbarObjectKind objectKind, uint32_t slot);
    SkillRechargeTestResult requestIsRechargingOnGameThread(SkillbarObjectKind objectKind,
                                                            uint32_t slot,
                                                            unsigned long long timeoutMs = 100);
    SkillRechargeTestResult getLastGameThreadRechargeTest();
    SkillRechargeSweepResult requestRechargeSweepOnGameThread(SkillbarObjectKind objectKind,
                                                              uint32_t slotCount = 10,
                                                              unsigned long long timeoutMs = 150);
    SkillRechargeSweepResult getLastGameThreadRechargeSweep();
    VtableProbeBatchResult probeVtableFromSnapshot(const SkillbarSnapshot& snap,
                                                   SkillbarObjectKind objectKind,
                                                   uint32_t slot,
                                                   int startIdx,
                                                   int endIdx,
                                                   bool onGameThread);
    VtableProbeBatchResult probeVtableCurrentThread(SkillbarObjectKind objectKind,
                                                    uint32_t slot,
                                                    int startIdx,
                                                    int endIdx);
    VtableProbeBatchResult requestVtableProbeOnGameThread(SkillbarObjectKind objectKind,
                                                          uint32_t slot,
                                                          int startIdx,
                                                          int endIdx,
                                                          unsigned long long timeoutMs = 100);
    VtableProbeBatchResult getLastGameThreadVtableProbe();
    static void diffDump(const SkillbarSnapshot& before,
                         const SkillbarSnapshot& after,
                         char* outBuf, size_t bufSize);
    void setSkillbarPointerOffsets(const SkillbarPointerOffsets& offsets);
    SkillbarPointerOffsets getSkillbarPointerOffsets() const;

private:
    using GetContextCollection_t = void*(*)();
    uintptr_t findContextCollection();
    bool resolveSnapshotFromContext(uintptr_t ctxAddr, SkillbarSnapshot& snap, bool enableLog, const char* origin);
    void processPendingRechargeTestFromSnapshot(const SkillbarSnapshot& snap);
    void processPendingVtableProbeFromSnapshot(const SkillbarSnapshot& snap);
    void storeSnapshot(const SkillbarSnapshot& snap);
    SkillbarSnapshot getCachedSnapshot();

    GetContextCollection_t m_fnGetCtx = nullptr;
    uintptr_t m_cachedCtxAddr = 0;
    bool m_isInitialized = false;
    unsigned long long m_lastUiRefreshTickMs = 0;
    std::mutex m_snapshotMutex;
    SkillbarSnapshot m_lastSnapshot = {};
    SkillbarPointerOffsets m_skillbarOffsets = {};
    std::mutex m_rechargeTestMutex;
    unsigned long long m_nextRechargeRequestId = 1;
    bool m_hasPendingRechargeTest = false;
    unsigned long long m_pendingRechargeRequestId = 0;
    SkillbarObjectKind m_pendingRechargeObjectKind = SkillbarObjectKind::ChCliSkillbar;
    uint32_t m_pendingRechargeSlot = 0;
    SkillRechargeTestResult m_lastGameThreadRechargeTest = {};
    bool m_hasPendingRechargeSweep = false;
    unsigned long long m_pendingRechargeSweepRequestId = 0;
    SkillbarObjectKind m_pendingRechargeSweepObjectKind = SkillbarObjectKind::ChCliSkillbar;
    uint32_t m_pendingRechargeSweepSlotCount = 0;
    SkillRechargeSweepResult m_lastGameThreadRechargeSweep = {};
    bool m_hasPendingVtableProbe = false;
    unsigned long long m_pendingVtableProbeRequestId = 0;
    SkillbarObjectKind m_pendingVtableProbeObjectKind = SkillbarObjectKind::ChCliSkillbar;
    uint32_t m_pendingVtableProbeSlot = 0;
    int m_pendingVtableProbeStartIndex = 0;
    int m_pendingVtableProbeEndIndex = 0;
    VtableProbeBatchResult m_lastGameThreadVtableProbe = {};
};

// Global instance for UI to use
extern SkillMemoryReader g_skillReader;
