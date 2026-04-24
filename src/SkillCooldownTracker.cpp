#define NOMINMAX
#include "SkillCooldownTracker.h"

#include "AppState.h"
#include "SkillDb.h"
#include "SkillMemoryReader.h"
#include "SkillTraceCoordinator.h"

#include <windows.h>
#include <psapi.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <cstring>
#include <deque>
#include <fstream>
#include <iomanip>
#include <map>
#include <mutex>
#include <limits>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

extern HINSTANCE dll_handle;

namespace kx::SkillCooldown {
namespace {
using kx::SkillDb::SkillConfig;

constexpr bool kEnableCooldownProbe = false;
constexpr bool kEnableSkillbarMemoryWatch = true;
constexpr size_t kPtr60PayloadOffset = 0x18;
constexpr size_t kPtr60PayloadLen = 0x80 - kPtr60PayloadOffset;
constexpr size_t kMaxCandidateSmsgRecords = 32;
constexpr size_t kMaxCandidateSmsgPayloadBytes = 64;
constexpr bool kEnableAutoDebugAssistSnapshots = false;
constexpr bool kEnablePacketCorrelationTimeline = false;
constexpr bool kEnableLegacyCooldownStageLogs = false;
constexpr bool kEnableLegacyZStateLogs = false;
constexpr unsigned long long kCooldownWatchRestartDedupMs = 750;
constexpr unsigned long long kPtrPacketWindowMs = 500;
constexpr unsigned long long kCooldownWatchWindowMs = 8000;
constexpr unsigned long long kInternalPollWindowMs = 3000;
constexpr unsigned long long kInternalPollIntervalMs = 2;
constexpr bool kEnableCandidateSmsgTracking = false;
constexpr bool kEnableCandidateSmsgDetailedLog = false;
constexpr size_t kMax0017Packets = 128;
constexpr size_t kMax0028Packets = 128;
constexpr size_t kMaxPtrWriteHits = 64;

struct RechargeTimer {
    unsigned long long endTickMs = 0;
};

struct ZNamedCluster {
    std::string name;
    uintptr_t cur40 = 0;
    uintptr_t cur60 = 0;
    uintptr_t ptr48Plus20 = 0;
};

struct NodePairState {
    uintptr_t readyNode40 = 0;
    uintptr_t readyNode60 = 0;
    uintptr_t readyPtr48Plus20 = 0;
    uintptr_t cooldownNode40 = 0;
    uintptr_t cooldownNode60 = 0;
    uintptr_t zStableA40 = 0;
    uintptr_t zStableA60 = 0;
    uintptr_t zStableAPtr48Plus20 = 0;
    std::vector<ZNamedCluster> zExtraClusters;
    unsigned int nextZClusterOrdinal = 1;
    bool sawReadyBaseline = false;
    bool sawCooldownBaseline = false;
    bool isCooldown = false;
    std::string stateText = "NODE_UNKNOWN";
    std::array<uint8_t, kPtr60PayloadLen> lastCooldownPayload = {};
    bool hasLastCooldownPayload = false;
};

struct TrackedSkillState {
    size_t configIndex = 0;
    int currentCharges = 1;
    unsigned long long standardCdEndMs = 0;
    unsigned long long castCdEndMs = 0;
    std::vector<RechargeTimer> rechargeTimers;
    std::string lastArchiveKey;
    bool wasCastable = true;
    bool wasFull = true;
    NodePairState nodeState;
};

struct PacketUseEvent {
    std::string archiveKey;
    std::string hotkeyName;
    int hotkeyId = static_cast<int>(kx::SkillHotkeyId::None);
    unsigned long long packetTickMs = 0;
    unsigned long long runId = 0;
};

struct TimedPayload0017 {
    unsigned long long elapsedMs = 0;
    std::vector<uint8_t> payload;
};

struct TimedPacketRecord {
    unsigned long long elapsedMs = 0;
    uint16_t rawHeaderId = 0;
    std::string name;
    std::vector<uint8_t> payload;
};

struct PtrWriteHitRecord {
    unsigned long long elapsedMs = 0;
    PtrWriteHitKind kind = PtrWriteHitKind::PTR40;
    uintptr_t hitAddr = 0;
    bool haveOldValue = false;
    uint64_t oldValue = 0;
    uint64_t newValue = 0;
    uintptr_t rip = 0;
    uint32_t slot = UINT32_MAX;
    uint32_t phase = 0;
    std::string source;
};

struct InternalPollTarget {
    PtrWriteHitKind kind = PtrWriteHitKind::PTR40;
    std::string label;
    uintptr_t addr = 0;
    uint64_t lastValue = 0;
    bool valid = false;
};

struct InternalPollingWatcher {
    bool active = false;
    unsigned long long runId = 0;
    std::string hotkeyName;
    int skillId = 0;
    unsigned long long startedTickMs = 0;
    unsigned long long untilTickMs = 0;
    unsigned long long lastPollTickMs = 0;
    uint32_t slot = UINT32_MAX;
    int durationMs = 0;
    uint64_t baselinePtr40 = 0;
    uint64_t baselinePtr60 = 0;
    unsigned long long cdStartTickMs = 0;
    bool readyToCooldownObserved = false;
    bool cooldownToReadyObserved = false;
    bool ptr40NodeCdSeen = false;
    bool ptr60NodeCdSeen = false;
    bool ptr40NodeReadySeen = false;
    bool ptr60NodeReadySeen = false;
    bool transitionSummaryLogged = false;
    size_t totalHitCount = 0;
    std::array<InternalPollTarget, 4> targets = {};
};

struct CooldownInfoLike {
    uint32_t slot = UINT32_MAX;
    std::string hotkey;
    int skillId = 0;
    uint64_t baselinePtr40 = 0;
    uint64_t baselinePtr60 = 0;
    uint64_t cdPtr40 = 0;
    uint64_t cdPtr60 = 0;
    unsigned long long cdStartTickMs = 0;
    int durationMs = 0;
    bool observedReady = false;
    unsigned long long observedReadyTickMs = 0;
    unsigned long long observedDurationMs = 0;
};

struct EstimatedCooldownState {
    bool available = false;
    bool estimated = true;
    std::string hotkey;
    std::string skillName;
    int durationMs = 0;
    unsigned long long cdStartTickMs = 0;
    bool cooldownActive = false;
    uint64_t ptr40Value = 0;
    uint64_t ptr60Value = 0;
};

struct TimedZStateSnapshot {
    unsigned long long runId = 0;
    unsigned long long elapsedMs = 0;
    std::string phaseName;
    std::string oldStateText;
    std::string stateText;
    uintptr_t cur40 = 0;
    uintptr_t cur60 = 0;
    uintptr_t ptr48Plus20 = 0;
};

struct CandidateSmsgRecord {
    unsigned long long elapsedMs = 0;
    uint16_t rawHeaderId = 0;
    std::string name;
    std::vector<uint8_t> payload;
};

struct CooldownPacketWatch {
    bool active = false;
    unsigned long long runId = 0;
    std::string hotkeyName;
    std::string archiveKey;
    unsigned long long startedTickMs = 0;
    unsigned long long untilTickMs = 0;
    bool loggedReadyBaseline = false;
    bool loggedCooldownStart = false;
    bool loggedReadyReturn = false;
    bool haveNodeCdOffset = false;
    unsigned long long nodeCdOffsetMs = 0;
    bool haveNodeReadyBaselineOffset = false;
    unsigned long long nodeReadyBaselineOffsetMs = 0;
    bool haveNodeReadyOffset = false;
    unsigned long long nodeReadyOffsetMs = 0;
    bool dumpedNodeReadyWindow = false;
    bool dumpedChCliDiff = false;
    std::vector<std::pair<unsigned long long, std::string>> groupEvents;
    std::vector<TimedPayload0017> all0017Packets;
    std::vector<TimedPacketRecord> all0028Packets;
    std::vector<PtrWriteHitRecord> ptrWriteHits;
    std::vector<CandidateSmsgRecord> candidateSmsgs;
    std::vector<TimedZStateSnapshot> zStateSnapshots;
    SkillbarSnapshot baselineSnapshot = {};
    bool loggedSkillbarT0 = false;
    bool loggedSkillbarNodeCd = false;
    bool loggedSkillbarNodeReady = false;
};

struct TimedChCliSnapshot {
    std::string phaseName;
    uintptr_t addr = 0;
    std::array<uint8_t, 0x200> bytes = {};
};

struct Packet0017Group {
    unsigned long long firstElapsedMs = 0;
    unsigned long long lastElapsedMs = 0;
    std::vector<TimedPayload0017> entries;
};

struct PendingVtableDump {
    std::string phaseName;
    std::string archiveKey;
    std::string hotkeyName;
    unsigned long long packetTickMs = 0;
    unsigned long long dueTickMs = 0;
    uint32_t mappedSlot = UINT32_MAX;
    SkillbarSnapshot baselineSnapshot = {};
};

struct PendingDebugAssistSnapshot {
    std::string phaseName;
    std::string archiveKey;
    std::string hotkeyName;
    unsigned long long packetTickMs = 0;
    unsigned long long dueTickMs = 0;
    unsigned long long runId = 0;
};

constexpr bool kEnableVerboseVtableDump = false;

std::atomic<bool> g_running = false;
std::thread g_worker;
std::mutex g_stateMutex;
std::string g_statusLine = "Skill cooldown tracker idle.";
std::vector<TrackedSkillState> g_skillStates;
unsigned long long g_lastProcessedPacketTickMs = 0;
std::deque<PendingVtableDump> g_pendingVtableDumps;
std::deque<PendingDebugAssistSnapshot> g_pendingDebugAssistSnapshots;
DebugAssistState g_debugAssistState = {true, true, true, false, "C", "Debug Assist auto mode ready. Press one tracked hotkey once.", "No debug snapshot captured yet."};
CooldownPacketWatch g_cooldownPacketWatch = {};
std::vector<Packet0017Group> g_completed0017Groups;
std::optional<Packet0017Group> g_open0017Group;
std::vector<TimedChCliSnapshot> g_chCliSnapshots;
unsigned long long g_nextCooldownRunId = 1;
std::vector<TimedZStateSnapshot> g_recentZStateSnapshots;
unsigned long long g_lastCooldownWatchStartTickMs = 0;
std::string g_lastCooldownWatchHotkey;
std::string g_lastCooldownWatchArchiveKey;
std::unordered_map<uintptr_t, uint64_t> g_lastWatchValues;
InternalPollingWatcher g_internalPollingWatcher = {};
std::unordered_map<uint32_t, CooldownInfoLike> g_cooldownInfoBySlot;
EstimatedCooldownState g_estimatedCooldownState = {};

std::string GetDllDirectory() {
    char path[MAX_PATH] = {};
    if (!GetModuleFileNameA(dll_handle, path, MAX_PATH)) return ".";
    std::string p = path;
    size_t slash = p.find_last_of("\\/");
    return slash == std::string::npos ? "." : p.substr(0, slash);
}

std::string GetCurrentDateFileName() {
    SYSTEMTIME st = {};
    GetLocalTime(&st);
    char fileName[32];
    snprintf(fileName, sizeof(fileName), "%04u%02u%02u.log", st.wYear, st.wMonth, st.wDay);
    return fileName;
}

std::string GetCurrentTimestampForFileName() {
    SYSTEMTIME st = {};
    GetLocalTime(&st);
    char fileName[48];
    snprintf(fileName, sizeof(fileName), "%04u%02u%02u_%02u%02u%02u.log",
             st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return fileName;
}

std::string BuildTimestampForLog() {
    SYSTEMTIME st = {};
    GetLocalTime(&st);
    char ts[32];
    snprintf(ts, sizeof(ts), "%02u:%02u:%02u.%03u", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    return ts;
}

void AppendLogLine(const std::string& line) {
    const std::string dir = GetDllDirectory() + "\\cd-log";
    CreateDirectoryA(dir.c_str(), nullptr);
    std::ofstream out(dir + "\\" + GetCurrentDateFileName(), std::ios::app);
    if (out) out << line << "\n";
}

std::string& RuntimeDiagFilePath() {
    static std::string s_runtimeDiagPath;
    return s_runtimeDiagPath;
}

void EnsureRuntimeDiagFilePath() {
    std::string& path = RuntimeDiagFilePath();
    if (!path.empty()) {
        return;
    }
    const std::string dir = GetDllDirectory() + "\\runtime-log";
    CreateDirectoryA(dir.c_str(), nullptr);
    path = dir + "\\" + GetCurrentTimestampForFileName();
}

void AppendRuntimeDiagLine(const std::string& line) {
    EnsureRuntimeDiagFilePath();
    std::ofstream out(RuntimeDiagFilePath(), std::ios::app);
    if (out) {
        out << line << "\n";
    }
}

bool IsBridgeOnlyModeEnabled() {
    // Bridge-only mode is deprecated; always operate in internal watcher mode.
    return false;
}

void LogRuntimeDiagnostics(unsigned long long now,
                           bool liveSnapshotNeeded,
                           bool liveSnapshotExecuted,
                           unsigned long long nextLiveRefreshTickMs,
                           unsigned long sleepMs) {
    EnsureRuntimeDiagFilePath();

    PROCESS_MEMORY_COUNTERS_EX pmc = {};
    SIZE_T workingSetMb = 0;
    SIZE_T privateMb = 0;
    if (GetProcessMemoryInfo(GetCurrentProcess(),
                             reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc),
                             sizeof(pmc))) {
        workingSetMb = pmc.WorkingSetSize / (1024 * 1024);
        privateMb = pmc.PrivateUsage / (1024 * 1024);
    }

    std::ostringstream oss;
    oss << "[" << BuildTimestampForLog() << "] [RUNTIME ]"
        << " tick=" << now
        << " cooldown_active=" << (g_cooldownPacketWatch.active ? 1 : 0)
        << " run_id=" << g_cooldownPacketWatch.runId
        << " logged_ready=" << (g_cooldownPacketWatch.loggedReadyReturn ? 1 : 0)
        << " pending_debug=" << g_pendingDebugAssistSnapshots.size()
        << " pending_vtable=" << g_pendingVtableDumps.size()
        << " packets_0017=" << g_cooldownPacketWatch.all0017Packets.size()
        << " packets_0028=" << g_cooldownPacketWatch.all0028Packets.size()
        << " packets_candidate=" << g_cooldownPacketWatch.candidateSmsgs.size()
        << " open_group=" << (g_open0017Group.has_value() ? 1 : 0)
        << " completed_groups=" << g_completed0017Groups.size()
        << " live_needed=" << (liveSnapshotNeeded ? 1 : 0)
        << " live_executed=" << (liveSnapshotExecuted ? 1 : 0)
        << " next_live_due_in_ms=" << (nextLiveRefreshTickMs > now ? (nextLiveRefreshTickMs - now) : 0)
        << " sleep_ms=" << sleepMs
        << " ws_mb=" << workingSetMb
        << " private_mb=" << privateMb;
    AppendRuntimeDiagLine(oss.str());
}

bool ReadQwordSafe(uintptr_t addr, uint64_t& out) {
    out = 0;
    if (!addr) {
        return false;
    }
    __try {
        out = *reinterpret_cast<const uint64_t*>(addr);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        out = 0;
        return false;
    }
}

uintptr_t UntagNodePointer(uint64_t value) {
    return static_cast<uintptr_t>(value) & ~static_cast<uintptr_t>(1);
}

std::string FormatQwordField(uintptr_t addr) {
    uint64_t value = 0;
    std::ostringstream oss;
    oss << "[0x" << std::hex << addr << "]=";
    if (ReadQwordSafe(addr, value)) {
        oss << "0x" << value;
    } else {
        oss << "<invalid>";
    }
    return oss.str();
}

void LogNodeReadyStructureValidation(const char* phaseName,
                                     const PacketUseEvent& e,
                                     uintptr_t inferredBase,
                                     uintptr_t ptr48Untagged,
                                     uintptr_t ptr60Addr) {
    if (!phaseName || !inferredBase) {
        return;
    }

    uint64_t base40Raw = 0;
    uint64_t base60Raw = 0;
    uint64_t ptr48Plus20Raw = 0;
    uint64_t ptr60Plus20Raw = 0;
    const bool haveBase40 = ReadQwordSafe(inferredBase + 0x40, base40Raw);
    const bool haveBase60 = ReadQwordSafe(inferredBase + 0x60, base60Raw);
    const bool havePtr48Plus20 = ptr48Untagged && ReadQwordSafe(ptr48Untagged + 0x20, ptr48Plus20Raw);
    const bool havePtr60Plus20 = ptr60Addr && ReadQwordSafe(ptr60Addr + 0x20, ptr60Plus20Raw);

    const uintptr_t base40Node = UntagNodePointer(base40Raw);
    const uintptr_t base60Node = UntagNodePointer(base60Raw);
    const uintptr_t ptr48Plus20Node = UntagNodePointer(ptr48Plus20Raw);

    const bool base40SelfRef = haveBase40 && base40Node == (inferredBase + 0x40);
    const bool base60SelfRef = haveBase60 && base60Node == (inferredBase + 0x60);
    const bool ptr48Plus20SelfRef = havePtr48Plus20 && ptr48Plus20Node == ptr60Addr;
    const bool ptr60Plus20Zero = havePtr60Plus20 && ptr60Plus20Raw == 0;

    std::ostringstream oss;
    oss << "[" << BuildTimestampForLog() << "] [NODEVAL ] "
        << "run_id=" << e.runId
        << " hotkey=" << e.hotkeyName
        << " phase=" << phaseName
        << " base40_self_ref=" << (base40SelfRef ? 1 : 0)
        << " base60_self_ref=" << (base60SelfRef ? 1 : 0)
        << " ptr48p20_self_ref=" << (ptr48Plus20SelfRef ? 1 : 0)
        << " ptr60p20_zero=" << (ptr60Plus20Zero ? 1 : 0)
        << " base40_node=0x" << std::hex << base40Node
        << " base60_node=0x" << base60Node
        << " ptr48p20_node=0x" << ptr48Plus20Node
        << " ptr60p20_raw=0x" << ptr60Plus20Raw;
    AppendLogLine(oss.str());
}

uint16_t ReadLe16FromVector(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<uint16_t>(data[offset]) |
           (static_cast<uint16_t>(data[offset + 1]) << 8);
}

uint32_t ReadLe32FromVector(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<uint32_t>(data[offset]) |
           (static_cast<uint32_t>(data[offset + 1]) << 8) |
           (static_cast<uint32_t>(data[offset + 2]) << 16) |
           (static_cast<uint32_t>(data[offset + 3]) << 24);
}

std::string FormatElapsedSeconds(unsigned long long elapsedMs) {
    std::ostringstream oss;
    oss << "+" << std::fixed << std::setprecision(3)
        << (static_cast<double>(elapsedMs) / 1000.0) << "s";
    return oss.str();
}

const char* PtrWriteHitKindToString(PtrWriteHitKind kind) {
    switch (kind) {
    case PtrWriteHitKind::SLOT:
        return "SLOT";
    case PtrWriteHitKind::PTR40:
        return "PTR40";
    case PtrWriteHitKind::PTR48:
        return "PTR48";
    case PtrWriteHitKind::PTR60:
        return "PTR60";
    case PtrWriteHitKind::N48:
        return "N48";
    default:
        return "UNKNOWN";
    }
}

std::string FormatBridgePhase(uint32_t phase) {
    switch (phase) {
    case 1:
        return "t0";
    case 2:
        return "node_cd";
    case 3:
        return "node_ready";
    case 0:
        return "0";
    default:
        return std::to_string(phase);
    }
}

TrackedSkillState* FindStateByHotkey(const std::string& hotkey);

void RememberWatchValue(uintptr_t addr, uint64_t value) {
    if (addr) {
        g_lastWatchValues[addr] = value;
    }
}

uint32_t CurrentInternalPollPhaseCodeLocked() {
    if (g_internalPollingWatcher.readyToCooldownObserved &&
        g_internalPollingWatcher.durationMs > 0) {
        const unsigned long long now = GetTickCount64();
        if (now >= g_internalPollingWatcher.cdStartTickMs +
                   static_cast<unsigned long long>(g_internalPollingWatcher.durationMs)) {
            return 3;
        }
    }
    if (g_cooldownPacketWatch.loggedReadyReturn) {
        return 3;
    }
    return 2;
}

bool IsReadyBaselineStateForWatcherLocked(const InternalPollingWatcher& watcher) {
    if (watcher.hotkeyName.empty()) {
        return false;
    }
    if (auto* state = FindStateByHotkey(watcher.hotkeyName)) {
        return state->nodeState.stateText == "READY_BASELINE";
    }
    return false;
}

CooldownInfoLike& GetOrCreateCooldownInfoForWatcherLocked(const InternalPollingWatcher& watcher) {
    CooldownInfoLike& info = g_cooldownInfoBySlot[watcher.slot];
    info.slot = watcher.slot;
    if (!watcher.hotkeyName.empty()) {
        info.hotkey = watcher.hotkeyName;
    }
    info.skillId = watcher.skillId;
    if (watcher.baselinePtr40 != 0) {
        info.baselinePtr40 = watcher.baselinePtr40;
    }
    if (watcher.baselinePtr60 != 0) {
        info.baselinePtr60 = watcher.baselinePtr60;
    }
    if (watcher.durationMs > 0) {
        info.durationMs = watcher.durationMs;
    }
    return info;
}

int ResolveEstimatedDurationMsForHotkey(const std::string& hotkey) {
    const auto& table = kx::SkillDb::GetSkillTable();
    for (const auto& cfg : table) {
        if (cfg.hotkeyName != hotkey) {
            continue;
        }
        if (cfg.ammoRechargeMs > 0) {
            return cfg.ammoRechargeMs;
        }
        if (cfg.rechargeMs > 0) {
            return cfg.rechargeMs;
        }
        return 0;
    }
    return 0;
}

void RecordPtrWriteHitLocked(PtrWriteHitKind kind,
                             uintptr_t hitAddr,
                             bool haveOldValue,
                             uint64_t oldValue,
                             uint64_t newValue,
                             uintptr_t rip,
                             uint32_t slot,
                             uint32_t phase,
                             const char* source) {
    if (!g_cooldownPacketWatch.active) {
        return;
    }

    const unsigned long long now = GetTickCount64();
    const unsigned long long elapsedMs =
        now > g_cooldownPacketWatch.startedTickMs ? (now - g_cooldownPacketWatch.startedTickMs) : 0;

    RememberWatchValue(hitAddr, newValue);
    g_cooldownPacketWatch.ptrWriteHits.push_back({
        elapsedMs,
        kind,
        hitAddr,
        haveOldValue,
        oldValue,
        newValue,
        rip,
        slot,
        phase,
        source ? source : ""
    });
    if (g_cooldownPacketWatch.ptrWriteHits.size() > kMaxPtrWriteHits) {
        g_cooldownPacketWatch.ptrWriteHits.erase(g_cooldownPacketWatch.ptrWriteHits.begin());
    }
}

std::string BuildPtrHitLabel(const PtrWriteHitRecord& hit) {
    std::ostringstream oss;
    oss << PtrWriteHitKindToString(hit.kind)
        << " hit_addr=0x" << std::hex << hit.hitAddr
        << " old_value=";
    if (hit.haveOldValue) {
        oss << "0x" << hit.oldValue;
    } else {
        oss << "<unknown>";
    }
    oss << " new_value=0x" << std::hex << hit.newValue
        << " rip=0x" << std::hex << hit.rip << std::dec
        << " slot=";
    if (hit.slot == UINT32_MAX) {
        oss << "na";
    } else {
        oss << hit.slot;
    }
    oss << " phase=" << FormatBridgePhase(hit.phase);
    if (!hit.source.empty()) {
        oss << " source=" << hit.source;
    }
    return oss.str();
}

std::optional<unsigned long long> GetNodeReadyAnchorMsLocked() {
    if (g_cooldownPacketWatch.haveNodeReadyOffset) {
        return g_cooldownPacketWatch.nodeReadyOffsetMs;
    }
    return std::nullopt;
}

std::optional<long long> ComputeDeltaToReadyMs(unsigned long long elapsedMs) {
    const auto anchorMs = GetNodeReadyAnchorMsLocked();
    if (!anchorMs.has_value()) {
        return std::nullopt;
    }
    return static_cast<long long>(elapsedMs) -
           static_cast<long long>(anchorMs.value());
}

std::string ComputeStageTagFromDeltaToReady(long long deltaToReadyMs) {
    if (deltaToReadyMs < -15000) {
        return "far_early";
    }
    if (deltaToReadyMs < -5000) {
        return "early";
    }
    if (deltaToReadyMs < -1000) {
        return "late";
    }
    if (deltaToReadyMs <= 1000) {
        return "near_ready";
    }
    return "post_ready";
}

std::string BuildHexPreview(const std::vector<uint8_t>& bytes, size_t maxBytes = 16) {
    if (bytes.empty()) {
        return "<empty>";
    }

    std::ostringstream oss;
    const size_t count = std::min(bytes.size(), maxBytes);
    for (size_t i = 0; i < count; ++i) {
        if (i != 0) {
            oss << '.';
        }
        oss << std::hex << std::nouppercase << std::setw(2) << std::setfill('0')
            << static_cast<unsigned>(bytes[i]);
    }
    if (bytes.size() > maxBytes) {
        oss << "...";
    }
    return oss.str();
}

std::string BuildHexFull(const std::vector<uint8_t>& bytes) {
    if (bytes.empty()) {
        return "<empty>";
    }
    std::ostringstream oss;
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (i != 0) {
            oss << ".";
        }
        oss << std::hex << std::nouppercase << std::setw(2) << std::setfill('0')
            << static_cast<unsigned>(bytes[i]);
    }
    return oss.str();
}

std::string BuildU8FieldView(const std::vector<uint8_t>& payload, size_t from, size_t to) {
    std::ostringstream oss;
    oss << "u8[" << from << ".." << (to > 0 ? to - 1 : 0) << "]=";
    bool wrote = false;
    for (size_t i = from; i < to && i < payload.size(); ++i) {
        if (wrote) {
            oss << ' ';
        }
        oss << std::hex << std::nouppercase << std::setw(2) << std::setfill('0')
            << static_cast<unsigned>(payload[i]);
        wrote = true;
    }
    if (!wrote) {
        oss << "<none>";
    }
    return oss.str();
}

std::string BuildU16FieldView(const std::vector<uint8_t>& payload, size_t from, size_t to) {
    std::ostringstream oss;
    oss << "u16[" << from << ".." << (to > 0 ? to - 1 : 0) << "]=";
    bool wrote = false;
    for (size_t i = from; i + 1 < to && i + 1 < payload.size(); i += 2) {
        if (wrote) {
            oss << ' ';
        }
        oss << std::hex << std::nouppercase << std::setw(4) << std::setfill('0')
            << ReadLe16FromVector(payload, i);
        wrote = true;
    }
    if (!wrote) {
        oss << "<none>";
    }
    return oss.str();
}

std::string BuildU32FieldView(const std::vector<uint8_t>& payload, size_t from, size_t to) {
    std::ostringstream oss;
    oss << "u32[" << from << ".." << (to > 0 ? to - 1 : 0) << "]=";
    bool wrote = false;
    for (size_t i = from; i + 3 < to && i + 3 < payload.size(); i += 4) {
        if (wrote) {
            oss << ' ';
        }
        oss << std::hex << std::nouppercase << std::setw(8) << std::setfill('0')
            << ReadLe32FromVector(payload, i);
        wrote = true;
    }
    if (!wrote) {
        oss << "<none>";
    }
    return oss.str();
}

void FlushOpen0017GroupLocked();

void LogNodeAnchorLocked(const char* tag, const char* phaseName, unsigned long long offsetMs, const PacketUseEvent& e) {
    std::ostringstream oss;
    oss << "[" << BuildTimestampForLog() << "] [CDHUNT  ] "
        << tag
        << " hotkey=" << e.hotkeyName
        << " archive_key=" << e.archiveKey
        << " offset=" << FormatElapsedSeconds(offsetMs);
    if (phaseName && phaseName[0] != '\0') {
        oss << " phase=" << phaseName;
    }
    AppendLogLine(oss.str());
}

void DumpCooldownTimelineLocked() {
    if (!g_cooldownPacketWatch.active) {
        return;
    }

    std::ostringstream oss;
    oss << "[" << BuildTimestampForLog() << "] [CDHUNT  ] === TIMELINE ==="
        << " +0.000s SKILL_PRESS";
    AppendLogLine(oss.str());

    for (const auto& event : g_cooldownPacketWatch.groupEvents) {
        std::ostringstream line;
        line << "[" << BuildTimestampForLog() << "] [CDHUNT  ]   "
             << FormatElapsedSeconds(event.first) << "  " << event.second;
        AppendLogLine(line.str());
    }

    if (g_cooldownPacketWatch.haveNodeCdOffset) {
        std::ostringstream line;
        line << "[" << BuildTimestampForLog() << "] [CDHUNT  ]   "
             << FormatElapsedSeconds(g_cooldownPacketWatch.nodeCdOffsetMs) << "  NODE_CD";
        AppendLogLine(line.str());
    }
    if (const auto anchorMs = GetNodeReadyAnchorMsLocked(); anchorMs.has_value()) {
        std::ostringstream line;
        line << "[" << BuildTimestampForLog() << "] [CDHUNT  ]   "
             << FormatElapsedSeconds(anchorMs.value()) << "  NODE_READY_ANCHOR";
        AppendLogLine(line.str());
    }
}

std::string ComputePhaseTagFor0028(unsigned long long elapsedMs) {
    const auto deltaToReady = ComputeDeltaToReadyMs(elapsedMs);
    if (!deltaToReady.has_value()) {
        return "unknown";
    }
    return ComputeStageTagFromDeltaToReady(deltaToReady.value());
}

bool ShouldTrackCandidateSmsg(const PacketInfo& packet) {
    if (!kEnableCandidateSmsgTracking) {
        return false;
    }

    // Current packet-side mainline:
    // - 0x0001 / SMSG_AGENT_UPDATE_BATCH remains the strongest container candidate.
    // - 0x0015 / SMSG_PLAYER_DATA_UPDATE is the secondary candidate worth
    //   preserving in full and analyzing field-by-field.
    if (packet.rawHeaderId != 0x0001 && packet.rawHeaderId != 0x0015) {
        return false;
    }

    if (packet.data.empty() || packet.data.size() > kMaxCandidateSmsgPayloadBytes) {
        return false;
    }

    return packet.data.size() >= 4;
}

std::string BuildCandidateSubtypeKey(const CandidateSmsgRecord& pkt) {
    std::ostringstream oss;
    oss << "h=0x" << std::hex << std::setw(4) << std::setfill('0') << pkt.rawHeaderId << std::dec;
    if (!pkt.payload.empty()) {
        oss << " b0=0x" << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<unsigned>(pkt.payload[0]) << std::dec;
    }
    if (pkt.payload.size() > 1) {
        oss << " b1=0x" << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<unsigned>(pkt.payload[1]) << std::dec;
    }
    oss << " size=" << pkt.payload.size();
    return oss.str();
}

std::string BuildCandidateFieldView(const std::vector<uint8_t>& payload) {
    std::ostringstream oss;
    oss << BuildU8FieldView(payload, 0, payload.size())
        << " "
        << BuildU16FieldView(payload, 0, payload.size())
        << " "
        << BuildU32FieldView(payload, 0, payload.size());
    return oss.str();
}

void DumpCandidateSmsgSummaryLocked() {
    if (!g_cooldownPacketWatch.active) {
        return;
    }
    if (IsBridgeOnlyModeEnabled()) {
        return;
    }

    AppendLogLine("[" + BuildTimestampForLog() + "] [CDSMSG  ] === CANDIDATE SMSG TABLE === run_id=" +
                  std::to_string(g_cooldownPacketWatch.runId));
    if (g_cooldownPacketWatch.candidateSmsgs.empty()) {
        AppendLogLine("[" + BuildTimestampForLog() + "] [CDSMSG  ] run_id=" +
                      std::to_string(g_cooldownPacketWatch.runId) + " <none>");
        return;
    }

    struct HeaderSummary {
        size_t count = 0;
        unsigned long long firstElapsedMs = 0;
        unsigned long long lastElapsedMs = 0;
        std::set<size_t> sizes;
    };
    struct SubtypeSummary {
        size_t count = 0;
        unsigned long long firstElapsedMs = 0;
        unsigned long long lastElapsedMs = 0;
    };

    std::map<uint16_t, HeaderSummary> summaryByHeader;
    std::map<std::string, SubtypeSummary> summaryBySubtype;
    for (const auto& pkt : g_cooldownPacketWatch.candidateSmsgs) {
        if constexpr (kEnableCandidateSmsgDetailedLog) {
            const auto deltaToReady = ComputeDeltaToReadyMs(pkt.elapsedMs);
            std::ostringstream line;
            line << "[" << BuildTimestampForLog() << "] [CDSMSG  ] "
                 << "run_id=" << g_cooldownPacketWatch.runId
                 << " elapsed_ms=" << pkt.elapsedMs
                 << " header=0x" << std::hex << std::setw(4) << std::setfill('0') << pkt.rawHeaderId
                 << std::dec
                 << " size=" << pkt.payload.size()
                 << " name=" << (pkt.name.empty() ? "<unnamed>" : pkt.name)
                 << " subtype=" << BuildCandidateSubtypeKey(pkt)
                 << " payload_hex=" << BuildHexFull(pkt.payload)
                 << " stage_tag="
                 << (deltaToReady.has_value() ? ComputeStageTagFromDeltaToReady(deltaToReady.value()) : "unknown");
            if (deltaToReady.has_value()) {
                line << " delta_to_ready_ms=" << deltaToReady.value();
            } else {
                line << " delta_to_ready_ms=<unknown>";
            }
            if (pkt.rawHeaderId == 0x0015) {
                line << " field_view={" << BuildCandidateFieldView(pkt.payload) << "}";
            }
            AppendLogLine(line.str());
        }

        auto& summary = summaryByHeader[pkt.rawHeaderId];
        if (summary.count == 0) {
            summary.firstElapsedMs = pkt.elapsedMs;
        }
        ++summary.count;
        summary.lastElapsedMs = pkt.elapsedMs;
        summary.sizes.insert(pkt.payload.size());

        const std::string subtypeKey = BuildCandidateSubtypeKey(pkt);
        auto& subtype = summaryBySubtype[subtypeKey];
        if (subtype.count == 0) {
            subtype.firstElapsedMs = pkt.elapsedMs;
        }
        ++subtype.count;
        subtype.lastElapsedMs = pkt.elapsedMs;
    }

    AppendLogLine("[" + BuildTimestampForLog() + "] [CDSMSG  ] === CANDIDATE SMSG SUMMARY === run_id=" +
                  std::to_string(g_cooldownPacketWatch.runId));
    for (const auto& [headerId, summary] : summaryByHeader) {
        std::ostringstream sizeList;
        bool first = true;
        for (const size_t size : summary.sizes) {
            if (!first) {
                sizeList << ",";
            }
            first = false;
            sizeList << size;
        }

        std::ostringstream line;
        line << "[" << BuildTimestampForLog() << "] [CDSMSG  ] "
             << "run_id=" << g_cooldownPacketWatch.runId
             << " header=0x" << std::hex << std::setw(4) << std::setfill('0') << headerId
             << std::dec
             << " count=" << summary.count
             << " first_elapsed_ms=" << summary.firstElapsedMs
             << " last_elapsed_ms=" << summary.lastElapsedMs
             << " sizes=" << sizeList.str();
        AppendLogLine(line.str());
    }

    AppendLogLine("[" + BuildTimestampForLog() + "] [CDSMSG  ] === CANDIDATE SMSG SUBTYPES === run_id=" +
                  std::to_string(g_cooldownPacketWatch.runId));
    for (const auto& [subtypeKey, summary] : summaryBySubtype) {
        std::ostringstream line;
        line << "[" << BuildTimestampForLog() << "] [CDSMSG  ] "
             << "run_id=" << g_cooldownPacketWatch.runId
             << " subtype=" << subtypeKey
             << " count=" << summary.count
             << " first_elapsed_ms=" << summary.firstElapsedMs
             << " last_elapsed_ms=" << summary.lastElapsedMs;
        AppendLogLine(line.str());
    }
}

std::string BuildZStateShapeKey(const TimedZStateSnapshot& snapshot) {
    std::ostringstream oss;
    oss << "state=" << snapshot.stateText
        << " cur40=0x" << std::hex << snapshot.cur40
        << " cur60=0x" << snapshot.cur60
        << " ptr48p20=0x" << snapshot.ptr48Plus20;
    return oss.str();
}

bool SameZStateShape(const TimedZStateSnapshot& lhs, const TimedZStateSnapshot& rhs) {
    return lhs.stateText == rhs.stateText &&
           lhs.cur40 == rhs.cur40 &&
           lhs.cur60 == rhs.cur60 &&
           lhs.ptr48Plus20 == rhs.ptr48Plus20;
}

std::string Build0017GroupSizeList(const Packet0017Group& group) {
    std::vector<size_t> sizes;
    for (const auto& entry : group.entries) {
        const size_t currentSize = entry.payload.size();
        if (std::find(sizes.begin(), sizes.end(), currentSize) == sizes.end()) {
            sizes.push_back(currentSize);
        }
    }
    std::sort(sizes.begin(), sizes.end());

    std::ostringstream oss;
    for (size_t i = 0; i < sizes.size(); ++i) {
        if (i != 0) {
            oss << ",";
        }
        oss << sizes[i];
    }
    return oss.str();
}

std::string BuildNearby0017GroupsForZLocked(unsigned long long centerElapsedMs, unsigned long long windowMs) {
    std::ostringstream oss;
    bool found = false;
    for (size_t i = 0; i < g_completed0017Groups.size(); ++i) {
        const auto& group = g_completed0017Groups[i];
        const long long deltaMs = static_cast<long long>(group.firstElapsedMs) -
                                  static_cast<long long>(centerElapsedMs);
        const long long absDeltaMs = deltaMs >= 0 ? deltaMs : -deltaMs;
        if (absDeltaMs > static_cast<long long>(windowMs)) {
            continue;
        }
        if (found) {
            oss << " | ";
        }
        oss << "#" << (i + 1)
            << "@"
            << (deltaMs >= 0 ? "+" : "") << deltaMs << "ms"
            << " sizes=" << Build0017GroupSizeList(group);
        found = true;
    }
    return found ? oss.str() : std::string("<none>");
}

std::string BuildNearby0028ForZLocked(unsigned long long centerElapsedMs, unsigned long long windowMs) {
    std::ostringstream oss;
    bool found = false;
    for (const auto& pkt : g_cooldownPacketWatch.all0028Packets) {
        const long long deltaMs = static_cast<long long>(pkt.elapsedMs) -
                                  static_cast<long long>(centerElapsedMs);
        const long long absDeltaMs = deltaMs >= 0 ? deltaMs : -deltaMs;
        if (absDeltaMs > static_cast<long long>(windowMs)) {
            continue;
        }
        if (found) {
            oss << " | ";
        }
        oss << "@"
            << (deltaMs >= 0 ? "+" : "") << deltaMs << "ms"
            << " size=" << pkt.payload.size()
            << " hex=" << BuildHexFull(pkt.payload);
        found = true;
    }
    return found ? oss.str() : std::string("<none>");
}

void DumpZPacketMemoryTimelineLocked() {
    if (!g_cooldownPacketWatch.active ||
        g_cooldownPacketWatch.hotkeyName != "Z" ||
        g_cooldownPacketWatch.zStateSnapshots.empty()) {
        return;
    }

    struct TimelineEvent {
        unsigned long long offsetMs = 0;
        std::string kind;
        std::string label;
    };

    std::vector<TimelineEvent> events;
    events.push_back({0, "PRESS", "SKILL_PRESS"});
    for (const auto& snapshot : g_cooldownPacketWatch.zStateSnapshots) {
        std::ostringstream label;
        label << "ZMEM phase=" << snapshot.phaseName
              << " old=" << snapshot.oldStateText
              << " new=" << snapshot.stateText
              << " " << BuildZStateShapeKey(snapshot);
        events.push_back({snapshot.elapsedMs, "ZMEM", label.str()});
    }
    for (size_t i = 0; i < g_completed0017Groups.size(); ++i) {
        const auto& group = g_completed0017Groups[i];
        std::ostringstream label;
        label << "0x0017_group#" << (i + 1)
              << " sizes=" << Build0017GroupSizeList(group);
        events.push_back({group.firstElapsedMs, "0017", label.str()});
    }
    for (const auto& pkt : g_cooldownPacketWatch.all0028Packets) {
        std::ostringstream label;
        label << "0x0028 size=" << pkt.payload.size()
              << " hex=" << BuildHexFull(pkt.payload);
        events.push_back({pkt.elapsedMs, "0028", label.str()});
    }
    if (g_cooldownPacketWatch.haveNodeCdOffset) {
        events.push_back({g_cooldownPacketWatch.nodeCdOffsetMs, "NODE", "NODE_CD_START"});
    }
    if (g_cooldownPacketWatch.haveNodeReadyBaselineOffset) {
        events.push_back({g_cooldownPacketWatch.nodeReadyBaselineOffsetMs, "NODE", "NODE_READY_BASELINE"});
    }
    if (g_cooldownPacketWatch.haveNodeReadyOffset) {
        events.push_back({g_cooldownPacketWatch.nodeReadyOffsetMs, "NODE", "NODE_READY"});
    }

    std::sort(events.begin(), events.end(), [](const TimelineEvent& lhs, const TimelineEvent& rhs) {
        if (lhs.offsetMs != rhs.offsetMs) {
            return lhs.offsetMs < rhs.offsetMs;
        }
        if (lhs.kind != rhs.kind) {
            return lhs.kind < rhs.kind;
        }
        return lhs.label < rhs.label;
    });

    AppendLogLine("[" + BuildTimestampForLog() + "] [ZCORR   ] === Z RUN TIMELINE === run_id=" +
                  std::to_string(g_cooldownPacketWatch.runId));
    for (const auto& event : events) {
        std::ostringstream line;
        line << "[" << BuildTimestampForLog() << "] [ZCORR   ] "
             << "run_id=" << g_cooldownPacketWatch.runId
             << " offset=" << FormatElapsedSeconds(event.offsetMs)
             << " " << event.label;
        AppendLogLine(line.str());
    }
}

void DumpZStateSwitchesLocked() {
    if (!g_cooldownPacketWatch.active ||
        g_cooldownPacketWatch.hotkeyName != "Z" ||
        g_cooldownPacketWatch.zStateSnapshots.size() < 2) {
        return;
    }

    AppendLogLine("[" + BuildTimestampForLog() + "] [ZSWITCH ] === Z STATE SWITCHES === run_id=" +
                  std::to_string(g_cooldownPacketWatch.runId));
    for (size_t i = 1; i < g_cooldownPacketWatch.zStateSnapshots.size(); ++i) {
        const auto& before = g_cooldownPacketWatch.zStateSnapshots[i - 1];
        const auto& after = g_cooldownPacketWatch.zStateSnapshots[i];
        if (SameZStateShape(before, after)) {
            continue;
        }

        std::ostringstream switchLine;
        switchLine << "[" << BuildTimestampForLog() << "] [ZSWITCH ] "
                   << "run_id=" << g_cooldownPacketWatch.runId
                   << " offset=" << FormatElapsedSeconds(after.elapsedMs)
                   << " from_phase=" << before.phaseName
                   << " to_phase=" << after.phaseName
                   << " from={" << BuildZStateShapeKey(before) << "}"
                   << " to={" << BuildZStateShapeKey(after) << "}";
        AppendLogLine(switchLine.str());

        std::ostringstream pktLine;
        pktLine << "[" << BuildTimestampForLog() << "] [ZSWITCH ] "
                << "run_id=" << g_cooldownPacketWatch.runId
                << " offset=" << FormatElapsedSeconds(after.elapsedMs)
                << " nearby_0017=" << BuildNearby0017GroupsForZLocked(after.elapsedMs, 300);
        AppendLogLine(pktLine.str());

        std::ostringstream attrLine;
        attrLine << "[" << BuildTimestampForLog() << "] [ZSWITCH ] "
                 << "run_id=" << g_cooldownPacketWatch.runId
                 << " offset=" << FormatElapsedSeconds(after.elapsedMs)
                 << " nearby_0028=" << BuildNearby0028ForZLocked(after.elapsedMs, 300);
        AppendLogLine(attrLine.str());
    }
}

void DumpZClusterComparisonLocked() {
    if (!g_cooldownPacketWatch.active ||
        g_cooldownPacketWatch.hotkeyName != "Z" ||
        g_cooldownPacketWatch.zStateSnapshots.empty()) {
        return;
    }

    AppendLogLine("[" + BuildTimestampForLog() + "] [ZCLUSTER] === Z STATE CLUSTER CHECK === run_id=" +
                  std::to_string(g_cooldownPacketWatch.runId));
    for (const auto& snapshot : g_cooldownPacketWatch.zStateSnapshots) {
        if (snapshot.stateText == "UNKNOWN") {
            continue;
        }

        const TimedZStateSnapshot* matched = nullptr;
        for (const auto& previous : g_recentZStateSnapshots) {
            if (previous.runId == snapshot.runId) {
                continue;
            }
            if (!SameZStateShape(previous, snapshot)) {
                continue;
            }
            matched = &previous;
            break;
        }

        std::ostringstream line;
        line << "[" << BuildTimestampForLog() << "] [ZCLUSTER] "
             << "run_id=" << snapshot.runId
             << " phase=" << snapshot.phaseName
             << " offset=" << FormatElapsedSeconds(snapshot.elapsedMs)
             << " state=" << snapshot.stateText
             << " {" << BuildZStateShapeKey(snapshot) << "}";
        if (matched) {
            line << " same_cluster=1"
                 << " matched_run=" << matched->runId
                 << " matched_phase=" << matched->phaseName
                 << " matched_offset=" << FormatElapsedSeconds(matched->elapsedMs);
        } else {
            line << " same_cluster=0";
        }
        AppendLogLine(line.str());
    }
}

void DumpPacketCorrelationTimelineLocked() {
    if (!g_cooldownPacketWatch.active) {
        return;
    }
    if (IsBridgeOnlyModeEnabled()) {
        return;
    }

    struct TimelineEvent {
        unsigned long long offsetMs = 0;
        std::string label;
    };

    std::vector<TimelineEvent> events;
    events.push_back({0, "SKILL_PRESS"});
    if (g_cooldownPacketWatch.haveNodeCdOffset) {
        events.push_back({g_cooldownPacketWatch.nodeCdOffsetMs, "NODE_CD_START"});
    }
    for (const auto& pkt : g_cooldownPacketWatch.all0017Packets) {
        std::ostringstream label;
        label << "0x0017 size=" << pkt.payload.size()
              << " payload=" << BuildHexFull(pkt.payload);
        events.push_back({pkt.elapsedMs, label.str()});
    }
    for (const auto& pkt : g_cooldownPacketWatch.all0028Packets) {
        std::ostringstream label;
        label << "0x0028 size=" << pkt.payload.size()
              << " payload=" << BuildHexFull(pkt.payload);
        events.push_back({pkt.elapsedMs, label.str()});
    }
    if (g_cooldownPacketWatch.haveNodeReadyBaselineOffset) {
        events.push_back({g_cooldownPacketWatch.nodeReadyBaselineOffsetMs, "NODE_READY_BASELINE"});
    }
    if (g_cooldownPacketWatch.haveNodeReadyOffset) {
        events.push_back({g_cooldownPacketWatch.nodeReadyOffsetMs, "NODE_READY"});
    }

    std::sort(events.begin(), events.end(), [](const TimelineEvent& a, const TimelineEvent& b) {
        if (a.offsetMs != b.offsetMs) {
            return a.offsetMs < b.offsetMs;
        }
        return a.label < b.label;
    });

    AppendLogLine("[" + BuildTimestampForLog() + "] [CDCORR  ] === RUN TIMELINE === run_id=" + std::to_string(g_cooldownPacketWatch.runId));
    for (const auto& event : events) {
        std::ostringstream line;
        line << "[" << BuildTimestampForLog() << "] [CDCORR  ] "
             << "run_id=" << g_cooldownPacketWatch.runId
             << " offset=" << FormatElapsedSeconds(event.offsetMs)
             << " " << event.label;
        AppendLogLine(line.str());
    }
}

void DumpPtrPacketTimelineLocked() {
    if (!g_cooldownPacketWatch.active) {
        return;
    }
    if (IsBridgeOnlyModeEnabled()) {
        constexpr unsigned long long kCompactPacketNearMs = 500;

        struct CompactPacketEvent {
            unsigned long long offsetMs = 0;
            uint16_t header = 0;
            size_t size = 0;
            std::string nearLabel;
        };

        struct AnchorEvent {
            unsigned long long offsetMs = 0;
            std::string label;
        };

        std::vector<AnchorEvent> anchors;
        anchors.push_back({0, "SKILL_PRESS"});
        if (g_cooldownPacketWatch.haveNodeCdOffset) {
            anchors.push_back({g_cooldownPacketWatch.nodeCdOffsetMs, "NODE_CD_START"});
        }
        if (g_cooldownPacketWatch.haveNodeReadyOffset) {
            anchors.push_back({g_cooldownPacketWatch.nodeReadyOffsetMs, "NODE_READY"});
        }
        for (const auto& hit : g_cooldownPacketWatch.ptrWriteHits) {
            anchors.push_back({hit.elapsedMs, PtrWriteHitKindToString(hit.kind)});
        }

        auto nearestAnchorLabel = [&](unsigned long long packetOffset) -> std::optional<std::string> {
            std::optional<std::string> bestLabel;
            unsigned long long bestDelta = kCompactPacketNearMs + 1;
            for (const auto& anchor : anchors) {
                const unsigned long long delta =
                    packetOffset > anchor.offsetMs ? (packetOffset - anchor.offsetMs) : (anchor.offsetMs - packetOffset);
                if (delta <= kCompactPacketNearMs && delta < bestDelta) {
                    bestDelta = delta;
                    bestLabel = anchor.label;
                }
            }
            return bestLabel;
        };

        std::vector<CompactPacketEvent> packetEvents;
        auto appendIfNear = [&](unsigned long long offsetMs, uint16_t header, size_t size) {
            const auto nearLabel = nearestAnchorLabel(offsetMs);
            if (!nearLabel.has_value()) {
                return;
            }
            packetEvents.push_back({offsetMs, header, size, nearLabel.value()});
        };

        for (const auto& pkt : g_cooldownPacketWatch.all0028Packets) {
            appendIfNear(pkt.elapsedMs, pkt.rawHeaderId, pkt.payload.size());
        }
        for (const auto& pkt : g_cooldownPacketWatch.all0017Packets) {
            appendIfNear(pkt.elapsedMs, static_cast<uint16_t>(kx::SMSG_HeaderId::SKILL_UPDATE), pkt.payload.size());
        }

        std::sort(packetEvents.begin(), packetEvents.end(), [](const CompactPacketEvent& a, const CompactPacketEvent& b) {
            if (a.offsetMs != b.offsetMs) {
                return a.offsetMs < b.offsetMs;
            }
            if (a.header != b.header) {
                return a.header < b.header;
            }
            return a.size < b.size;
        });
        packetEvents.erase(
            std::unique(packetEvents.begin(), packetEvents.end(), [](const CompactPacketEvent& a, const CompactPacketEvent& b) {
                return a.offsetMs == b.offsetMs && a.header == b.header && a.size == b.size;
            }),
            packetEvents.end());

        AppendLogLine("[" + BuildTimestampForLog() + "] [PTRPKTL ] compact === PTR/PACKET TIMELINE === run_id=" +
                      std::to_string(g_cooldownPacketWatch.runId) +
                      " near_ms=" + std::to_string(kCompactPacketNearMs));
        AppendLogLine("[" + BuildTimestampForLog() + "] [PTRPKTL ] compact run_id=" +
                      std::to_string(g_cooldownPacketWatch.runId) + " offset=+0.000s SKILL_PRESS");
        for (const auto& event : packetEvents) {
            std::ostringstream line;
            line << "[" << BuildTimestampForLog() << "] [PTRPKTL ] compact "
                 << "run_id=" << g_cooldownPacketWatch.runId
                 << " offset=" << FormatElapsedSeconds(event.offsetMs)
                 << " near=" << event.nearLabel
                 << " 0x" << std::hex << std::setw(4) << std::setfill('0') << event.header
                 << std::dec << " size=" << event.size;
            AppendLogLine(line.str());
        }
        return;
    }

    struct TimelineEvent {
        unsigned long long offsetMs = 0;
        std::string label;
    };

    std::vector<TimelineEvent> events;
    events.push_back({0, "SKILL_PRESS"});

    for (const auto& hit : g_cooldownPacketWatch.ptrWriteHits) {
        events.push_back({hit.elapsedMs, BuildPtrHitLabel(hit)});
    }

    if (g_cooldownPacketWatch.haveNodeCdOffset) {
        events.push_back({g_cooldownPacketWatch.nodeCdOffsetMs, "NODE_CD_START"});
    }
    if (g_cooldownPacketWatch.haveNodeReadyOffset) {
        events.push_back({g_cooldownPacketWatch.nodeReadyOffsetMs, "NODE_READY"});
    }

    for (const auto& pkt : g_cooldownPacketWatch.all0017Packets) {
        std::ostringstream label;
        label << "0x0017 size=" << pkt.payload.size()
              << " payload=" << BuildHexFull(pkt.payload);
        events.push_back({pkt.elapsedMs, label.str()});
    }
    for (const auto& pkt : g_cooldownPacketWatch.all0028Packets) {
        std::ostringstream label;
        label << "0x0028 size=" << pkt.payload.size()
              << " payload=" << BuildHexFull(pkt.payload);
        if (pkt.payload.size() == 6) {
            label << " field0_u16=" << ReadLe16FromVector(pkt.payload, 0)
                  << " field1_u16=" << ReadLe16FromVector(pkt.payload, 2)
                  << " field2_u16=" << ReadLe16FromVector(pkt.payload, 4);
        }
        events.push_back({pkt.elapsedMs, label.str()});
    }

    std::sort(events.begin(), events.end(), [](const TimelineEvent& a, const TimelineEvent& b) {
        if (a.offsetMs != b.offsetMs) {
            return a.offsetMs < b.offsetMs;
        }
        return a.label < b.label;
    });

    AppendLogLine("[" + BuildTimestampForLog() + "] [PTRPKTL ] === PTR/PACKET TIMELINE === run_id=" +
                  std::to_string(g_cooldownPacketWatch.runId));
    for (const auto& event : events) {
        std::ostringstream line;
        line << "[" << BuildTimestampForLog() << "] [PTRPKTL ] "
             << "run_id=" << g_cooldownPacketWatch.runId
             << " offset=" << FormatElapsedSeconds(event.offsetMs)
             << " " << event.label;
        AppendLogLine(line.str());
    }
}

void DumpPtrPacketComparisonLocked() {
    if (!g_cooldownPacketWatch.active) {
        return;
    }

    AppendLogLine("[" + BuildTimestampForLog() + "] [PTRCMP  ] === PTR WINDOW COMPARISON === run_id=" +
                  std::to_string(g_cooldownPacketWatch.runId) +
                  " window_ms=" + std::to_string(kPtrPacketWindowMs));

    const size_t ptr40InternalHits = std::count_if(g_cooldownPacketWatch.ptrWriteHits.begin(),
                                                   g_cooldownPacketWatch.ptrWriteHits.end(),
                                                   [](const PtrWriteHitRecord& hit) {
        return hit.kind == PtrWriteHitKind::PTR40 && hit.source == "internal_poll";
                                                   });
    const size_t ptr60InternalHits = std::count_if(g_cooldownPacketWatch.ptrWriteHits.begin(),
                                                   g_cooldownPacketWatch.ptrWriteHits.end(),
                                                   [](const PtrWriteHitRecord& hit) {
        return hit.kind == PtrWriteHitKind::PTR60 && hit.source == "internal_poll";
                                                   });
    const size_t n48InternalHits = std::count_if(g_cooldownPacketWatch.ptrWriteHits.begin(),
                                                 g_cooldownPacketWatch.ptrWriteHits.end(),
                                                 [](const PtrWriteHitRecord& hit) {
        return hit.kind == PtrWriteHitKind::N48 && hit.source == "internal_poll";
                                                 });
    const bool ptr40Changed = std::any_of(g_cooldownPacketWatch.ptrWriteHits.begin(),
                                          g_cooldownPacketWatch.ptrWriteHits.end(),
                                          [](const PtrWriteHitRecord& hit) {
        return hit.kind == PtrWriteHitKind::PTR40 && hit.source == "internal_poll" &&
               hit.haveOldValue && hit.oldValue != hit.newValue;
                                          });
    const bool ptr60Changed = std::any_of(g_cooldownPacketWatch.ptrWriteHits.begin(),
                                          g_cooldownPacketWatch.ptrWriteHits.end(),
                                          [](const PtrWriteHitRecord& hit) {
        return hit.kind == PtrWriteHitKind::PTR60 && hit.source == "internal_poll" &&
               hit.haveOldValue && hit.oldValue != hit.newValue;
                                          });
    {
        std::ostringstream fields;
        fields << "[" << BuildTimestampForLog() << "] [PTRCMP  ] "
               << "run_id=" << g_cooldownPacketWatch.runId
               << " ptr40_internal_hits=" << ptr40InternalHits
               << " ptr60_internal_hits=" << ptr60InternalHits
               << " n48_internal_hits=" << n48InternalHits
               << " ptr40_changed=" << (ptr40Changed ? 1 : 0)
               << " ptr60_changed=" << (ptr60Changed ? 1 : 0);
        AppendLogLine(fields.str());
    }

    if (g_cooldownPacketWatch.ptrWriteHits.empty()) {
        AppendLogLine("[" + BuildTimestampForLog() + "] [PTRCMP  ] run_id=" +
                      std::to_string(g_cooldownPacketWatch.runId) + " <no_ptr_hits>");
        return;
    }
    if (IsBridgeOnlyModeEnabled()) {
        for (const auto& hit : g_cooldownPacketWatch.ptrWriteHits) {
            std::ostringstream line;
            line << "[" << BuildTimestampForLog() << "] [PTRCMP  ] "
                 << "run_id=" << g_cooldownPacketWatch.runId
                 << " " << PtrWriteHitKindToString(hit.kind)
                 << " old=";
            if (hit.haveOldValue) {
                line << "0x" << std::hex << hit.oldValue;
            } else {
                line << "<unknown>";
            }
            line << " new=0x" << std::hex << hit.newValue
                 << " delta=";
            if (hit.haveOldValue) {
                line << std::dec << static_cast<long long>(hit.newValue) - static_cast<long long>(hit.oldValue);
            } else {
                line << "<unknown>";
            }
            line << " hit_addr=0x" << std::hex << hit.hitAddr
                 << " rip=0x" << hit.rip
                 << std::dec << " slot=";
            if (hit.slot == UINT32_MAX) {
                line << "na";
            } else {
                line << hit.slot;
            }
            line << " phase=" << FormatBridgePhase(hit.phase);
            if (!hit.source.empty()) {
                line << " source=" << hit.source;
            }
            AppendLogLine(line.str());
        }
        return;
    }

    struct WindowEvent {
        long long deltaMs = 0;
        std::string label;
    };

    for (size_t i = 0; i < g_cooldownPacketWatch.ptrWriteHits.size(); ++i) {
        const auto& hit = g_cooldownPacketWatch.ptrWriteHits[i];
        std::ostringstream header;
        header << "[" << BuildTimestampForLog() << "] [PTRCMP  ] "
               << "anchor#" << (i + 1)
               << " center=" << FormatElapsedSeconds(hit.elapsedMs)
               << " " << BuildPtrHitLabel(hit);
        AppendLogLine(header.str());

        std::vector<WindowEvent> events;
        events.push_back({0, BuildPtrHitLabel(hit)});
        int count0017Before = 0;
        int count0017After = 0;
        int count0028Before = 0;
        int count0028After = 0;
        std::optional<long long> nearest0017BeforeDeltaMs;
        std::optional<long long> nearest0028BeforeDeltaMs;

        for (const auto& pkt : g_cooldownPacketWatch.all0017Packets) {
            const long long deltaMs = static_cast<long long>(pkt.elapsedMs) - static_cast<long long>(hit.elapsedMs);
            const long long absDeltaMs = deltaMs >= 0 ? deltaMs : -deltaMs;
            if (absDeltaMs > static_cast<long long>(kPtrPacketWindowMs)) {
                continue;
            }
            if (deltaMs < 0) {
                ++count0017Before;
                if (!nearest0017BeforeDeltaMs.has_value() || deltaMs > nearest0017BeforeDeltaMs.value()) {
                    nearest0017BeforeDeltaMs = deltaMs;
                }
            } else if (deltaMs > 0) {
                ++count0017After;
            }
            std::ostringstream label;
            label << "0x0017 size=" << pkt.payload.size()
                  << " payload=" << BuildHexFull(pkt.payload);
            events.push_back({deltaMs, label.str()});
        }

        for (const auto& pkt : g_cooldownPacketWatch.all0028Packets) {
            const long long deltaMs = static_cast<long long>(pkt.elapsedMs) - static_cast<long long>(hit.elapsedMs);
            const long long absDeltaMs = deltaMs >= 0 ? deltaMs : -deltaMs;
            if (absDeltaMs > static_cast<long long>(kPtrPacketWindowMs)) {
                continue;
            }
            if (deltaMs < 0) {
                ++count0028Before;
                if (!nearest0028BeforeDeltaMs.has_value() || deltaMs > nearest0028BeforeDeltaMs.value()) {
                    nearest0028BeforeDeltaMs = deltaMs;
                }
            } else if (deltaMs > 0) {
                ++count0028After;
            }
            std::ostringstream label;
            label << "0x0028 size=" << pkt.payload.size()
                  << " payload=" << BuildHexFull(pkt.payload);
            if (pkt.payload.size() == 6) {
                label << " field0_u16=" << ReadLe16FromVector(pkt.payload, 0)
                      << " field1_u16=" << ReadLe16FromVector(pkt.payload, 2)
                      << " field2_u16=" << ReadLe16FromVector(pkt.payload, 4);
            }
            events.push_back({deltaMs, label.str()});
        }

        const bool sequence0028Then0017ThenPtr =
            nearest0028BeforeDeltaMs.has_value() &&
            nearest0017BeforeDeltaMs.has_value() &&
            nearest0028BeforeDeltaMs.value() < nearest0017BeforeDeltaMs.value();

        std::ostringstream summary;
        summary << "[" << BuildTimestampForLog() << "] [PTRCMP  ] "
                << "  summary"
                << " before_500ms_0017_count=" << count0017Before
                << " before_500ms_0028_count=" << count0028Before
                << " after_500ms_0017_count=" << count0017After
                << " after_500ms_0028_count=" << count0028After
                << " nearest_before_0017_delta_ms="
                << (nearest0017BeforeDeltaMs.has_value() ? std::to_string(nearest0017BeforeDeltaMs.value()) : std::string("<none>"))
                << " nearest_before_0028_delta_ms="
                << (nearest0028BeforeDeltaMs.has_value() ? std::to_string(nearest0028BeforeDeltaMs.value()) : std::string("<none>"))
                << " seq_0028_then_0017_then_ptr=" << (sequence0028Then0017ThenPtr ? 1 : 0);
        AppendLogLine(summary.str());

        std::sort(events.begin(), events.end(), [](const WindowEvent& a, const WindowEvent& b) {
            if (a.deltaMs != b.deltaMs) {
                return a.deltaMs < b.deltaMs;
            }
            return a.label < b.label;
        });

        for (const auto& event : events) {
            std::ostringstream line;
            line << "[" << BuildTimestampForLog() << "] [PTRCMP  ] "
                 << "  delta_ms=" << (event.deltaMs >= 0 ? "+" : "") << event.deltaMs
                 << " " << event.label;
            AppendLogLine(line.str());
        }
    }
}

void Dump0028ComparisonTableLocked() {
    if (!g_cooldownPacketWatch.active) {
        return;
    }

    AppendLogLine("[" + BuildTimestampForLog() + "] [CD0028  ] === 0x0028 TABLE === run_id=" + std::to_string(g_cooldownPacketWatch.runId));
    if (g_cooldownPacketWatch.all0028Packets.empty()) {
        AppendLogLine("[" + BuildTimestampForLog() + "] [CD0028  ] run_id=" + std::to_string(g_cooldownPacketWatch.runId) + " <none>");
        return;
    }

    for (const auto& pkt : g_cooldownPacketWatch.all0028Packets) {
        const auto deltaToReady = ComputeDeltaToReadyMs(pkt.elapsedMs);
        std::ostringstream line;
        line << "[" << BuildTimestampForLog() << "] [CD0028  ] "
             << "run_id=" << g_cooldownPacketWatch.runId
             << " elapsed_ms=" << pkt.elapsedMs
             << " size=" << pkt.payload.size()
             << " header=0x" << std::hex << std::setw(4) << std::setfill('0') << pkt.rawHeaderId
             << std::dec
             << " payload_hex=" << BuildHexFull(pkt.payload);
        if (pkt.payload.size() == 6) {
            line << " field0_u16=" << ReadLe16FromVector(pkt.payload, 0)
                 << " field1_u16=" << ReadLe16FromVector(pkt.payload, 2)
                 << " field2_u16=" << ReadLe16FromVector(pkt.payload, 4);
        }
        line << " stage_tag=" << ComputePhaseTagFor0028(pkt.elapsedMs);
        if (deltaToReady.has_value()) {
            line << " delta_to_ready_ms=" << deltaToReady.value();
        } else {
            line << " delta_to_ready_ms=<unknown>";
        }
        AppendLogLine(line.str());
    }
}

void Dump0017GroupStageTableLocked() {
    if (!g_cooldownPacketWatch.active) {
        return;
    }

    AppendLogLine("[" + BuildTimestampForLog() + "] [CD0017T ] === 0x0017 GROUP TABLE === run_id=" +
                  std::to_string(g_cooldownPacketWatch.runId));
    if (g_completed0017Groups.empty()) {
        AppendLogLine("[" + BuildTimestampForLog() + "] [CD0017T ] run_id=" +
                      std::to_string(g_cooldownPacketWatch.runId) + " <none>");
        return;
    }

    for (const auto& group : g_completed0017Groups) {
        std::vector<size_t> sizes;
        for (const auto& entry : group.entries) {
            const size_t currentSize = entry.payload.size();
            if (std::find(sizes.begin(), sizes.end(), currentSize) == sizes.end()) {
                sizes.push_back(currentSize);
            }
        }
        std::sort(sizes.begin(), sizes.end());

        std::ostringstream sizeList;
        for (size_t i = 0; i < sizes.size(); ++i) {
            if (i != 0) {
                sizeList << ",";
            }
            sizeList << sizes[i];
        }

        const auto deltaToReady = ComputeDeltaToReadyMs(group.firstElapsedMs);
        std::ostringstream line;
        line << "[" << BuildTimestampForLog() << "] [CD0017T ] "
             << "run_id=" << g_cooldownPacketWatch.runId
             << " group_offset_ms=" << group.firstElapsedMs
             << " packet_sizes_present=" << sizeList.str()
             << " stage_tag="
             << (deltaToReady.has_value() ? ComputeStageTagFromDeltaToReady(deltaToReady.value()) : "unknown");
        if (deltaToReady.has_value()) {
            line << " delta_to_ready_ms=" << deltaToReady.value();
        } else {
            line << " delta_to_ready_ms=<unknown>";
        }
        AppendLogLine(line.str());
    }
}

void DumpNearReadyHitRateSummaryLocked() {
    if (!g_cooldownPacketWatch.active) {
        return;
    }

    const auto anchorMs = GetNodeReadyAnchorMsLocked();
    if (!anchorMs.has_value()) {
        std::ostringstream unavailableLine;
        unavailableLine << "[" << BuildTimestampForLog() << "] [CDSTAGE ] "
                        << "run_id=" << g_cooldownPacketWatch.runId
                        << " ready_anchor_status=unavailable"
                        << " 0x0028_stage_stats=unavailable"
                        << " 0x0017_stage_stats=unavailable"
                        << " near_ready_analysis=disabled";
        AppendLogLine(unavailableLine.str());
        return;
    }

    size_t pkt0028NearReady = 0;
    size_t pkt0028Total = g_cooldownPacketWatch.all0028Packets.size();
    for (const auto& pkt : g_cooldownPacketWatch.all0028Packets) {
        const auto deltaToReady = ComputeDeltaToReadyMs(pkt.elapsedMs);
        if (deltaToReady.has_value() &&
            ComputeStageTagFromDeltaToReady(deltaToReady.value()) == "near_ready") {
            ++pkt0028NearReady;
        }
    }

    size_t group0017NearReady = 0;
    size_t group0017Total = g_completed0017Groups.size();
    for (const auto& group : g_completed0017Groups) {
        const auto deltaToReady = ComputeDeltaToReadyMs(group.firstElapsedMs);
        if (deltaToReady.has_value() &&
            ComputeStageTagFromDeltaToReady(deltaToReady.value()) == "near_ready") {
            ++group0017NearReady;
        }
    }

    std::ostringstream line;
    line << "[" << BuildTimestampForLog() << "] [CDSTAGE ] "
         << "run_id=" << g_cooldownPacketWatch.runId
         << " ready_anchor_status=valid"
         << " node_ready_anchor=NODE_READY"
         << " 0x0028_near_ready_hits=" << pkt0028NearReady
         << "/" << pkt0028Total;
    if (pkt0028Total != 0) {
        line << " 0x0028_near_ready_rate="
             << std::fixed << std::setprecision(3)
             << (static_cast<double>(pkt0028NearReady) / static_cast<double>(pkt0028Total));
    } else {
        line << " 0x0028_near_ready_rate=<none>";
    }
    line << " 0x0017_group_near_ready_hits=" << group0017NearReady
         << "/" << group0017Total;
    if (group0017Total != 0) {
        line << " 0x0017_group_near_ready_rate="
             << std::fixed << std::setprecision(3)
             << (static_cast<double>(group0017NearReady) / static_cast<double>(group0017Total));
    } else {
        line << " 0x0017_group_near_ready_rate=<none>";
    }
    AppendLogLine(line.str());
}

size_t CountPtrHitsNearAnchorLocked(PtrWriteHitKind kind,
                                    unsigned long long anchorMs,
                                    unsigned long long windowMs) {
    size_t count = 0;
    for (const auto& hit : g_cooldownPacketWatch.ptrWriteHits) {
        if (hit.kind != kind) {
            continue;
        }
        const unsigned long long diff =
            hit.elapsedMs >= anchorMs ? (hit.elapsedMs - anchorMs) : (anchorMs - hit.elapsedMs);
        if (diff <= windowMs) {
            ++count;
        }
    }
    return count;
}

size_t Count0028NearReadyLocked(unsigned long long windowMs) {
    if (!g_cooldownPacketWatch.haveNodeReadyOffset) {
        return 0;
    }
    size_t count = 0;
    for (const auto& pkt : g_cooldownPacketWatch.all0028Packets) {
        const unsigned long long diff =
            pkt.elapsedMs >= g_cooldownPacketWatch.nodeReadyOffsetMs
                ? (pkt.elapsedMs - g_cooldownPacketWatch.nodeReadyOffsetMs)
                : (g_cooldownPacketWatch.nodeReadyOffsetMs - pkt.elapsedMs);
        if (diff <= windowMs) {
            ++count;
        }
    }
    return count;
}

size_t Count0017GroupNearReadyLocked(unsigned long long windowMs) {
    if (!g_cooldownPacketWatch.haveNodeReadyOffset) {
        return 0;
    }
    size_t count = 0;
    for (const auto& group : g_completed0017Groups) {
        const unsigned long long diff =
            group.firstElapsedMs >= g_cooldownPacketWatch.nodeReadyOffsetMs
                ? (group.firstElapsedMs - g_cooldownPacketWatch.nodeReadyOffsetMs)
                : (g_cooldownPacketWatch.nodeReadyOffsetMs - group.firstElapsedMs);
        if (diff <= windowMs) {
            ++count;
        }
    }
    return count;
}

std::pair<uintptr_t, size_t> FindTopRipForKindLocked(PtrWriteHitKind kind) {
    std::unordered_map<uintptr_t, size_t> counts;
    for (const auto& hit : g_cooldownPacketWatch.ptrWriteHits) {
        if (hit.kind != kind) {
            continue;
        }
        ++counts[hit.rip];
    }

    uintptr_t topRip = 0;
    size_t topCount = 0;
    for (const auto& [rip, count] : counts) {
        if (count > topCount) {
            topRip = rip;
            topCount = count;
        }
    }
    return {topRip, topCount};
}

const char* ConfidenceLabelFromScore(double value01) {
    if (value01 >= 0.75) return "HIGH";
    if (value01 >= 0.45) return "MEDIUM";
    return "LOW";
}

void DumpAutoRunSummaryLocked() {
    if (!g_cooldownPacketWatch.active) {
        return;
    }
    if (IsBridgeOnlyModeEnabled()) {
        return;
    }

    const size_t ptr40Hits = std::count_if(g_cooldownPacketWatch.ptrWriteHits.begin(),
                                           g_cooldownPacketWatch.ptrWriteHits.end(),
                                           [](const PtrWriteHitRecord& hit) {
        return hit.kind == PtrWriteHitKind::PTR40;
                                           });
    const size_t ptr60Hits = std::count_if(g_cooldownPacketWatch.ptrWriteHits.begin(),
                                           g_cooldownPacketWatch.ptrWriteHits.end(),
                                           [](const PtrWriteHitRecord& hit) {
        return hit.kind == PtrWriteHitKind::PTR60;
                                           });
    const size_t ptr40InternalHits = std::count_if(g_cooldownPacketWatch.ptrWriteHits.begin(),
                                                   g_cooldownPacketWatch.ptrWriteHits.end(),
                                                   [](const PtrWriteHitRecord& hit) {
        return hit.kind == PtrWriteHitKind::PTR40 && hit.source == "internal_poll";
                                                   });
    const size_t ptr60InternalHits = std::count_if(g_cooldownPacketWatch.ptrWriteHits.begin(),
                                                   g_cooldownPacketWatch.ptrWriteHits.end(),
                                                   [](const PtrWriteHitRecord& hit) {
        return hit.kind == PtrWriteHitKind::PTR60 && hit.source == "internal_poll";
                                                   });
    const size_t n48InternalHits = std::count_if(g_cooldownPacketWatch.ptrWriteHits.begin(),
                                                 g_cooldownPacketWatch.ptrWriteHits.end(),
                                                 [](const PtrWriteHitRecord& hit) {
        return hit.kind == PtrWriteHitKind::N48 && hit.source == "internal_poll";
                                                 });
    const size_t ptr40InternalNodeCdHits = std::count_if(g_cooldownPacketWatch.ptrWriteHits.begin(),
                                                         g_cooldownPacketWatch.ptrWriteHits.end(),
                                                         [](const PtrWriteHitRecord& hit) {
        return hit.kind == PtrWriteHitKind::PTR40 &&
               hit.source == "internal_poll" &&
               hit.phase == 2;
                                                         });
    const size_t ptr60InternalNodeCdHits = std::count_if(g_cooldownPacketWatch.ptrWriteHits.begin(),
                                                         g_cooldownPacketWatch.ptrWriteHits.end(),
                                                         [](const PtrWriteHitRecord& hit) {
        return hit.kind == PtrWriteHitKind::PTR60 &&
               hit.source == "internal_poll" &&
               hit.phase == 2;
                                                         });
    const bool internalNodeCdChainDetected = ptr40InternalNodeCdHits != 0 && ptr60InternalNodeCdHits != 0;
    const bool nodeCdDetected = g_cooldownPacketWatch.haveNodeCdOffset || internalNodeCdChainDetected;
    const bool ptr40Changed = ptr40InternalHits != 0 || ptr40InternalNodeCdHits != 0;
    const bool ptr60Changed = ptr60InternalHits != 0 || ptr60InternalNodeCdHits != 0;
    const bool pointerChainDetected = internalNodeCdChainDetected || (ptr40Changed && ptr60Changed && nodeCdDetected);
    const size_t ptrTotalHits = ptr40Hits + ptr60Hits;

    const size_t nearCd40 = g_cooldownPacketWatch.haveNodeCdOffset
        ? CountPtrHitsNearAnchorLocked(PtrWriteHitKind::PTR40, g_cooldownPacketWatch.nodeCdOffsetMs, 250)
        : 0;
    const size_t nearCd60 = g_cooldownPacketWatch.haveNodeCdOffset
        ? CountPtrHitsNearAnchorLocked(PtrWriteHitKind::PTR60, g_cooldownPacketWatch.nodeCdOffsetMs, 250)
        : 0;
    const size_t nearReady40 = g_cooldownPacketWatch.haveNodeReadyOffset
        ? CountPtrHitsNearAnchorLocked(PtrWriteHitKind::PTR40, g_cooldownPacketWatch.nodeReadyOffsetMs, 300)
        : 0;
    const size_t nearReady60 = g_cooldownPacketWatch.haveNodeReadyOffset
        ? CountPtrHitsNearAnchorLocked(PtrWriteHitKind::PTR60, g_cooldownPacketWatch.nodeReadyOffsetMs, 300)
        : 0;

    const double score40 =
        static_cast<double>(ptr40Hits) * 1.0 +
        static_cast<double>(nearCd40) * 2.2 +
        static_cast<double>(nearReady40) * 1.8 +
        (nodeCdDetected ? 0.8 : 0.0) +
        (g_cooldownPacketWatch.haveNodeReadyOffset ? 0.6 : 0.0);
    const double score60 =
        static_cast<double>(ptr60Hits) * 1.0 +
        static_cast<double>(nearCd60) * 2.6 +
        static_cast<double>(nearReady60) * 2.0 +
        (nodeCdDetected ? 1.0 : 0.0) +
        (g_cooldownPacketWatch.haveNodeReadyOffset ? 0.8 : 0.0);

    const double baseScoreSum = score40 + score60;
    double baseConfidence = 0.0;
    bool preferPtr60 = false;
    if (baseScoreSum > 0.0) {
        preferPtr60 = score60 >= score40;
        baseConfidence = std::max(score40, score60) / baseScoreSum;
    }
    if (!internalNodeCdChainDetected &&
        (!g_cooldownPacketWatch.loggedCooldownStart || !g_cooldownPacketWatch.loggedReadyReturn)) {
        baseConfidence *= 0.75;
    }
    if (!internalNodeCdChainDetected && ptrTotalHits < 2) {
        baseConfidence *= 0.65;
    }
    if (internalNodeCdChainDetected) {
        baseConfidence = (std::max)(baseConfidence, 0.78);
    }
    baseConfidence = std::clamp(baseConfidence, 0.0, 0.99);

    const auto [topRip40, topRip40Count] = FindTopRipForKindLocked(PtrWriteHitKind::PTR40);
    const auto [topRip60, topRip60Count] = FindTopRipForKindLocked(PtrWriteHitKind::PTR60);
    const bool internalPollOnlyRip =
        (ptr40Hits + ptr60Hits) > 0 &&
        std::all_of(g_cooldownPacketWatch.ptrWriteHits.begin(),
                    g_cooldownPacketWatch.ptrWriteHits.end(),
                    [](const PtrWriteHitRecord& hit) {
        if (hit.kind != PtrWriteHitKind::PTR40 && hit.kind != PtrWriteHitKind::PTR60) {
            return true;
        }
        return hit.source == "internal_poll" && hit.rip == 0;
                    });

    const size_t nearReady0028 = Count0028NearReadyLocked(300);
    const size_t nearReady0017 = Count0017GroupNearReadyLocked(300);
    const size_t total0028 = g_cooldownPacketWatch.all0028Packets.size();
    const size_t total0017Groups = g_completed0017Groups.size();
    const double packetScore0028 = static_cast<double>(total0028) + static_cast<double>(nearReady0028) * 1.8;
    const double packetScore0017 = static_cast<double>(total0017Groups) + static_cast<double>(nearReady0017) * 1.6;
    const double packetScoreSum = packetScore0028 + packetScore0017;
    bool prefer0028 = packetScore0028 >= packetScore0017;
    double packetConfidence = packetScoreSum > 0.0
        ? std::max(packetScore0028, packetScore0017) / packetScoreSum
        : 0.0;
    if (!g_cooldownPacketWatch.haveNodeReadyOffset) {
        packetConfidence *= 0.8;
    }
    packetConfidence = std::clamp(packetConfidence, 0.0, 0.99);

    std::ostringstream header;
    header << "[" << BuildTimestampForLog() << "] [CDAUTO  ] === AUTO RUN SUMMARY ==="
           << " run_id=" << g_cooldownPacketWatch.runId
           << " hotkey=" << g_cooldownPacketWatch.hotkeyName
           << " archive_key=" << g_cooldownPacketWatch.archiveKey;
    AppendLogLine(header.str());

    std::ostringstream baseLine;
    baseLine << "[" << BuildTimestampForLog() << "] [CDAUTO  ] "
             << "base_candidate="
             << (preferPtr60 ? "PTR60_TREE(base=ptr60-0x60,fields@+0x40/+0x60)"
                             : "PTR40_TREE(base+0x40-node path)")
             << " confidence="
             << std::fixed << std::setprecision(2) << (baseConfidence * 100.0) << "%"
             << " level=" << ConfidenceLabelFromScore(baseConfidence)
             << " score_ptr40=" << std::setprecision(3) << score40
             << " score_ptr60=" << score60
             << " ptr40_hits=" << ptr40Hits
             << " ptr60_hits=" << ptr60Hits
             << " ptr40_internal_hits=" << ptr40InternalHits
             << " ptr60_internal_hits=" << ptr60InternalHits
             << " n48_internal_hits=" << n48InternalHits
             << " ptr40_internal_node_cd_hits=" << ptr40InternalNodeCdHits
             << " ptr60_internal_node_cd_hits=" << ptr60InternalNodeCdHits
             << " ptr40_changed=" << (ptr40Changed ? 1 : 0)
             << " ptr60_changed=" << (ptr60Changed ? 1 : 0)
             << " pointer_chain_detected=" << (pointerChainDetected ? "yes" : "no")
             << " near_cd(ptr40/ptr60)=" << nearCd40 << "/" << nearCd60
             << " near_ready(ptr40/ptr60)=" << nearReady40 << "/" << nearReady60
             << " node_cd=" << (nodeCdDetected ? "yes" : "no")
             << " node_ready=" << (g_cooldownPacketWatch.haveNodeReadyOffset ? "yes" : "no");
    AppendLogLine(baseLine.str());

    std::ostringstream ripLine;
    ripLine << "[" << BuildTimestampForLog() << "] [CDAUTO  ] "
            << "rip_hotspots ";
    if (internalPollOnlyRip) {
        ripLine << "unavailable source=internal_poll";
    } else {
        ripLine << "ptr40_top=0x" << std::hex << topRip40 << std::dec << "x" << topRip40Count
                << " ptr60_top=0x" << std::hex << topRip60 << std::dec << "x" << topRip60Count;
    }
    AppendLogLine(ripLine.str());

    std::ostringstream packetLine;
    packetLine << "[" << BuildTimestampForLog() << "] [CDAUTO  ] "
               << "packet_candidate=" << (prefer0028 ? "0x0028(AGENT_ATTRIBUTE_UPDATE)" : "0x0017-group(SKILL_UPDATE)")
               << " confidence=" << std::fixed << std::setprecision(2) << (packetConfidence * 100.0) << "%"
               << " level=" << ConfidenceLabelFromScore(packetConfidence)
               << " total_0028=" << total0028
               << " total_0017_groups=" << total0017Groups
               << " near_ready_0028=" << nearReady0028
               << " near_ready_0017_groups=" << nearReady0017
               << " node_ready_anchor=" << (g_cooldownPacketWatch.haveNodeReadyOffset ? "yes" : "no");
    AppendLogLine(packetLine.str());
}

void Dump0028PairSummaryLocked() {
    if (!g_cooldownPacketWatch.active) {
        return;
    }

    AppendLogLine("[" + BuildTimestampForLog() + "] [CD0028  ] === 0x0028 PAIRS === run_id=" + std::to_string(g_cooldownPacketWatch.runId));

    bool foundPair = false;
    for (size_t i = 0; i < g_cooldownPacketWatch.all0028Packets.size(); ++i) {
        const auto& first = g_cooldownPacketWatch.all0028Packets[i];
        if (first.payload.size() != 2) {
            continue;
        }
        for (size_t j = i + 1; j < g_cooldownPacketWatch.all0028Packets.size(); ++j) {
            const auto& second = g_cooldownPacketWatch.all0028Packets[j];
            if (second.elapsedMs < first.elapsedMs) {
                continue;
            }
            if (second.elapsedMs - first.elapsedMs >= 300) {
                break;
            }
            if (second.payload.size() != 6) {
                continue;
            }

            foundPair = true;
            const uint8_t xx = second.payload.size() > 2 ? second.payload[2] : 0;
            const auto deltaToReady = ComputeDeltaToReadyMs(second.elapsedMs);
            std::ostringstream line;
            line << "[" << BuildTimestampForLog() << "] [CD0028  ] "
                 << "run_id=" << g_cooldownPacketWatch.runId
                 << " pair size2_ms=" << first.elapsedMs
                 << " -> size6_ms=" << second.elapsedMs
                 << " gap_ms=" << (second.elapsedMs - first.elapsedMs)
                 << " size6_xx="
                 << std::hex << std::nouppercase << std::setw(2) << std::setfill('0')
                 << static_cast<unsigned>(xx)
                 << std::dec
                 << " stage_tag=" << ComputePhaseTagFor0028(second.elapsedMs);
            if (deltaToReady.has_value()) {
                line << " delta_to_ready_ms=" << deltaToReady.value();
            } else {
                line << " delta_to_ready_ms=<unknown>";
            }
            AppendLogLine(line.str());
            break;
        }
    }

    if (!foundPair) {
        AppendLogLine("[" + BuildTimestampForLog() + "] [CD0028  ] run_id=" + std::to_string(g_cooldownPacketWatch.runId) + " no_pairs");
    }
}

void Dump0028XXSummaryLocked() {
    if (!g_cooldownPacketWatch.active) {
        return;
    }

    std::map<uint8_t, int> xxFreq;
    std::map<std::string, int> phaseFreq;
    for (const auto& pkt : g_cooldownPacketWatch.all0028Packets) {
        if (pkt.payload.size() != 6) {
            continue;
        }
        const uint8_t xx = pkt.payload[2];
        ++xxFreq[xx];
        ++phaseFreq[ComputePhaseTagFor0028(pkt.elapsedMs)];
    }

    std::ostringstream header;
    header << "[" << BuildTimestampForLog() << "] [CD0028  ] === 0x0028 XX SUMMARY === run_id="
           << g_cooldownPacketWatch.runId;
    AppendLogLine(header.str());

    std::ostringstream xxLine;
    xxLine << "[" << BuildTimestampForLog() << "] [CD0028  ] size6_xx_freq";
    if (xxFreq.empty()) {
        xxLine << " <none>";
    } else {
        for (const auto& [xx, count] : xxFreq) {
            xxLine << " "
                   << std::hex << std::nouppercase << std::setw(2) << std::setfill('0')
                   << static_cast<unsigned>(xx)
                   << std::dec << "x" << count;
        }
    }
    AppendLogLine(xxLine.str());

    std::ostringstream phaseLine;
    phaseLine << "[" << BuildTimestampForLog() << "] [CD0028  ] size6_phase_freq";
    if (phaseFreq.empty()) {
        phaseLine << " <none>";
    } else {
        for (const auto& [phase, count] : phaseFreq) {
            phaseLine << " " << phase << "x" << count;
        }
    }
    AppendLogLine(phaseLine.str());
}

void DumpNodeReadyWindowLocked() {
    if (!g_cooldownPacketWatch.active ||
        !g_cooldownPacketWatch.haveNodeReadyOffset ||
        g_cooldownPacketWatch.dumpedNodeReadyWindow) {
        return;
    }

    std::vector<const TimedPayload0017*> before;
    std::vector<const TimedPayload0017*> after;
    for (const auto& entry : g_cooldownPacketWatch.all0017Packets) {
        if (entry.elapsedMs + 3000 < g_cooldownPacketWatch.nodeReadyOffsetMs) {
            continue;
        }
        if (entry.elapsedMs > g_cooldownPacketWatch.nodeReadyOffsetMs + 3000) {
            continue;
        }
        if (entry.elapsedMs < g_cooldownPacketWatch.nodeReadyOffsetMs) {
            before.push_back(&entry);
        } else {
            after.push_back(&entry);
        }
    }

    {
        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [CDREADY ] === NODE_READY WINDOW ANALYSIS ==="
            << " node_ready=" << FormatElapsedSeconds(g_cooldownPacketWatch.nodeReadyOffsetMs)
            << " before_count=" << before.size()
            << " after_count=" << after.size();
        AppendLogLine(oss.str());
    }

    auto dumpSide = [&](const char* label, const std::vector<const TimedPayload0017*>& entries, bool positivePrefix) {
        std::ostringstream header;
        header << "[" << BuildTimestampForLog() << "] [CDREADY ] "
               << "[" << label << "] count=" << entries.size();
        AppendLogLine(header.str());

        std::map<uint8_t, int> len30Freq;
        std::map<std::string, int> len31PairFreq;
        for (size_t i = 0; i < entries.size(); ++i) {
            const auto& entry = *entries[i];
            const long long deltaMs = static_cast<long long>(entry.elapsedMs) -
                                      static_cast<long long>(g_cooldownPacketWatch.nodeReadyOffsetMs);

            std::ostringstream line;
            line << "[" << BuildTimestampForLog() << "] [CDREADY ] "
                 << "  [" << i << "] offset="
                 << (deltaMs >= 0 ? "+" : "") << std::fixed << std::setprecision(3)
                 << (static_cast<double>(deltaMs) / 1000.0) << "s"
                 << " len=" << entry.payload.size();

            if (entry.payload.size() == 30 && entry.payload.size() > 18) {
                const uint8_t b18 = entry.payload[18];
                ++len30Freq[b18];
                line << " [18]="
                     << std::hex << std::nouppercase << std::setw(2) << std::setfill('0')
                     << static_cast<unsigned>(b18) << std::dec;
            } else if (entry.payload.size() >= 22 && entry.payload.size() == 31) {
                const uint16_t lo = ReadLe16FromVector(entry.payload, 18);
                const uint16_t hi = ReadLe16FromVector(entry.payload, 20);
                std::ostringstream key;
                key << std::hex << std::nouppercase << std::setw(4) << std::setfill('0')
                    << lo << "/" << std::setw(4) << hi;
                ++len31PairFreq[key.str()];
                line << " [18..19]="
                     << std::hex << std::nouppercase << std::setw(4) << std::setfill('0') << lo
                     << " [20..21]=" << std::setw(4) << hi
                     << std::dec;
            }
            AppendLogLine(line.str());
        }

        {
            std::ostringstream freq;
            freq << "[" << BuildTimestampForLog() << "] [CDREADY ] "
                 << "  [LEN30 BYTE18 FREQ " << label << "]";
            if (len30Freq.empty()) {
                freq << " <none>";
            } else {
                for (const auto& [value, count] : len30Freq) {
                    freq << " "
                         << std::hex << std::nouppercase << std::setw(2) << std::setfill('0')
                         << static_cast<unsigned>(value)
                         << std::dec << "x" << count;
                }
            }
            AppendLogLine(freq.str());
        }

        {
            std::ostringstream freq;
            freq << "[" << BuildTimestampForLog() << "] [CDREADY ] "
                 << "  [LEN31 U16PAIR FREQ " << label << "]";
            if (len31PairFreq.empty()) {
                freq << " <none>";
            } else {
                for (const auto& [value, count] : len31PairFreq) {
                    freq << " " << value << "x" << count;
                }
            }
            AppendLogLine(freq.str());
        }
    };

    dumpSide("BEFORE", before, false);
    dumpSide("AFTER", after, true);
    g_cooldownPacketWatch.dumpedNodeReadyWindow = true;
}

void Finalize0017GroupLocked(Packet0017Group&& group) {
    if (group.entries.empty()) {
        return;
    }
    if (IsBridgeOnlyModeEnabled()) {
        return;
    }

    const size_t groupIndex = g_completed0017Groups.size() + 1;
    std::ostringstream header;
    header << "[" << BuildTimestampForLog() << "] [CD0017G ] "
           << "hotkey=" << g_cooldownPacketWatch.hotkeyName
           << " archive_key=" << g_cooldownPacketWatch.archiveKey
           << " GROUP #" << groupIndex
           << " offset=" << FormatElapsedSeconds(group.firstElapsedMs)
           << " count=" << group.entries.size();
    AppendLogLine(header.str());

    for (size_t i = 0; i < group.entries.size(); ++i) {
        std::ostringstream packetLine;
        packetLine << "[" << BuildTimestampForLog() << "] [CD0017G ] "
                   << "  [" << i << "]"
                   << " ts=" << FormatElapsedSeconds(group.entries[i].elapsedMs)
                   << " len=" << group.entries[i].payload.size()
                   << " payload=" << BuildHexPreview(group.entries[i].payload, 24);
        AppendLogLine(packetLine.str());
    }

    std::map<uint8_t, std::vector<const TimedPayload0017*>> buckets;
    for (const auto& entry : group.entries) {
        buckets[static_cast<uint8_t>(entry.payload.size())].push_back(&entry);
    }

    std::ostringstream timelineLabel;
    timelineLabel << "GROUP#" << groupIndex;

    for (const auto& [len, bucketEntries] : buckets) {
        std::ostringstream bucketLine;
        bucketLine << "[" << BuildTimestampForLog() << "] [CD0017G ] "
                   << "BUCKET len=" << static_cast<unsigned>(len)
                   << " count=" << bucketEntries.size();
        AppendLogLine(bucketLine.str());

        if (len == 30) {
            std::ostringstream seqHeader;
            seqHeader << "[" << BuildTimestampForLog() << "] [CD0017G ] "
                      << "[LEN30 BYTE18 SEQUENCE] GROUP#" << groupIndex
                      << " offset=" << FormatElapsedSeconds(group.firstElapsedMs);
            AppendLogLine(seqHeader.str());

            std::map<uint8_t, int> freq;
            std::ostringstream summary;
            summary << " len30[18]:";
            for (size_t i = 0; i < bucketEntries.size(); ++i) {
                if (bucketEntries[i]->payload.size() <= 18) {
                    continue;
                }
                const uint8_t b18 = bucketEntries[i]->payload[18];
                ++freq[b18];

                std::ostringstream line;
                line << "[" << BuildTimestampForLog() << "] [CD0017G ] "
                     << "    [" << i << "] [18]="
                     << std::hex << std::nouppercase << std::setw(2) << std::setfill('0')
                     << static_cast<unsigned>(b18)
                     << std::dec;
                AppendLogLine(line.str());

                summary << (i == 0 ? " " : "/")
                        << std::hex << std::nouppercase << std::setw(2) << std::setfill('0')
                        << static_cast<unsigned>(b18)
                        << std::dec;
            }

            std::ostringstream freqLine;
            freqLine << "[" << BuildTimestampForLog() << "] [CD0017G ]   FREQ:";
            for (const auto& [value, count] : freq) {
                freqLine << " "
                         << std::hex << std::nouppercase << std::setw(2) << std::setfill('0')
                         << static_cast<unsigned>(value)
                         << std::dec << "x" << count;
            }
            AppendLogLine(freqLine.str());
            timelineLabel << " " << summary.str();
        }

        if (len == 31) {
            std::ostringstream pairHeader;
            pairHeader << "[" << BuildTimestampForLog() << "] [CD0017G ] "
                       << "[LEN31 U16PAIR [18..21]] GROUP#" << groupIndex
                       << " offset=" << FormatElapsedSeconds(group.firstElapsedMs);
            AppendLogLine(pairHeader.str());

            std::ostringstream summary;
            summary << " len31[18..21]:";
            bool wroteSummary = false;
            for (size_t i = 0; i < bucketEntries.size(); ++i) {
                if (bucketEntries[i]->payload.size() < 22) {
                    continue;
                }
                const uint16_t lo = ReadLe16FromVector(bucketEntries[i]->payload, 18);
                const uint16_t hi = ReadLe16FromVector(bucketEntries[i]->payload, 20);

                std::ostringstream line;
                line << "[" << BuildTimestampForLog() << "] [CD0017G ] "
                     << "    [" << i << "] [18..19]="
                     << std::hex << std::nouppercase << std::setw(4) << std::setfill('0') << lo
                     << std::dec << " (" << std::setw(4) << lo << ")"
                     << " [20..21]="
                     << std::hex << std::nouppercase << std::setw(4) << std::setfill('0') << hi
                     << std::dec << " (" << std::setw(4) << hi << ")";
                AppendLogLine(line.str());

                if (!wroteSummary) {
                    summary << " ";
                    wroteSummary = true;
                } else {
                    summary << ", ";
                }
                summary << std::hex << std::nouppercase << std::setw(4) << std::setfill('0') << lo
                        << "/"
                        << std::setw(4) << hi
                        << std::dec;
            }
            if (wroteSummary) {
                timelineLabel << " |" << summary.str();
            }
        }

        const auto& firstPayload = bucketEntries.front()->payload;
        std::vector<std::pair<size_t, uint8_t>> stableBytes;
        std::vector<std::string> varyingFields;
        for (size_t i = 0; i < firstPayload.size(); ++i) {
            const uint8_t first = firstPayload[i];
            bool stable = true;
            std::ostringstream values;
            values << std::hex << std::nouppercase << std::setw(2) << std::setfill('0')
                   << static_cast<unsigned>(first);
            for (size_t p = 1; p < bucketEntries.size(); ++p) {
                const uint8_t current = bucketEntries[p]->payload[i];
                if (current != first) {
                    stable = false;
                }
                values << "->" << std::setw(2) << static_cast<unsigned>(current);
            }
            if (stable) {
                stableBytes.push_back({i, first});
            } else {
                std::ostringstream field;
                field << "[" << i << "]=" << values.str();
                varyingFields.push_back(field.str());
            }
        }

        std::ostringstream stableLine;
        stableLine << "[" << BuildTimestampForLog() << "] [CD0017G ]   STABLE:";
        if (stableBytes.empty()) {
            stableLine << " <none>";
        } else {
            const size_t emitCount = std::min<size_t>(stableBytes.size(), 24);
            for (size_t i = 0; i < emitCount; ++i) {
                stableLine << " [" << stableBytes[i].first << "]="
                           << std::hex << std::nouppercase << std::setw(2) << std::setfill('0')
                           << static_cast<unsigned>(stableBytes[i].second)
                           << std::dec;
            }
            if (stableBytes.size() > emitCount) {
                stableLine << " ...";
            }
        }
        AppendLogLine(stableLine.str());

        std::ostringstream varyingLine;
        varyingLine << "[" << BuildTimestampForLog() << "] [CD0017G ]   VARYING:";
        if (varyingFields.empty()) {
            varyingLine << " <none>";
        } else {
            const size_t emitCount = std::min<size_t>(varyingFields.size(), 12);
            for (size_t i = 0; i < emitCount; ++i) {
                varyingLine << " " << varyingFields[i];
            }
            if (varyingFields.size() > emitCount) {
                varyingLine << " ...";
            }
        }
        AppendLogLine(varyingLine.str());
    }

    g_cooldownPacketWatch.groupEvents.push_back({group.firstElapsedMs, timelineLabel.str()});

    if (!g_completed0017Groups.empty()) {
        const Packet0017Group& prev = g_completed0017Groups.back();
        const size_t compareLen = std::min(prev.entries.front().payload.size(), group.entries.front().payload.size());
        std::vector<std::string> diffs;
        for (size_t i = 0; i < compareLen; ++i) {
            const uint8_t before = prev.entries.front().payload[i];
            const uint8_t after = group.entries.front().payload[i];
            if (before == after) {
                continue;
            }
            std::ostringstream diff;
            diff << "[" << i << "]="
                 << std::hex << std::nouppercase << std::setw(2) << std::setfill('0')
                 << static_cast<unsigned>(before)
                 << "->"
                 << std::setw(2) << static_cast<unsigned>(after)
                 << std::dec;
            diffs.push_back(diff.str());
        }

        std::ostringstream diffLine;
        diffLine << "[" << BuildTimestampForLog() << "] [CD0017G ] DIFF_vs_#"
                 << g_completed0017Groups.size() << ":";
        if (diffs.empty()) {
            diffLine << " <none>";
        } else {
            const size_t emitCount = std::min<size_t>(diffs.size(), 16);
            for (size_t i = 0; i < emitCount; ++i) {
                diffLine << " " << diffs[i];
            }
            if (diffs.size() > emitCount) {
                diffLine << " ...";
            }
        }
        AppendLogLine(diffLine.str());
    }

    const Packet0017Group* fieldViewGroup = &group;
    std::ostringstream fvHeader;
    fvHeader << "[" << BuildTimestampForLog() << "] [CD0017G ] "
             << "FIELD_VIEW GROUP #" << groupIndex
             << " range=[18..31]";
    AppendLogLine(fvHeader.str());
    for (size_t i = 0; i < fieldViewGroup->entries.size(); ++i) {
        const auto& entry = fieldViewGroup->entries[i];
        std::ostringstream payloadLine;
        payloadLine << "[" << BuildTimestampForLog() << "] [CD0017G ] "
                    << "  [payload " << i << "]"
                    << " len=" << entry.payload.size()
                    << " ts=" << FormatElapsedSeconds(entry.elapsedMs)
                    << " " << BuildU8FieldView(entry.payload, 18, 32)
                    << " " << BuildU16FieldView(entry.payload, 18, 32)
                    << " " << BuildU32FieldView(entry.payload, 18, 32);
        AppendLogLine(payloadLine.str());
    }

    g_completed0017Groups.push_back(std::move(group));
}

void FlushOpen0017GroupLocked() {
    if (!g_open0017Group.has_value()) {
        return;
    }
    Finalize0017GroupLocked(std::move(g_open0017Group.value()));
    g_open0017Group.reset();
}

uintptr_t ReadTaggedFieldAsNode(uintptr_t base, uintptr_t offset) {
    uint64_t value = 0;
    if (!ReadQwordSafe(base + offset, value)) {
        return 0;
    }
    return static_cast<uintptr_t>(value & ~static_cast<uint64_t>(1));
}

void RecordZStateSnapshotLocked(const char* phaseName,
                                const std::string& oldStateText,
                                const std::string& newStateText,
                                const SkillbarSnapshot& snap) {
    if (!g_cooldownPacketWatch.active || g_cooldownPacketWatch.hotkeyName != "Z") {
        return;
    }

    const uintptr_t ptr48Untagged = snap.ptr48Addr ? (snap.ptr48Addr & ~static_cast<uintptr_t>(1)) : 0;
    const uintptr_t inferredBase = snap.ptr60Addr >= 0x60 ? (snap.ptr60Addr - 0x60) : 0;
    const uintptr_t cur40 = inferredBase ? ReadTaggedFieldAsNode(inferredBase, 0x40) : 0;
    const uintptr_t cur60 = inferredBase ? ReadTaggedFieldAsNode(inferredBase, 0x60) : 0;

    uint64_t ptr48Plus20Raw = 0;
    const uintptr_t ptr48Plus20 = ptr48Untagged && ReadQwordSafe(ptr48Untagged + 0x20, ptr48Plus20Raw)
                                      ? UntagNodePointer(ptr48Plus20Raw)
                                      : 0;
    const uintptr_t n48FromCur40Addr = cur40 ? (cur40 + 0x48) : 0;
    const uintptr_t n48FromCur60Addr = cur60 ? (cur60 + 0x48) : 0;
    uint64_t n48FromCur40Value = 0;
    uint64_t n48FromCur60Value = 0;
    const bool haveN48FromCur40 = ReadQwordSafe(n48FromCur40Addr, n48FromCur40Value);
    const bool haveN48FromCur60 = ReadQwordSafe(n48FromCur60Addr, n48FromCur60Value);
    if (haveN48FromCur40) {
        RememberWatchValue(n48FromCur40Addr, n48FromCur40Value);
    }
    if (haveN48FromCur60) {
        RememberWatchValue(n48FromCur60Addr, n48FromCur60Value);
    }

    const unsigned long long nowTick = GetTickCount64();
    const unsigned long long elapsedMs =
        nowTick > g_cooldownPacketWatch.startedTickMs ? (nowTick - g_cooldownPacketWatch.startedTickMs) : 0;

    TimedZStateSnapshot snapshot = {};
    snapshot.runId = g_cooldownPacketWatch.runId;
    snapshot.elapsedMs = elapsedMs;
    snapshot.phaseName = phaseName ? phaseName : "?";
    snapshot.oldStateText = oldStateText;
    snapshot.stateText = newStateText;
    snapshot.cur40 = cur40;
    snapshot.cur60 = cur60;
    snapshot.ptr48Plus20 = ptr48Plus20;

    if (!g_cooldownPacketWatch.zStateSnapshots.empty()) {
        const auto& last = g_cooldownPacketWatch.zStateSnapshots.back();
        if (SameZStateShape(last, snapshot) && last.phaseName == snapshot.phaseName) {
            return;
        }
        if (std::strncmp(snapshot.phaseName.c_str(), "live", 4) == 0 && SameZStateShape(last, snapshot)) {
            return;
        }
    }

    g_cooldownPacketWatch.zStateSnapshots.push_back(snapshot);

    std::ostringstream oss;
    oss << "[" << BuildTimestampForLog() << "] [ZNODE   ] "
        << "run_id=" << snapshot.runId
        << " phase=" << snapshot.phaseName
        << " node_state_old=" << snapshot.oldStateText
        << " node_state_new=" << snapshot.stateText
        << " cur40=0x" << std::hex << snapshot.cur40
        << " cur60=0x" << snapshot.cur60
        << " ptr48p20=0x" << snapshot.ptr48Plus20
        << std::dec
        << " elapsed_ms=" << snapshot.elapsedMs;
    AppendLogLine(oss.str());

    {
        std::ostringstream watch;
        watch << "[" << BuildTimestampForLog() << "] [SKWATCH] "
              << "run_id=" << snapshot.runId
              << " phase=" << snapshot.phaseName
              << " cur40=0x" << std::hex << cur40
              << " n48_from_cur40_addr=0x" << n48FromCur40Addr
              << " n48_from_cur40_value=";
        if (haveN48FromCur40) {
            watch << "0x" << n48FromCur40Value;
        } else {
            watch << "<invalid>";
        }
        AppendLogLine(watch.str());
    }

    {
        std::ostringstream watch;
        watch << "[" << BuildTimestampForLog() << "] [SKWATCH] "
              << "run_id=" << snapshot.runId
              << " phase=" << snapshot.phaseName
              << " cur60=0x" << std::hex << cur60
              << " n48_from_cur60_addr=0x" << n48FromCur60Addr
              << " n48_from_cur60_value=";
        if (haveN48FromCur60) {
            watch << "0x" << n48FromCur60Value;
        } else {
            watch << "<invalid>";
        }
        AppendLogLine(watch.str());
    }
}

const std::vector<SkillConfig>& GetSkillTable() {
    return kx::SkillDb::GetSkillTable();
}

std::string BuildArchiveKey(const kx::SkillUsePacketCapture& c) {
    return kx::BuildSkillUseArchiveKeyLegacy(c);
}

bool ShouldTrackPacket(const std::string& archiveKey) {
    return archiveKey.rfind("tag2a_", 0) != 0;
}

std::string NormalizeHotkeyName(const std::string& raw) {
    auto pick = [&](const char* token, const char* out) -> std::string {
        return raw.find(token) != std::string::npos ? std::string(out) : std::string();
    };
    if (auto v = pick("(2)", "2"); !v.empty()) return v;
    if (auto v = pick("(3)", "3"); !v.empty()) return v;
    if (auto v = pick("(4)", "4"); !v.empty()) return v;
    if (auto v = pick("(5)", "5"); !v.empty()) return v;
    if (auto v = pick("(6)", "6"); !v.empty()) return v;
    if (auto v = pick("(Z)", "Z"); !v.empty()) return v;
    if (auto v = pick("(X)", "X"); !v.empty()) return v;
    if (auto v = pick("(C)", "C"); !v.empty()) return v;
    if (auto v = pick("(Q)", "Q"); !v.empty()) return v;
    return raw;
}

std::string NormalizeDebugHotkeyName(const std::string& raw) {
    std::string hotkey = NormalizeHotkeyName(raw);
    std::transform(hotkey.begin(), hotkey.end(), hotkey.begin(), [](unsigned char ch) {
        return static_cast<char>(std::toupper(ch));
    });
    return hotkey;
}

bool IsAutoDebugHotkey(const std::string& hotkey) {
    const std::string normalized = NormalizeDebugHotkeyName(hotkey);
    return normalized == "6" || normalized == "Z" || normalized == "X" || normalized == "C" || normalized == "Q";
}

const SkillConfig* GetConfig(const TrackedSkillState& s) {
    const auto& table = GetSkillTable();
    return s.configIndex < table.size() ? &table[s.configIndex] : nullptr;
}

const SkillConfig* FindConfigByArchiveKey(const std::string& key) {
    const auto& table = GetSkillTable();
    for (const auto& c : table) {
        if (std::find(c.archiveKeys.begin(), c.archiveKeys.end(), key) != c.archiveKeys.end()) return &c;
    }
    return nullptr;
}

const SkillConfig* FindConfigByHotkey(const std::string& hotkey) {
    const auto& table = GetSkillTable();
    for (const auto& c : table) {
        if (c.hotkeyName == hotkey) return &c;
    }
    return nullptr;
}

TrackedSkillState* FindStateByHotkey(const std::string& hotkey) {
    for (auto& s : g_skillStates) {
        auto* c = GetConfig(s);
        if (c && c->hotkeyName == hotkey) return &s;
    }
    return nullptr;
}

bool IsCastable(const TrackedSkillState& s, unsigned long long now) {
    (void)now;
    return s.nodeState.stateText == "NODE_READY" ||
           s.nodeState.stateText == "READY_BASELINE";
}

bool IsFull(const TrackedSkillState& s, unsigned long long now) {
    (void)now;
    return s.nodeState.stateText == "NODE_READY" ||
           s.nodeState.stateText == "READY_BASELINE";
}

void LogEvent(const char* type, const TrackedSkillState& s, const std::string& details) {
    auto* c = GetConfig(s);
    if (!c) return;
    std::ostringstream oss;
    oss << "[" << BuildTimestampForLog() << "] [" << std::left << std::setw(8) << type << "] "
        << std::setw(20) << c->skillName << " (" << c->hotkeyName << ") | ";
    switch (c->cdType) {
    case kx::SkillDb::SkillCdType::STANDARD: oss << std::setw(15) << "STANDARD"; break;
    case kx::SkillDb::SkillCdType::MULTI_CHARGE: oss << std::setw(15) << "MULTI_CHARGE"; break;
    case kx::SkillDb::SkillCdType::CHARGE_PLUS_CD: oss << std::setw(15) << "CHARGE_PLUS_CD"; break;
    }
    oss << " | " << details;
    AppendLogLine(oss.str());
}

PacketUseEvent SnapshotFreshUseEvent() {
    PacketUseEvent e{};
    int hotkeyId = kx::g_lastSkillHotkey.load(std::memory_order_acquire);
    auto hotkeyTick = kx::g_lastSkillHotkeyTickMs.load(std::memory_order_acquire);
    std::string hotkeyName = NormalizeHotkeyName(kx::GetSkillHotkeyName(hotkeyId));

    std::lock_guard<std::mutex> lock(kx::g_lastSkillUsePacketMutex);
    const auto& c = kx::g_lastSkillUsePacket;
    if (!c.valid || c.tickMs == 0 || c.tickMs == g_lastProcessedPacketTickMs || hotkeyTick == 0 || c.tickMs < hotkeyTick) {
        return e;
    }

    e.archiveKey = BuildArchiveKey(c);
    if (!ShouldTrackPacket(e.archiveKey)) return e;
    e.hotkeyName = hotkeyName;
    e.hotkeyId = hotkeyId;
    e.packetTickMs = c.tickMs;
    return e;
}

std::optional<uint32_t> TryMapTrackedSlot(const PacketUseEvent& e) {
    if (e.archiveKey == "tag29_val00") return 0; // Healing(6)
    if (e.archiveKey == "tag29_val01") return 1; // Utility1(Z)
    if (e.archiveKey == "tag25_val83") return 2; // Utility2(X)
    if (e.archiveKey == "tag29_val04") return 4; // Elite(Q)
    if (e.archiveKey == "tag29_val0c") return 5; // Profession1(F1), provisional

    if (e.hotkeyName == "6") return 0;
    if (e.hotkeyName == "Z") return 1;
    if (e.hotkeyName == "X") return 2;
    if (e.hotkeyName == "Q") return 4;
    return std::nullopt;
}

std::optional<uint32_t> TryMapProbeSlot(const PacketUseEvent& e) {
    if (auto mapped = TryMapTrackedSlot(e); mapped.has_value()) {
        return mapped;
    }

    if (e.hotkeyName == "C") return 3;
    return std::nullopt;
}

uint32_t ReadLe32FromBytes(const uint8_t* data, size_t offset);
std::string BuildChangedCandidateSummary(const uint8_t* before,
                                         const uint8_t* after,
                                         size_t size,
                                         const char* label);

const char* GetSkillbarSlotLabel(size_t slotIndex) {
    static const char* kLabels[] = {
        "6", "Z", "X", "C", "Q", "F1", "F2", "F3", "F4", "F5"
    };
    return slotIndex < std::size(kLabels) ? kLabels[slotIndex] : "?";
}

bool IsReadyLikeNodeState(const std::string& stateText) {
    return stateText == "NODE_READY" || stateText == "READY_BASELINE";
}

bool IsCooldownLikeNodeState(const std::string& stateText) {
    return stateText == "NODE_CD" ||
           stateText == "CD_STATE" ||
           stateText == "ALT40_READY60" ||
           stateText == "ALT40_ALT60_A" ||
           stateText == "ALT40_ALT60_B";
}

std::string BuildSkillbarSlotLine(const SkillbarSnapshot& snap,
                                  size_t slotIndex,
                                  const std::vector<kx::SkillTrace::LiveSkillbarRow>& liveRows) {
    if (slotIndex >= snap.slots.size()) {
        return {};
    }

    const auto& slot = snap.slots[slotIndex];
    std::ostringstream oss;
    oss << "slot=" << slotIndex
        << " hotkey=" << GetSkillbarSlotLabel(slotIndex)
        << " addr=0x" << std::hex << slot.slotAddr
        << std::dec;

    if (!slot.valid) {
        oss << " valid=0";
        return oss.str();
    }

    const uint32_t u00 = ReadLe32FromBytes(slot.rawBytes, 0x00);
    const uint32_t u04 = ReadLe32FromBytes(slot.rawBytes, 0x04);
    const uint32_t u08 = ReadLe32FromBytes(slot.rawBytes, 0x08);
    const uint32_t u0C = ReadLe32FromBytes(slot.rawBytes, 0x0C);
    const uint32_t u10 = ReadLe32FromBytes(slot.rawBytes, 0x10);
    const uint32_t u14 = ReadLe32FromBytes(slot.rawBytes, 0x14);
    const uint32_t u18 = ReadLe32FromBytes(slot.rawBytes, 0x18);
    const uint32_t u1C = ReadLe32FromBytes(slot.rawBytes, 0x1C);
    const uint8_t  b20 = slot.rawBytes[0x20];
    const uint64_t q00 =
        static_cast<uint64_t>(ReadLe32FromBytes(slot.rawBytes, 0x00)) |
        (static_cast<uint64_t>(ReadLe32FromBytes(slot.rawBytes, 0x04)) << 32);
    const uint64_t q08 =
        static_cast<uint64_t>(ReadLe32FromBytes(slot.rawBytes, 0x08)) |
        (static_cast<uint64_t>(ReadLe32FromBytes(slot.rawBytes, 0x0C)) << 32);

    oss << " u32[00]=" << u00
        << " u32[04]=" << u04
        << " u32[08]=" << u08
        << " u32[0C]=" << u0C
        << " u32[10]=" << u10
        << " u32[14]=" << u14
        << " u32[18]=" << u18
        << " u32[1C]=" << u1C
        << " b20=0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(b20)
        << " q00=0x" << std::setw(16) << q00
        << " q08=0x" << std::setw(16) << q08
        << std::dec;

    const std::string resolvedName = kx::SkillDb::ResolveSkillName(static_cast<int>(u00));
    if (u00 > 0 && !resolvedName.empty() && resolvedName != "Unknown Skill") {
        oss << " name=\"" << resolvedName << "\"";
    }

    for (const auto& row : liveRows) {
        if (row.slotIndex != static_cast<int>(slotIndex)) {
            continue;
        }
        oss << " live_id=" << row.apiId
            << " live_name=\"" << row.skillName << "\""
            << " live_src=" << row.source
            << " live_conf=" << row.confidence;
        break;
    }

    return oss.str();
}

std::optional<uint32_t> ResolveFocusSlot(const SkillbarSnapshot& snap, std::optional<uint32_t> mappedSlot) {
    if (mappedSlot.has_value() && mappedSlot.value() < snap.slots.size()) {
        return mappedSlot;
    }
    if (snap.focusSlotIndex < snap.slots.size()) {
        return snap.focusSlotIndex;
    }
    return std::nullopt;
}

void LogBridgeWatchExtractionLocked(const char* phaseName,
                                    const PacketUseEvent& e,
                                    const SkillbarSnapshot& snap,
                                    std::optional<uint32_t> mappedSlot) {
    const auto resolvedSlot = ResolveFocusSlot(snap, mappedSlot);
    const uintptr_t focusSlotAddr =
        resolvedSlot.has_value() && resolvedSlot.value() < snap.slots.size()
            ? snap.slots[resolvedSlot.value()].slotAddr
            : 0;
    const uintptr_t ptr40Slot = snap.charSkillbarAddr ? (snap.charSkillbarAddr + 0x40) : 0;
    const uintptr_t ptr60Slot = snap.charSkillbarAddr ? (snap.charSkillbarAddr + 0x60) : 0;

    uint64_t focusSlotValue = 0;
    uint64_t ptr40Value = 0;
    uint64_t ptr60Value = 0;
    const bool haveFocusSlotValue = ReadQwordSafe(focusSlotAddr, focusSlotValue);
    const bool havePtr40Value = ReadQwordSafe(ptr40Slot, ptr40Value);
    const bool havePtr60Value = ReadQwordSafe(ptr60Slot, ptr60Value);

    if (haveFocusSlotValue) {
        RememberWatchValue(focusSlotAddr, focusSlotValue);
    }
    if (havePtr40Value) {
        RememberWatchValue(ptr40Slot, ptr40Value);
    }
    if (havePtr60Value) {
        RememberWatchValue(ptr60Slot, ptr60Value);
    }

    {
        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [SKWATCH] "
            << "run_id=" << e.runId
            << " focus_slot=";
        if (resolvedSlot.has_value()) {
            oss << resolvedSlot.value();
        } else {
            oss << "na";
        }
        oss << " hotkey=" << e.hotkeyName
            << " phase=" << (phaseName ? phaseName : "?")
            << " focus_slot_source="
            << (mappedSlot.has_value() ? "mapped" : (snap.focusSlotIndex < snap.slots.size() ? "pressed_skill" : "na"))
            << " ptr48_raw=0x" << std::hex << snap.ptr48Addr;
        AppendLogLine(oss.str());
    }

    {
        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [SKWATCH] "
            << "focus_slot_addr=0x" << std::hex << focusSlotAddr
            << " focus_slot_value=";
        if (haveFocusSlotValue) {
            oss << "0x" << focusSlotValue;
        } else {
            oss << "<invalid>";
        }
        AppendLogLine(oss.str());
    }

    {
        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [SKWATCH] "
            << "ptr40_slot=0x" << std::hex << ptr40Slot
            << " ptr40_value=";
        if (havePtr40Value) {
            oss << "0x" << ptr40Value;
        } else {
            oss << "<invalid>";
        }
        AppendLogLine(oss.str());
    }

    {
        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [SKWATCH] "
            << "ptr60_slot=0x" << std::hex << ptr60Slot
            << " ptr60_value=";
        if (havePtr60Value) {
            oss << "0x" << ptr60Value;
        } else {
            oss << "<invalid>";
        }
        AppendLogLine(oss.str());
    }
}

void LogSkillbarMemorySnapshotLocked(const char* phaseName,
                                     const PacketUseEvent& e,
                                     const SkillbarSnapshot& snap,
                                     const SkillbarSnapshot* baseline) {
    if (!kEnableSkillbarMemoryWatch || !snap.valid) {
        return;
    }

    const auto mappedSlot = TryMapProbeSlot(e);
    const auto resolvedSlot = ResolveFocusSlot(snap, mappedSlot);
    const bool logAllSlots =
        !IsBridgeOnlyModeEnabled() &&
        (std::strcmp(phaseName, "t0") == 0 ||
         std::strcmp(phaseName, "node_ready") == 0);
    const auto liveRows = kx::SkillTrace::GetLiveSkillbarRows();

    {
        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [SKMEM   ] "
            << "phase=" << phaseName
            << " run_id=" << e.runId
            << " hotkey=" << e.hotkeyName
            << " focus_slot=";
        if (resolvedSlot.has_value()) {
            oss << resolvedSlot.value();
        } else {
            oss << "na";
        }
        oss << " ctx=0x" << std::hex << snap.ctxAddr
            << " chcli_ctx=0x" << snap.chCliContextAddr
            << " local=0x" << snap.localCharAddr
            << " chcli_skillbar=0x" << snap.chCliSkillbarAddr
            << " char_skillbar=0x" << snap.charSkillbarAddr
            << " ptr40=0x" << snap.ptr40Addr
            << " ptr48=0x" << snap.ptr48Addr
            << " ptr60=0x" << snap.ptr60Addr
            << std::dec;
        AppendLogLine(oss.str());
    }

    LogBridgeWatchExtractionLocked(phaseName, e, snap, mappedSlot);

    if (logAllSlots) {
        for (size_t slotIndex = 0; slotIndex < snap.slots.size(); ++slotIndex) {
            AppendLogLine("[" + BuildTimestampForLog() + "] [SKMEM   ] " +
                          std::string("phase=") + phaseName + " " +
                          BuildSkillbarSlotLine(snap, slotIndex, liveRows));
        }
    } else if (resolvedSlot.has_value() && resolvedSlot.value() < snap.slots.size()) {
        AppendLogLine("[" + BuildTimestampForLog() + "] [SKMEM   ] " +
                      std::string("phase=") + phaseName + " " +
                      BuildSkillbarSlotLine(snap, resolvedSlot.value(), liveRows));
    }

    if (IsBridgeOnlyModeEnabled()) {
        return;
    }

    if (!baseline || !baseline->valid) {
        return;
    }

    AppendLogLine("[" + BuildTimestampForLog() + "] [SKMEM   ] phase=" + std::string(phaseName) + " " +
                  BuildChangedCandidateSummary(baseline->rawCharSkillbarWide,
                                               snap.rawCharSkillbarWide,
                                               sizeof(snap.rawCharSkillbarWide),
                                               "char_skillbar_wide"));

    if (resolvedSlot.has_value() && resolvedSlot.value() < snap.slots.size() &&
        resolvedSlot.value() < baseline->slots.size()) {
        const auto& beforeSlot = baseline->slots[resolvedSlot.value()];
        const auto& afterSlot = snap.slots[resolvedSlot.value()];
        AppendLogLine("[" + BuildTimestampForLog() + "] [SKMEM   ] phase=" + std::string(phaseName) + " " +
                      BuildChangedCandidateSummary(beforeSlot.rawBytes,
                                                   afterSlot.rawBytes,
                                                   sizeof(afterSlot.rawBytes),
                                                   "focus_slot"));
    }
}

bool InitInternalPollTarget(InternalPollTarget& target,
                            PtrWriteHitKind kind,
                            const char* label,
                            uintptr_t addr) {
    target.kind = kind;
    target.label = label ? label : "";
    target.addr = addr;
    target.lastValue = 0;
    target.valid = addr != 0 && ReadQwordSafe(addr, target.lastValue);
    if (target.valid) {
        RememberWatchValue(addr, target.lastValue);
    }
    return target.valid;
}

void UpdateInternalPollN48TargetsLocked() {
    auto& watcher = g_internalPollingWatcher;
    if (!watcher.active) {
        return;
    }

    const uintptr_t cur40 = watcher.targets[0].valid ? UntagNodePointer(watcher.targets[0].lastValue) : 0;
    const uintptr_t cur60 = watcher.targets[1].valid ? UntagNodePointer(watcher.targets[1].lastValue) : 0;
    InitInternalPollTarget(watcher.targets[2], PtrWriteHitKind::N48, "N48_CUR40", cur40 ? (cur40 + 0x48) : 0);
    InitInternalPollTarget(watcher.targets[3], PtrWriteHitKind::N48, "N48_CUR60", cur60 ? (cur60 + 0x48) : 0);
}

void StartInternalPollingWatcherLocked(const PacketUseEvent& e, const SkillbarSnapshot& baselineSnapshot) {
    g_internalPollingWatcher = {};
    if (!baselineSnapshot.valid || !baselineSnapshot.charSkillbarAddr) {
        return;
    }

    auto& watcher = g_internalPollingWatcher;
    const unsigned long long now = GetTickCount64();
    watcher.active = true;
    watcher.runId = g_cooldownPacketWatch.runId;
    watcher.hotkeyName = e.hotkeyName;
    if (const auto* cfg = FindConfigByHotkey(e.hotkeyName)) {
        watcher.skillId = cfg->apiId;
    }
    watcher.startedTickMs = now;
    watcher.untilTickMs = now + kInternalPollWindowMs;
    watcher.lastPollTickMs = 0;
    watcher.slot = TryMapProbeSlot(e).value_or(UINT32_MAX);
    watcher.durationMs = ResolveEstimatedDurationMsForHotkey(e.hotkeyName);

    InitInternalPollTarget(watcher.targets[0],
                           PtrWriteHitKind::PTR40,
                           "PTR40",
                           baselineSnapshot.charSkillbarAddr + 0x40);
    InitInternalPollTarget(watcher.targets[1],
                           PtrWriteHitKind::PTR60,
                           "PTR60",
                           baselineSnapshot.charSkillbarAddr + 0x60);
    watcher.baselinePtr40 = watcher.targets[0].valid ? watcher.targets[0].lastValue : 0;
    watcher.baselinePtr60 = watcher.targets[1].valid ? watcher.targets[1].lastValue : 0;
    UpdateInternalPollN48TargetsLocked();

    CooldownInfoLike& info = GetOrCreateCooldownInfoForWatcherLocked(watcher);
    info.cdPtr40 = 0;
    info.cdPtr60 = 0;
    info.cdStartTickMs = 0;
    info.observedReady = false;
    info.observedReadyTickMs = 0;
    info.observedDurationMs = 0;

    g_estimatedCooldownState.available = true;
    g_estimatedCooldownState.estimated = true;
    g_estimatedCooldownState.hotkey = e.hotkeyName;
    g_estimatedCooldownState.skillName = "Unknown Skill";
    g_estimatedCooldownState.durationMs = watcher.durationMs;
    g_estimatedCooldownState.cdStartTickMs = 0;
    g_estimatedCooldownState.cooldownActive = false;
    g_estimatedCooldownState.ptr40Value = watcher.targets[0].valid ? watcher.targets[0].lastValue : 0;
    g_estimatedCooldownState.ptr60Value = watcher.targets[1].valid ? watcher.targets[1].lastValue : 0;
    const auto& table = kx::SkillDb::GetSkillTable();
    for (const auto& cfg : table) {
        if (cfg.hotkeyName == e.hotkeyName) {
            g_estimatedCooldownState.skillName = cfg.skillName;
            break;
        }
    }

    std::ostringstream oss;
    oss << "[" << BuildTimestampForLog() << "] [SKWATCH] "
        << "internal watcher start"
        << " run_id=" << watcher.runId
        << " watches=" << watcher.targets.size()
        << " interval_ms=" << kInternalPollIntervalMs
        << " window_ms=" << kInternalPollWindowMs;
    AppendLogLine(oss.str());
}

void StopInternalPollingWatcherLocked() {
    g_internalPollingWatcher = {};
}

void ProcessInternalPollingWatcherLocked(unsigned long long now) {
    auto& watcher = g_internalPollingWatcher;
    if (!watcher.active) {
        return;
    }
    if (!g_cooldownPacketWatch.active || watcher.runId != g_cooldownPacketWatch.runId || now >= watcher.untilTickMs) {
        StopInternalPollingWatcherLocked();
        return;
    }
    if (watcher.lastPollTickMs != 0 && now < watcher.lastPollTickMs + kInternalPollIntervalMs) {
        return;
    }
    watcher.lastPollTickMs = now;

    for (size_t i = 0; i < watcher.targets.size(); ++i) {
        auto& target = watcher.targets[i];
        if (!target.valid || !target.addr) {
            continue;
        }

        uint64_t newValue = 0;
        if (!ReadQwordSafe(target.addr, newValue)) {
            target.valid = false;
            continue;
        }
        if (newValue == target.lastValue) {
            continue;
        }

        const uint64_t oldValue = target.lastValue;
        target.lastValue = newValue;
        ++watcher.totalHitCount;
        if (watcher.totalHitCount > 512) {
            std::ostringstream guard;
            guard << "[" << BuildTimestampForLog() << "] [SKWATCH] "
                  << "internal watcher stop run_id=" << watcher.runId
                  << " reason=hit_storm"
                  << " total_hits=" << watcher.totalHitCount;
            AppendLogLine(guard.str());
            StopInternalPollingWatcherLocked();
            return;
        }
        if (target.kind == PtrWriteHitKind::PTR40) {
            g_estimatedCooldownState.ptr40Value = newValue;
        } else if (target.kind == PtrWriteHitKind::PTR60) {
            g_estimatedCooldownState.ptr60Value = newValue;
        }

        uint32_t phase = 2;
        if (target.kind == PtrWriteHitKind::PTR40 || target.kind == PtrWriteHitKind::PTR60) {
            const bool readyBaselineState = IsReadyBaselineStateForWatcherLocked(watcher);
            const bool baselinePtrMatch =
                (target.kind == PtrWriteHitKind::PTR40 && watcher.baselinePtr40 != 0 && newValue == watcher.baselinePtr40) ||
                (target.kind == PtrWriteHitKind::PTR60 && watcher.baselinePtr60 != 0 && newValue == watcher.baselinePtr60);
            if (watcher.readyToCooldownObserved && (readyBaselineState || baselinePtrMatch)) {
                phase = 3;
            } else {
            phase = CurrentInternalPollPhaseCodeLocked();
            }
        }
        RecordPtrWriteHitLocked(target.kind,
                                target.addr,
                                true,
                                oldValue,
                                newValue,
                                0,
                                watcher.slot,
                                phase,
                                "internal_poll");

        if (target.kind == PtrWriteHitKind::PTR40 || target.kind == PtrWriteHitKind::PTR60) {
            std::ostringstream hit;
            hit << "[" << BuildTimestampForLog() << "] [SKHIT] "
                << "run_id=" << watcher.runId
                << " kind=" << target.label
                << " hit_addr=0x" << std::hex << target.addr
                << " old=0x" << oldValue
                << " new=0x" << newValue
                << " rip=0x0"
                << std::dec
                << " slot=";
            if (watcher.slot == UINT32_MAX) {
                hit << "na";
            } else {
                hit << watcher.slot;
            }
            hit << " phase=" << FormatBridgePhase(phase)
                << " source=internal_poll";
            AppendLogLine(hit.str());
        }

        if (target.kind == PtrWriteHitKind::PTR40 || target.kind == PtrWriteHitKind::PTR60) {
            if (phase == 2) {
                if (target.kind == PtrWriteHitKind::PTR40) {
                    watcher.ptr40NodeCdSeen = true;
                } else {
                    watcher.ptr60NodeCdSeen = true;
                }
                if (!watcher.readyToCooldownObserved &&
                    watcher.ptr40NodeCdSeen &&
                    watcher.ptr60NodeCdSeen) {
                    watcher.readyToCooldownObserved = true;
                    watcher.cdStartTickMs = now;
                    const unsigned long long extendedUntil =
                        watcher.cdStartTickMs +
                        static_cast<unsigned long long>((std::max)(watcher.durationMs, 0)) +
                        3000ULL;
                    watcher.untilTickMs = (std::max)(watcher.untilTickMs, extendedUntil);
                    g_cooldownPacketWatch.untilTickMs = (std::max)(g_cooldownPacketWatch.untilTickMs, extendedUntil);
                    CooldownInfoLike& info = GetOrCreateCooldownInfoForWatcherLocked(watcher);
                    info.cdStartTickMs = now;
                    info.cdPtr40 = watcher.targets[0].lastValue;
                    info.cdPtr60 = watcher.targets[1].lastValue;
                    info.durationMs = watcher.durationMs;
                    g_estimatedCooldownState.cdStartTickMs = now;
                    g_estimatedCooldownState.cooldownActive = true;
                    g_estimatedCooldownState.estimated = true;
                    g_cooldownPacketWatch.loggedCooldownStart = true;
                    g_cooldownPacketWatch.haveNodeCdOffset = true;
                    g_cooldownPacketWatch.nodeCdOffsetMs =
                        now > g_cooldownPacketWatch.startedTickMs
                            ? (now - g_cooldownPacketWatch.startedTickMs)
                            : 0;
                }
            } else if (phase == 3 && watcher.readyToCooldownObserved) {
                if (target.kind == PtrWriteHitKind::PTR40) {
                    watcher.ptr40NodeReadySeen = true;
                } else {
                    watcher.ptr60NodeReadySeen = true;
                }
                if (!watcher.cooldownToReadyObserved &&
                    watcher.ptr40NodeReadySeen &&
                    watcher.ptr60NodeReadySeen) {
                    watcher.cooldownToReadyObserved = true;
                    g_estimatedCooldownState.cooldownActive = false;
                    const unsigned long long observedDurationMs =
                        watcher.cdStartTickMs != 0 && now >= watcher.cdStartTickMs
                            ? (now - watcher.cdStartTickMs)
                            : 0ULL;
                    g_estimatedCooldownState.estimated = false;
                    if (observedDurationMs > 0) {
                        g_estimatedCooldownState.durationMs = static_cast<int>((std::min)(
                            observedDurationMs,
                            static_cast<unsigned long long>((std::numeric_limits<int>::max)())));
                    }
                    CooldownInfoLike& info = GetOrCreateCooldownInfoForWatcherLocked(watcher);
                    info.observedReady = true;
                    info.observedReadyTickMs = now;
                    info.observedDurationMs = observedDurationMs;
                    g_cooldownPacketWatch.loggedReadyReturn = true;
                    g_cooldownPacketWatch.haveNodeReadyOffset = true;
                    g_cooldownPacketWatch.nodeReadyOffsetMs =
                        now > g_cooldownPacketWatch.startedTickMs
                            ? (now - g_cooldownPacketWatch.startedTickMs)
                            : 0;
                    std::ostringstream cdtrack;
                    cdtrack << "[" << BuildTimestampForLog() << "] [CDTRACK ] "
                            << "slot=";
                    if (info.slot == UINT32_MAX) {
                        cdtrack << "na";
                    } else {
                        cdtrack << info.slot;
                    }
                    cdtrack << " hotkey=" << info.hotkey
                            << " observed_ready_ms=" << g_cooldownPacketWatch.nodeReadyOffsetMs
                            << " observed_duration_ms=" << info.observedDurationMs;
                    AppendLogLine(cdtrack.str());
                }
            }
        }

        if (watcher.readyToCooldownObserved &&
            watcher.cooldownToReadyObserved &&
            !watcher.transitionSummaryLogged) {
            watcher.transitionSummaryLogged = true;
            std::ostringstream cmp;
            cmp << "[" << BuildTimestampForLog() << "] [PTRCMP  ] "
                << "run_id=" << watcher.runId
                << " ready->cooldown and cooldown->ready both observed";
            AppendLogLine(cmp.str());
            StopInternalPollingWatcherLocked();
            return;
        }

        if (i == 0 || i == 1) {
            UpdateInternalPollN48TargetsLocked();
        }
    }
}

std::string BuildChangedCandidateSummary(const uint8_t* before,
                                         const uint8_t* after,
                                         size_t size,
                                         const char* label) {
    std::ostringstream oss;
    oss << label << ": ";

    int emitted = 0;
    for (size_t offset = 0; offset + 4 <= size && emitted < 8; offset += 4) {
        const uint32_t before32 = static_cast<uint32_t>(before[offset]) |
                                  (static_cast<uint32_t>(before[offset + 1]) << 8) |
                                  (static_cast<uint32_t>(before[offset + 2]) << 16) |
                                  (static_cast<uint32_t>(before[offset + 3]) << 24);
        const uint32_t after32 = static_cast<uint32_t>(after[offset]) |
                                 (static_cast<uint32_t>(after[offset + 1]) << 8) |
                                 (static_cast<uint32_t>(after[offset + 2]) << 16) |
                                 (static_cast<uint32_t>(after[offset + 3]) << 24);
        if (before32 == after32) {
            continue;
        }
        if (emitted > 0) {
            oss << " | ";
        }
        oss << "u32@0x" << std::hex << offset
            << "=" << before32 << "->" << after32;
        ++emitted;
    }

    if (emitted == 0) {
        oss << "no changed u32 fields";
    }
    return oss.str();
}

std::string FormatSweepSummary(const SkillRechargeSweepResult& sweep) {
    std::ostringstream oss;
    for (size_t i = 0; i < sweep.slotCount; ++i) {
        const auto& entry = sweep.slots[i];
        if (i > 0) {
            oss << ",";
        }
        oss << i << ":";
        if (!entry.executed) {
            oss << (entry.callFaulted ? "fault" : "na");
        } else {
            oss << (entry.returnedValue ? "1" : "0");
        }
    }
    return oss.str();
}

std::string FormatSweepEntry(const SkillRechargeSweepResult& sweep, uint32_t slot) {
    if (slot >= sweep.slotCount) {
        return "na";
    }
    const auto& entry = sweep.slots[slot];
    if (!entry.executed) {
        return entry.callFaulted ? "fault" : "na";
    }
    return entry.returnedValue ? "1" : "0";
}

uint32_t ReadLe32FromBytes(const uint8_t* data, size_t offset) {
    return static_cast<uint32_t>(data[offset]) |
           (static_cast<uint32_t>(data[offset + 1]) << 8) |
           (static_cast<uint32_t>(data[offset + 2]) << 16) |
           (static_cast<uint32_t>(data[offset + 3]) << 24);
}

float ReadFloat32FromBytes(const uint8_t* data, size_t offset) {
    const uint32_t raw = ReadLe32FromBytes(data, offset);
    float value = 0.0f;
    std::memcpy(&value, &raw, sizeof(value));
    return value;
}

std::optional<uint32_t> GetFindingsValidationSlotForHotkey(const std::string& hotkeyName) {
    if (hotkeyName == "6") {
        return 0;
    }
    if (hotkeyName == "C") {
        return 3;
    }
    return std::nullopt;
}

std::string BuildFindingsAbilityGuessSummary(const SkillbarSnapshot& snap, const std::string& hotkeyName) {
    const auto guessedSlot = GetFindingsValidationSlotForHotkey(hotkeyName);
    if (!guessedSlot.has_value()) {
        return {};
    }

    std::ostringstream oss;
    oss << " findings_guess=";

    const uint32_t slotIndex = guessedSlot.value();
    if (slotIndex >= snap.slots.size()) {
        oss << "slot_out_of_range(slot=" << slotIndex << ")";
        return oss.str();
    }

    const auto& slot = snap.slots[slotIndex];
    oss << "slot=" << slotIndex
        << " addr=0x" << std::hex << slot.slotAddr;
    if (!slot.valid) {
        oss << " invalid";
        return oss.str();
    }

    const uint32_t id = ReadLe32FromBytes(slot.rawBytes, 0x00);
    const uint32_t ammo = ReadLe32FromBytes(slot.rawBytes, 0x04);
    const uint32_t lastUpdate = ReadLe32FromBytes(slot.rawBytes, 0x08);
    const float rechargeRate = ReadFloat32FromBytes(slot.rawBytes, 0x0C);
    const uint32_t recharge = ReadLe32FromBytes(slot.rawBytes, 0x10);
    const uint32_t rechargeRemaining = ReadLe32FromBytes(slot.rawBytes, 0x14);
    const uint32_t ammoRecharge = ReadLe32FromBytes(slot.rawBytes, 0x18);
    const uint32_t ammoRechargeRemaining = ReadLe32FromBytes(slot.rawBytes, 0x1C);
    const uint8_t state = slot.rawBytes[0x20];

    oss << std::dec
        << " id=" << id
        << " ammo=" << ammo
        << " last_update=" << lastUpdate
        << " recharge_rate=" << std::fixed << std::setprecision(3) << rechargeRate
        << " recharge=" << recharge
        << " recharge_remaining=" << rechargeRemaining
        << " ammo_recharge=" << ammoRecharge
        << " ammo_recharge_remaining=" << ammoRechargeRemaining
        << " state=0x" << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(state)
        << " raw24=";

    for (size_t i = 0; i < 0x24 && i < sizeof(slot.rawBytes); ++i) {
        if (i > 0) {
            oss << ".";
        }
        oss << std::setw(2) << static_cast<unsigned>(slot.rawBytes[i]);
    }

    return oss.str();
}

bool ShouldLogFindingsHeapDumpPhase(const char* phaseName) {
    if (!phaseName) {
        return false;
    }
    return std::strcmp(phaseName, "t0") == 0 ||
           std::strcmp(phaseName, "+1000ms") == 0 ||
           std::strcmp(phaseName, "+10000ms") == 0 ||
           std::strcmp(phaseName, "ready-500ms") == 0 ||
           std::strcmp(phaseName, "ready+250ms") == 0;
}

bool IsLikelyHeapPointer(uint64_t value) {
    return value >= 0x100000000000ULL && value < 0x200000000000ULL;
}

std::optional<uint32_t> GetExpectedCooldownMsForTsScan(const PacketUseEvent& e) {
    if (e.hotkeyName == "C") {
        return 20000u;
    }
    return std::nullopt;
}

void LogCooldownEndCandidateScan(const char* phaseName,
                                 const PacketUseEvent& e,
                                 const char* label,
                                 const uint8_t* base,
                                 uint32_t internalNow,
                                 size_t scanBytes = 0x100) {
    if (!base) {
        return;
    }

    const auto expectedCdMs = GetExpectedCooldownMsForTsScan(e);
    if (!expectedCdMs.has_value()) {
        return;
    }
    if (!internalNow) {
        return;
    }

    const uint64_t cdEndExpected = static_cast<uint64_t>(internalNow) + expectedCdMs.value();
    const uint64_t tolerance = 1000;
    const uint64_t lo = cdEndExpected > tolerance ? (cdEndExpected - tolerance) : 0;
    const uint64_t hi = cdEndExpected + tolerance;

    {
        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [TSSCAN  ] "
            << "phase=" << phaseName
            << " hotkey=" << e.hotkeyName
            << " archive_key=" << e.archiveKey
            << " label=" << label
            << " internal_now=" << internalNow
            << " cd_ms=" << expectedCdMs.value()
            << " expect_cd_end=" << cdEndExpected
            << " range=[" << lo << "," << hi << "]";
        AppendLogLine(oss.str());
    }

    const size_t clampedBytes = std::min<size_t>(scanBytes, 0x100);
    for (size_t i = 0; i + 4 <= clampedBytes; i += 4) {
        const uint32_t v32 = ReadLe32FromBytes(base, i);
        if (static_cast<uint64_t>(v32) >= lo && static_cast<uint64_t>(v32) <= hi) {
            std::ostringstream oss;
            oss << "[" << BuildTimestampForLog() << "] [TSSCAN  ] "
                << "HIT u32 label=" << label
                << " +" << std::hex << std::setw(2) << std::setfill('0') << i
                << std::dec
                << " = " << v32
                << " delta=" << (static_cast<int64_t>(v32) - static_cast<int64_t>(cdEndExpected)) << "ms";
            AppendLogLine(oss.str());
        }

        if (i + 8 <= clampedBytes) {
            uint64_t v64 = 0;
            std::memcpy(&v64, base + i, sizeof(v64));
            if (v64 >= lo && v64 <= hi) {
                std::ostringstream oss;
                oss << "[" << BuildTimestampForLog() << "] [TSSCAN  ] "
                    << "HIT u64 label=" << label
                    << " +" << std::hex << std::setw(2) << std::setfill('0') << i
                    << std::dec
                    << " = " << v64
                    << " delta=" << (static_cast<int64_t>(v64) - static_cast<int64_t>(cdEndExpected)) << "ms";
                AppendLogLine(oss.str());
            }
        }
    }
}

bool ShouldCaptureChCliPhase(const char* phaseName) {
    return std::strcmp(phaseName, "t0") == 0 ||
           std::strcmp(phaseName, "+1000ms") == 0 ||
           std::strcmp(phaseName, "+10000ms") == 0;
}

bool ShouldLogHeapFieldOffset(const char* label, size_t offset) {
    if (std::strcmp(label, "ptr48+0x20") == 0) {
        switch (offset) {
        case 0x00:
        case 0x04:
        case 0x10:
        case 0x14:
        case 0x20:
        case 0x24:
        case 0x28:
        case 0x40:
        case 0x44:
            return true;
        default:
            return offset >= 0x60 && offset <= 0x90 && (offset % 4 == 0);
        }
    }
    return offset < 0x60 && (offset % 4 == 0);
}

void LogChCliDiffSummary() {
    if (!g_cooldownPacketWatch.active || g_cooldownPacketWatch.dumpedChCliDiff) {
        return;
    }

    const TimedChCliSnapshot* t0 = nullptr;
    const TimedChCliSnapshot* p1 = nullptr;
    const TimedChCliSnapshot* p10 = nullptr;
    for (const auto& snap : g_chCliSnapshots) {
        if (snap.phaseName == "t0") {
            t0 = &snap;
        } else if (snap.phaseName == "+1000ms") {
            p1 = &snap;
        } else if (snap.phaseName == "+10000ms") {
            p10 = &snap;
        }
    }
    if (!t0 || !p1 || !p10) {
        return;
    }

    std::ostringstream header;
    header << "[" << BuildTimestampForLog() << "] [CHCLIDIFF] "
           << "hotkey=" << g_cooldownPacketWatch.hotkeyName
           << " archive_key=" << g_cooldownPacketWatch.archiveKey
           << " phases=t0,+1000ms,+10000ms"
           << " addr=0x" << std::hex << t0->addr;
    AppendLogLine(header.str());

    bool anyU32 = false;
    for (size_t i = 0; i + 4 <= t0->bytes.size(); i += 4) {
        const uint32_t v0 = ReadLe32FromBytes(t0->bytes.data(), i);
        const uint32_t v1 = ReadLe32FromBytes(p1->bytes.data(), i);
        const uint32_t v2 = ReadLe32FromBytes(p10->bytes.data(), i);
        if (v0 == v1 && v1 == v2) {
            continue;
        }
        anyU32 = true;
        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [CHCLIDIFF] "
            << "u32 +" << std::hex << std::setw(2) << std::setfill('0') << i
            << std::dec
            << " t0=" << v0
            << " +1000ms=" << v1
            << " +10000ms=" << v2;
        AppendLogLine(oss.str());
    }
    if (!anyU32) {
        AppendLogLine("[" + BuildTimestampForLog() + "] [CHCLIDIFF] u32 no_changes");
    }

    bool anyU64 = false;
    for (size_t i = 0; i + 8 <= t0->bytes.size(); i += 8) {
        uint64_t v0 = 0;
        uint64_t v1 = 0;
        uint64_t v2 = 0;
        std::memcpy(&v0, t0->bytes.data() + i, sizeof(v0));
        std::memcpy(&v1, p1->bytes.data() + i, sizeof(v1));
        std::memcpy(&v2, p10->bytes.data() + i, sizeof(v2));
        if (v0 == v1 && v1 == v2) {
            continue;
        }
        anyU64 = true;
        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [CHCLIDIFF] "
            << "u64 +" << std::hex << std::setw(2) << std::setfill('0') << i
            << " t0=0x" << v0
            << " +1000ms=0x" << v1
            << " +10000ms=0x" << v2;
        AppendLogLine(oss.str());
    }
    if (!anyU64) {
        AppendLogLine("[" + BuildTimestampForLog() + "] [CHCLIDIFF] u64 no_changes");
    }

    g_cooldownPacketWatch.dumpedChCliDiff = true;
}

void LogSecondLevelHeapDump(const char* phaseName,
                            const PacketUseEvent& e,
                            const char* parentLabel,
                            int parentOffset,
                            uint64_t childValue) {
    if (!IsLikelyHeapPointer(childValue)) {
        return;
    }

    const uintptr_t childPtr = static_cast<uintptr_t>(childValue & ~static_cast<uint64_t>(1));
    uint8_t raw[0x60] = {};
    SIZE_T bytesRead = 0;
    if (!ReadProcessMemory(GetCurrentProcess(),
                           reinterpret_cast<LPCVOID>(childPtr),
                           raw,
                           sizeof(raw),
                           &bytesRead) ||
        bytesRead != sizeof(raw)) {
        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [HEAPDUMP2] "
            << "phase=" << phaseName
            << " hotkey=" << e.hotkeyName
            << " archive_key=" << e.archiveKey
            << " " << parentLabel << "+0x" << std::hex << std::setw(2) << std::setfill('0') << parentOffset
            << " -> 0x" << childPtr
            << " read_failed";
        AppendLogLine(oss.str());
        return;
    }

    {
        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [HEAPDUMP2] "
            << "phase=" << phaseName
            << " hotkey=" << e.hotkeyName
            << " archive_key=" << e.archiveKey
            << " " << parentLabel << "+0x" << std::hex << std::setw(2) << std::setfill('0') << parentOffset
            << " -> 0x" << childPtr;
        AppendLogLine(oss.str());
    }

    for (size_t i = 0; i < sizeof(raw); i += 8) {
        uint64_t qword = 0;
        std::memcpy(&qword, raw + i, sizeof(qword));
        const uint32_t lo = static_cast<uint32_t>(qword & 0xFFFFFFFFULL);
        const uint32_t hi = static_cast<uint32_t>(qword >> 32);

        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [HEAPDUMP2] "
            << " +" << std::hex << std::setw(2) << std::setfill('0') << i
            << " qword=0x" << std::setw(16) << qword
            << std::dec
            << " lo=" << lo
            << " hi=" << hi;
        if (IsLikelyHeapPointer(qword)) {
            oss << " <<< PTR";
        }
        AppendLogLine(oss.str());
    }
}

void LogFindingsHeapDump(const char* phaseName,
                         const PacketUseEvent& e,
                         const SkillbarSnapshot& snap,
                         const std::string& nodeStateText,
                         uintptr_t ptr48Untagged,
                         uintptr_t inferredBase) {
    if (!ShouldLogFindingsHeapDumpPhase(phaseName)) {
        return;
    }

    if (!inferredBase) {
        return;
    }

    std::optional<uint32_t> internalNowForTsScan;
    auto dumpNodeTarget = [&](const char* label, uintptr_t fieldAddr, uintptr_t selfRefAddr) {
        uint64_t targetValue = 0;
        if (!ReadQwordSafe(fieldAddr, targetValue)) {
            std::ostringstream oss;
            oss << "[" << BuildTimestampForLog() << "] [HEAPDUMP] "
                << "phase=" << phaseName
                << " hotkey=" << e.hotkeyName
                << " archive_key=" << e.archiveKey
                << " field=" << label
                << " read_failed field_addr=0x" << std::hex << fieldAddr;
            AppendLogLine(oss.str());
            return;
        }

        const uintptr_t rawPtr = static_cast<uintptr_t>(targetValue);
        const uintptr_t nodePtr = rawPtr & ~static_cast<uintptr_t>(1);
        {
            std::ostringstream oss;
            oss << "[" << BuildTimestampForLog() << "] [HEAPDUMP] "
                << "phase=" << phaseName
                << " hotkey=" << e.hotkeyName
                << " archive_key=" << e.archiveKey
                << " field=" << label
                << " field_addr=0x" << std::hex << fieldAddr
                << " raw=0x" << rawPtr
                << " node=0x" << nodePtr
                << " self_ref=0x" << selfRefAddr
                << " ptr48_untagged=0x" << ptr48Untagged
                << std::dec
                << " node_state=" << nodeStateText;
            if (!nodePtr) {
                oss << " (null)";
            } else if (nodePtr == selfRefAddr || nodePtr == ptr48Untagged) {
                oss << " (self-ref, no cd object)";
            } else {
                oss << " (external_node)";
            }
            AppendLogLine(oss.str());
        }

        if (!nodePtr || nodePtr == selfRefAddr || nodePtr == ptr48Untagged) {
            return;
        }

        uint8_t raw[0x100] = {};
        SIZE_T bytesRead = 0;
        if (!ReadProcessMemory(GetCurrentProcess(),
                               reinterpret_cast<LPCVOID>(nodePtr),
                               raw,
                               sizeof(raw),
                               &bytesRead) ||
            bytesRead != sizeof(raw)) {
            std::ostringstream oss;
            oss << "[" << BuildTimestampForLog() << "] [HEAPDUMP] "
                << "phase=" << phaseName
                << " hotkey=" << e.hotkeyName
                << " archive_key=" << e.archiveKey
                << " field=" << label
                << " node=0x" << std::hex << nodePtr
                << " read_failed";
            AppendLogLine(oss.str());
            return;
        }

        for (size_t i = 0; i < sizeof(raw); i += 4) {
            if (!ShouldLogHeapFieldOffset(label, i)) {
                continue;
            }
            const uint32_t asU32 = ReadLe32FromBytes(raw, i);
            std::ostringstream oss;
            oss << "[" << BuildTimestampForLog() << "] [HEAPDUMP] "
                << "field=" << label
                << " +" << std::hex << std::setw(2) << std::setfill('0') << i
                << " raw="
                << std::setw(2) << static_cast<unsigned>(raw[i]) << "."
                << std::setw(2) << static_cast<unsigned>(raw[i + 1]) << "."
                << std::setw(2) << static_cast<unsigned>(raw[i + 2]) << "."
                << std::setw(2) << static_cast<unsigned>(raw[i + 3])
                << std::dec
                << " u32=" << asU32;
            AppendLogLine(oss.str());
        }

        if (std::strcmp(label, "base+0x40") == 0) {
            internalNowForTsScan = ReadLe32FromBytes(raw, 0x18);
        }
        const uint32_t internalNow = internalNowForTsScan.value_or(ReadLe32FromBytes(raw, 0x18));
        LogCooldownEndCandidateScan(phaseName, e, label, raw, internalNow, sizeof(raw));

        if (std::strcmp(label, "base+0x60") == 0) {
            uint64_t ptr50 = 0;
            uint64_t ptr58 = 0;
            std::memcpy(&ptr50, raw + 0x50, sizeof(ptr50));
            std::memcpy(&ptr58, raw + 0x58, sizeof(ptr58));
            LogSecondLevelHeapDump(phaseName, e, label, 0x50, ptr50);
            LogSecondLevelHeapDump(phaseName, e, label, 0x58, ptr58);
        }
    };

    dumpNodeTarget("base+0x40", inferredBase + 0x40, inferredBase + 0x40);
    dumpNodeTarget("base+0x60", inferredBase + 0x60, inferredBase + 0x60);
    if (ptr48Untagged) {
        dumpNodeTarget("ptr48+0x20", ptr48Untagged + 0x20, snap.ptr60Addr);
    }

    if (snap.chCliSkillbarAddr && internalNowForTsScan.has_value()) {
        uint8_t chcliRaw[0x200] = {};
        SIZE_T chcliBytesRead = 0;
        if (ReadProcessMemory(GetCurrentProcess(),
                              reinterpret_cast<LPCVOID>(snap.chCliSkillbarAddr),
                              chcliRaw,
                              sizeof(chcliRaw),
                              &chcliBytesRead) &&
            chcliBytesRead == sizeof(chcliRaw)) {
            LogCooldownEndCandidateScan(phaseName,
                                        e,
                                        "chcli",
                                        chcliRaw,
                                        internalNowForTsScan.value(),
                                        sizeof(chcliRaw));

            if (ShouldCaptureChCliPhase(phaseName) && g_cooldownPacketWatch.active) {
                bool alreadyCaptured = false;
                for (const auto& existing : g_chCliSnapshots) {
                    if (existing.phaseName == phaseName) {
                        alreadyCaptured = true;
                        break;
                    }
                }
                if (!alreadyCaptured) {
                    TimedChCliSnapshot snapshot = {};
                    snapshot.phaseName = phaseName;
                    snapshot.addr = snap.chCliSkillbarAddr;
                    std::memcpy(snapshot.bytes.data(), chcliRaw, sizeof(chcliRaw));
                    g_chCliSnapshots.push_back(std::move(snapshot));
                    LogChCliDiffSummary();
                }
            }
        } else {
            std::ostringstream oss;
            oss << "[" << BuildTimestampForLog() << "] [TSSCAN  ] "
                << "phase=" << phaseName
                << " hotkey=" << e.hotkeyName
                << " archive_key=" << e.archiveKey
                << " label=chcli"
                << " read_failed chcli=0x" << std::hex << snap.chCliSkillbarAddr;
            AppendLogLine(oss.str());
        }
    }
}

std::string BuildHexDumpSummary(const char* label, const uint8_t* data, size_t size) {
    if (!label || !data || size == 0) {
        return {};
    }

    std::ostringstream oss;
    oss << " " << label << "=";
    for (size_t i = 0; i < size; ++i) {
        if (i > 0) {
            oss << ".";
        }
        oss << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(data[i]);
    }
    return oss.str();
}

std::string DescribePayloadTags(uint32_t asU32, float asF32, uint32_t now) {
    const bool likeTimestamp = (asU32 > 1000000u) &&
                               (static_cast<int32_t>(now - asU32) >= 0) &&
                               (static_cast<int32_t>(now - asU32) < 120000);
    const bool likeRate = (asF32 >= 0.5f) && (asF32 <= 3.0f);
    const bool likeCooldownMs = (asU32 >= 500u) && (asU32 <= 120000u);
    const bool likeRemaining = (asU32 >= 100u) && (asU32 <= 60000u);
    const bool likeOneF = (asU32 == 0x3F800000u);

    if (likeTimestamp) {
        return "TIMESTAMP?";
    }
    if (likeOneF) {
        return "1.0f(rate?)";
    }
    if (likeRate) {
        return "RATE?";
    }
    if (likeCooldownMs && !likeRemaining) {
        return "RECHARGE?";
    }
    if (likeRemaining) {
        return "REMAINING?";
    }
    return {};
}

bool IsZXCProbeHotkey(const std::string& hotkeyName) {
    return hotkeyName == "Z" || hotkeyName == "X" || hotkeyName == "C";
}

bool ShouldLogBackscanPhase(const char* phaseName) {
    return phaseName && std::strcmp(phaseName, "t0") == 0;
}

bool ShouldLogPayscanPhase(const char* phaseName) {
    return phaseName &&
           (std::strcmp(phaseName, "t0") == 0 || std::strcmp(phaseName, "ready+250ms") == 0);
}

void LogPtr60BackwardCandidates(const char* phaseName,
                                const PacketUseEvent& e,
                                const SkillbarSnapshot& snap) {
    if (!IsZXCProbeHotkey(e.hotkeyName)) {
        return;
    }

    if (!snap.ptr60Addr || !ShouldLogBackscanPhase(phaseName)) {
        return;
    }

    const uint32_t now = GetTickCount();
    const uint8_t* base = snap.rawPtr60;

    std::ostringstream header;
    header << "[" << BuildTimestampForLog() << "] [BACKSCAN] phase=" << (phaseName ? phaseName : "?")
           << " hotkey=" << e.hotkeyName
           << " archive_key=" << e.archiveKey
           << " ptr60=0x" << std::hex << snap.ptr60Addr
           << std::dec
           << " now=" << now;
    AppendLogLine(header.str());

    for (int back = 0x10; back <= 0x40; back += 0x08) {
        const size_t offset = static_cast<size_t>(0x40 - back);
        if (offset + 0x18 > sizeof(snap.rawPtr60)) {
            continue;
        }

        const uint32_t field0 = ReadLe32FromBytes(base, offset + 0x00);
        const uint32_t field4 = ReadLe32FromBytes(base, offset + 0x04);
        const uint32_t field8 = ReadLe32FromBytes(base, offset + 0x08);
        const float fieldC = ReadFloat32FromBytes(base, offset + 0x0C);
        const uint32_t field10 = ReadLe32FromBytes(base, offset + 0x10);
        const uint32_t field14 = ReadLe32FromBytes(base, offset + 0x14);

        const bool field0LikeId = (field0 > 100u) && (field0 < 100000000u);
        const bool field4LikeAmmo = (field4 <= 10u);
        const bool field8LikeTimestamp = (static_cast<int32_t>(now - field8) >= 0) &&
                                         (static_cast<int32_t>(now - field8) < 300000);
        const bool fieldCLikeRate = (fieldC >= 0.5f) && (fieldC <= 3.0f);
        const bool field10LikeRecharge = (field10 >= 500u) && (field10 <= 120000u);
        const int score = static_cast<int>(field0LikeId) +
                          static_cast<int>(field4LikeAmmo) +
                          static_cast<int>(field8LikeTimestamp) +
                          static_cast<int>(fieldCLikeRate) +
                          static_cast<int>(field10LikeRecharge);

        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [BACKSCAN] "
            << "ptr60-0x" << std::hex << std::setw(2) << std::setfill('0') << back
            << " = 0x" << (snap.ptr60Addr - static_cast<uintptr_t>(back))
            << std::dec
            << " score=" << score << "/5"
            << " id=" << field0
            << " ammo=" << field4
            << " ts_age=" << static_cast<int32_t>(now - field8) << "ms"
            << " rate=" << std::fixed << std::setprecision(3) << fieldC
            << " recharge=" << field10
            << " remain=" << field14;
        AppendLogLine(oss.str());
    }
}

void LogPtr60PayloadScan(const char* phaseName,
                         const PacketUseEvent& e,
                         const SkillbarSnapshot& snap) {
    if (!IsZXCProbeHotkey(e.hotkeyName) || !snap.ptr60Addr || !ShouldLogPayscanPhase(phaseName)) {
        return;
    }

    const uint32_t now = GetTickCount();
    const uint8_t* payload = snap.rawPtr60 + kPtr60PayloadOffset;

    {
        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [PAYSCAN ] phase=" << (phaseName ? phaseName : "?")
            << " hotkey=" << e.hotkeyName
            << " archive_key=" << e.archiveKey
            << " ptr60=0x" << std::hex << snap.ptr60Addr
            << " payload_start=0x" << (snap.ptr60Addr + kPtr60PayloadOffset)
            << std::dec
            << " now=" << now;
        AppendLogLine(oss.str());
    }

    for (size_t i = 0; i + 4 <= kPtr60PayloadLen; i += 4) {
        const uint32_t asU32 = ReadLe32FromBytes(payload, i);
        const float asF32 = ReadFloat32FromBytes(payload, i);
        const std::string tags = DescribePayloadTags(asU32, asF32, now);

        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [PAYSCAN ]"
            << "  +" << std::hex << std::setw(2) << std::setfill('0') << (kPtr60PayloadOffset + i)
            << std::dec
            << "  u32=" << std::setw(12) << asU32
            << "  f32=" << std::fixed << std::setprecision(4) << std::setw(12) << asF32
            << "  raw=" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << asU32;
        if (!tags.empty()) {
            oss << " <<< " << tags;
        }
        AppendLogLine(oss.str());
    }
}

void LogPtr60Plus20Chase(const char* phaseName,
                         const PacketUseEvent& e,
                         const SkillbarSnapshot& snap) {
    if (!IsZXCProbeHotkey(e.hotkeyName) || !snap.ptr60Addr) {
        return;
    }

    static std::unordered_map<std::string, uintptr_t> s_lastPtr60Plus20ByHotkey;
    uintptr_t ptrNext = 0;
    std::memcpy(&ptrNext, snap.rawPtr60 + 0x20, sizeof(ptrNext));
    const std::string chaseKey = e.hotkeyName + "|" + e.archiveKey;
    const auto lastIt = s_lastPtr60Plus20ByHotkey.find(chaseKey);
    const bool changed = (lastIt == s_lastPtr60Plus20ByHotkey.end()) || (lastIt->second != ptrNext);
    s_lastPtr60Plus20ByHotkey[chaseKey] = ptrNext;
    if (!changed) {
        return;
    }

    std::ostringstream header;
    header << "[" << BuildTimestampForLog() << "] [CHASE   ] phase=" << (phaseName ? phaseName : "?")
           << " hotkey=" << e.hotkeyName
           << " archive_key=" << e.archiveKey
           << " ptr60=0x" << std::hex << snap.ptr60Addr
           << " ptr60+0x20=0x" << ptrNext;
    AppendLogLine(header.str());

    if (!ptrNext || ptrNext == snap.ptr60Addr) {
        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [CHASE   ] ptr60+0x20="
            << (!ptrNext ? "0" : "SELF")
            << " (NODE_READY/no chained object)";
        AppendLogLine(oss.str());
        return;
    }

    uint8_t raw[0x60] = {};
    const uintptr_t ptrNextUntagged = ptrNext & ~static_cast<uintptr_t>(0x1);
    SIZE_T bytesRead = 0;
    if (!ReadProcessMemory(GetCurrentProcess(),
                           reinterpret_cast<LPCVOID>(ptrNextUntagged),
                           raw,
                           sizeof(raw),
                           &bytesRead) ||
        bytesRead != sizeof(raw)) {
        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [CHASE   ] read_failed target=0x"
            << std::hex << ptrNextUntagged;
        AppendLogLine(oss.str());
        return;
    }

    {
        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [CHASE   ] "
            << BuildHexDumpSummary("raw60", raw, sizeof(raw));
        AppendLogLine(oss.str());
    }

    const uint32_t now = GetTickCount();
    for (size_t i = 0; i + 4 <= sizeof(raw); i += 4) {
        const uint32_t asU32 = ReadLe32FromBytes(raw, i);
        const float asF32 = ReadFloat32FromBytes(raw, i);
        const bool likeTimestamp = (static_cast<int32_t>(now - asU32) >= 0) &&
                                   (static_cast<int32_t>(now - asU32) < 300000);
        const bool likeRate = (asF32 >= 0.5f) && (asF32 <= 3.0f);
        const bool likeCooldownMs = (asU32 >= 500u) && (asU32 <= 120000u);
        const bool likeOneF = (asU32 == 0x3F800000u);

        std::string tag;
        if (likeTimestamp) {
            tag = "TS";
        } else if (likeOneF) {
            tag = "1.0f";
        } else if (likeRate) {
            tag = "RATE";
        } else if (likeCooldownMs) {
            tag = "CD";
        }

        if (tag.empty() && asU32 == 0) {
            continue;
        }

        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [CHASE   ]"
            << "  +" << std::hex << std::setw(2) << std::setfill('0') << i
            << std::dec
            << "  u32=" << std::setw(10) << asU32
            << "  f32=" << std::fixed << std::setprecision(3) << std::setw(10) << asF32
            << "  raw=" << std::hex << std::uppercase << std::setw(8) << std::setfill('0') << asU32;
        if (!tag.empty()) {
            oss << " <<" << tag;
        }
        AppendLogLine(oss.str());
    }
}

void LogNodePlus20Chase2(const char* phaseName,
                         const PacketUseEvent& e,
                         const SkillbarSnapshot& snap,
                         const std::string& nodeStateText) {
    if (!phaseName) {
        return;
    }

    const uintptr_t ptr48Untagged = snap.ptr48Addr ? (snap.ptr48Addr & ~static_cast<uintptr_t>(1)) : 0;
    const uintptr_t ptr60 = snap.ptr60Addr;

    if (ptr60) {
        uint64_t ptr60Plus20 = 0;
        std::memcpy(&ptr60Plus20, snap.rawPtr60 + 0x20, sizeof(ptr60Plus20));
        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [CHASE2  ] "
            << "phase=" << phaseName
            << " node=" << nodeStateText
            << " ptr60+0x20=0x" << std::hex << ptr60Plus20;
        AppendLogLine(oss.str());
    }

    if (ptr48Untagged) {
        uint64_t ptr48Plus20 = 0;
        if (ReadQwordSafe(ptr48Untagged + 0x20, ptr48Plus20)) {
            std::ostringstream oss;
            oss << "[" << BuildTimestampForLog() << "] [CHASE2  ] "
                << "phase=" << phaseName
                << " node=" << nodeStateText
                << " ptr48+0x20=0x" << std::hex << ptr48Plus20;
            AppendLogLine(oss.str());
        }
    }
}

std::string FormatZClusterOrdinal(unsigned int ordinal) {
    std::ostringstream oss;
    oss << "Z_CLUSTER_" << std::setw(2) << std::setfill('0') << ordinal;
    return oss.str();
}

std::string ClassifyZNodeState(NodePairState& nodeState,
                               uintptr_t node40,
                               uintptr_t node60,
                               uintptr_t ptr48Plus20) {
    if (!node40 && !node60 && !ptr48Plus20) {
        return "UNKNOWN";
    }

    if (!nodeState.sawReadyBaseline && node40 && node60) {
        nodeState.readyNode40 = node40;
        nodeState.readyNode60 = node60;
        nodeState.readyPtr48Plus20 = ptr48Plus20;
        nodeState.sawReadyBaseline = true;
    }

    if (!nodeState.sawReadyBaseline) {
        return "UNKNOWN";
    }

    if (node40 == nodeState.readyNode40 &&
        node60 == nodeState.readyNode60 &&
        ptr48Plus20 == nodeState.readyPtr48Plus20) {
        return "READY_BASELINE";
    }

    if (!nodeState.zStableA40 &&
        node40 && node60 &&
        (node40 != nodeState.readyNode40 ||
         node60 != nodeState.readyNode60 ||
         ptr48Plus20 != nodeState.readyPtr48Plus20)) {
        nodeState.zStableA40 = node40;
        nodeState.zStableA60 = node60;
        nodeState.zStableAPtr48Plus20 = ptr48Plus20;
    }

    if (node40 == nodeState.zStableA40 &&
        node60 == nodeState.zStableA60 &&
        ptr48Plus20 == nodeState.zStableAPtr48Plus20) {
        return "ALT40_ALT60_A";
    }

    for (const auto& cluster : nodeState.zExtraClusters) {
        if (cluster.cur40 == node40 &&
            cluster.cur60 == node60 &&
            cluster.ptr48Plus20 == ptr48Plus20) {
            return cluster.name;
        }
    }

    ZNamedCluster cluster = {};
    cluster.name = FormatZClusterOrdinal(nodeState.nextZClusterOrdinal++);
    cluster.cur40 = node40;
    cluster.cur60 = node60;
    cluster.ptr48Plus20 = ptr48Plus20;
    nodeState.zExtraClusters.push_back(cluster);
    return cluster.name;
}

void DumpZStateRuleSummaryLocked(const TrackedSkillState& s) {
    const auto* cfg = GetConfig(s);
    if (!cfg || cfg->hotkeyName != "Z") {
        return;
    }

    AppendLogLine("[" + BuildTimestampForLog() + "] [ZRULE   ] enum={READY_BASELINE,ALT40_ALT60_A,Z_CLUSTER_01...,UNKNOWN}");

    std::ostringstream readyRule;
    readyRule << "[" << BuildTimestampForLog() << "] [ZRULE   ] "
              << "READY_BASELINE => cur40=0x" << std::hex << s.nodeState.readyNode40
              << " cur60=0x" << s.nodeState.readyNode60
              << " ptr48p20=0x" << s.nodeState.readyPtr48Plus20;
    AppendLogLine(readyRule.str());

    std::ostringstream alt60ARule;
    alt60ARule << "[" << BuildTimestampForLog() << "] [ZRULE   ] "
               << "ALT40_ALT60_A => cur40=0x" << std::hex << s.nodeState.zStableA40
               << " cur60=0x" << s.nodeState.zStableA60
               << " ptr48p20=0x" << s.nodeState.zStableAPtr48Plus20;
    AppendLogLine(alt60ARule.str());

    for (const auto& cluster : s.nodeState.zExtraClusters) {
        std::ostringstream clusterRule;
        clusterRule << "[" << BuildTimestampForLog() << "] [ZRULE   ] "
                    << cluster.name
                    << " => cur40=0x" << std::hex << cluster.cur40
                    << " cur60=0x" << cluster.cur60
                    << " ptr48p20=0x" << cluster.ptr48Plus20;
        AppendLogLine(clusterRule.str());
    }

    AppendLogLine("[" + BuildTimestampForLog() + "] [ZRULE   ] UNKNOWN => tuple unavailable or not yet learned");
}

void DumpZStateValidationTableLocked() {
    if (!g_cooldownPacketWatch.active ||
        g_cooldownPacketWatch.hotkeyName != "Z" ||
        g_cooldownPacketWatch.zStateSnapshots.empty()) {
        return;
    }

    AppendLogLine("[" + BuildTimestampForLog() + "] [ZTABLE  ] === Z SAMPLE VALIDATION === run_id=" +
                  std::to_string(g_cooldownPacketWatch.runId));
    for (const auto& snapshot : g_cooldownPacketWatch.zStateSnapshots) {
        std::ostringstream line;
        line << "[" << BuildTimestampForLog() << "] [ZTABLE  ] "
             << "run_id=" << snapshot.runId
             << " phase=" << snapshot.phaseName
             << " cur40=0x" << std::hex << snapshot.cur40
             << " cur60=0x" << snapshot.cur60
             << " ptr48p20=0x" << snapshot.ptr48Plus20
             << std::dec
             << " node_state_old=" << snapshot.oldStateText
             << " node_state_new=" << snapshot.stateText
             << " classified_cluster=" << snapshot.stateText;
        AppendLogLine(line.str());
    }
}

void DumpZStateClusterTableLocked() {
    if (!g_cooldownPacketWatch.active ||
        g_cooldownPacketWatch.hotkeyName != "Z" ||
        g_cooldownPacketWatch.zStateSnapshots.empty()) {
        return;
    }

    struct ClusterRow {
        uintptr_t cur40 = 0;
        uintptr_t cur60 = 0;
        uintptr_t ptr48Plus20 = 0;
        int occurrences = 0;
        unsigned long long firstSeenRunId = 0;
        std::string firstSeenPhase;
        std::set<std::string> transitionFrom;
        std::set<std::string> transitionTo;
    };

    std::map<std::string, ClusterRow> rows;
    std::vector<TimedZStateSnapshot> snapshots = g_recentZStateSnapshots;
    snapshots.insert(snapshots.end(),
                     g_cooldownPacketWatch.zStateSnapshots.begin(),
                     g_cooldownPacketWatch.zStateSnapshots.end());

    for (size_t i = 0; i < snapshots.size(); ++i) {
        const auto& snapshot = snapshots[i];
        if (snapshot.stateText == "UNKNOWN") {
            continue;
        }

        auto& row = rows[snapshot.stateText];
        row.cur40 = snapshot.cur40;
        row.cur60 = snapshot.cur60;
        row.ptr48Plus20 = snapshot.ptr48Plus20;
        ++row.occurrences;
        if (row.firstSeenRunId == 0) {
            row.firstSeenRunId = snapshot.runId;
            row.firstSeenPhase = snapshot.phaseName;
        }

        if (i > 0 && snapshots[i - 1].runId == snapshot.runId &&
            snapshots[i - 1].stateText != snapshot.stateText) {
            row.transitionFrom.insert(snapshots[i - 1].stateText);
        }
        if (i + 1 < snapshots.size() && snapshots[i + 1].runId == snapshot.runId &&
            snapshots[i + 1].stateText != snapshot.stateText) {
            row.transitionTo.insert(snapshots[i + 1].stateText);
        }
    }

    auto joinSet = [](const std::set<std::string>& values) {
        if (values.empty()) {
            return std::string("<none>");
        }
        std::ostringstream oss;
        bool first = true;
        for (const auto& value : values) {
            if (!first) {
                oss << ",";
            }
            oss << value;
            first = false;
        }
        return oss.str();
    };

    AppendLogLine("[" + BuildTimestampForLog() + "] [ZCLTABLE] === Z STATE CLUSTER TABLE ===");
    for (const auto& [clusterName, row] : rows) {
        std::ostringstream line;
        line << "[" << BuildTimestampForLog() << "] [ZCLTABLE] "
             << "cluster_name=" << clusterName
             << " cur40=0x" << std::hex << row.cur40
             << " cur60=0x" << row.cur60
             << " ptr48p20=0x" << row.ptr48Plus20
             << std::dec
             << " occurrences=" << row.occurrences
             << " first_seen_run_phase=run_" << row.firstSeenRunId << ":" << row.firstSeenPhase
             << " transition_from=" << joinSet(row.transitionFrom)
             << " transition_to=" << joinSet(row.transitionTo);
        AppendLogLine(line.str());
    }
}

void DumpZStateStabilitySummaryLocked() {
    if (!g_cooldownPacketWatch.active ||
        g_cooldownPacketWatch.hotkeyName != "Z" ||
        g_cooldownPacketWatch.zStateSnapshots.empty()) {
        return;
    }

    std::map<std::string, int> currentCounts;
    for (const auto& snapshot : g_cooldownPacketWatch.zStateSnapshots) {
        ++currentCounts[snapshot.stateText];
    }

    std::map<std::string, int> recentCounts;
    for (const auto& snapshot : g_recentZStateSnapshots) {
        ++recentCounts[snapshot.stateText];
    }

    std::ostringstream stableLine;
    stableLine << "[" << BuildTimestampForLog() << "] [ZSUMMARY ] stable_clusters=";
    bool wroteStable = false;
    for (const auto& [state, count] : currentCounts) {
        if (state == "UNKNOWN" || count == 0) {
            continue;
        }
        if (wroteStable) {
            stableLine << ",";
        }
        stableLine << state << "x" << count;
        wroteStable = true;
    }
    if (!wroteStable) {
        stableLine << "<none>";
    }
    AppendLogLine(stableLine.str());

    std::ostringstream needsMoreLine;
    needsMoreLine << "[" << BuildTimestampForLog() << "] [ZSUMMARY ] need_more_samples=";
    bool wroteNeedMore = false;
    for (const auto& [state, count] : currentCounts) {
        if (state != "UNKNOWN") {
            continue;
        }
        if (wroteNeedMore) {
            needsMoreLine << ",";
        }
        needsMoreLine << state << "x" << count;
        wroteNeedMore = true;
    }
    if (!wroteNeedMore) {
        needsMoreLine << "<none>";
    }
    AppendLogLine(needsMoreLine.str());

    std::ostringstream recentLine;
    recentLine << "[" << BuildTimestampForLog() << "] [ZSUMMARY ] prior_cache=";
    bool wroteRecent = false;
    for (const auto& [state, count] : recentCounts) {
        if (count <= 0) {
            continue;
        }
        if (wroteRecent) {
            recentLine << ",";
        }
        recentLine << state << "x" << count;
        wroteRecent = true;
    }
    if (!wroteRecent) {
        recentLine << "<empty>";
    }
    AppendLogLine(recentLine.str());
}

void LogPtr60PayloadDiff(const char* reason,
                         const PacketUseEvent& e,
                         uintptr_t ptr60,
                         const std::array<uint8_t, kPtr60PayloadLen>& before,
                         const uint8_t* after) {
    if (!after) {
        return;
    }

    std::ostringstream header;
    header << "[" << BuildTimestampForLog() << "] [PAYDIFF ] reason=" << (reason ? reason : "?")
           << " hotkey=" << e.hotkeyName
           << " archive_key=" << e.archiveKey
           << " ptr60=0x" << std::hex << ptr60
           << " payload_start=0x" << (ptr60 + kPtr60PayloadOffset);
    AppendLogLine(header.str());

    for (size_t i = 0; i + 4 <= kPtr60PayloadLen; i += 4) {
        const uint32_t beforeU32 = ReadLe32FromBytes(before.data(), i);
        const uint32_t afterU32 = ReadLe32FromBytes(after, i);
        if (beforeU32 == afterU32) {
            continue;
        }

        const float beforeF32 = ReadFloat32FromBytes(before.data(), i);
        const float afterF32 = ReadFloat32FromBytes(after, i);

        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [PAYDIFF ]"
            << "  +" << std::hex << std::setw(2) << std::setfill('0') << (kPtr60PayloadOffset + i)
            << "  " << std::uppercase << std::setw(8) << beforeU32
            << " -> " << std::setw(8) << afterU32
            << std::dec
            << "  (u32: " << beforeU32 << " -> " << afterU32 << ")"
            << "  (f32: " << std::fixed << std::setprecision(4) << beforeF32
            << " -> " << afterF32 << ")";
        AppendLogLine(oss.str());
    }
}

std::pair<uintptr_t, uintptr_t> ReadNodePairForHotkey(const SkillbarSnapshot& snap, const std::string& hotkeyName) {
    uintptr_t inferredBase = 0;
    if (snap.ptr60Addr >= 0x60) {
        inferredBase = snap.ptr60Addr - 0x60;
    } else if (snap.charSkillbarAddr) {
        inferredBase = snap.charSkillbarAddr;
    }
    if (!inferredBase) {
        return {};
    }

    if (hotkeyName == "C" || hotkeyName == "6" || hotkeyName == "Z") {
        return {
            ReadTaggedFieldAsNode(inferredBase, 0x40),
            ReadTaggedFieldAsNode(inferredBase, 0x60)
        };
    }

    return {};
}

void RefreshNodeStateForHotkey(TrackedSkillState& s, const SkillbarSnapshot& snap) {
    const SkillConfig* cfg = GetConfig(s);
    if (!cfg) {
        return;
    }

    auto [node40, node60] = ReadNodePairForHotkey(snap, cfg->hotkeyName);
    const uintptr_t ptr48Untagged = snap.ptr48Addr ? (snap.ptr48Addr & ~static_cast<uintptr_t>(1)) : 0;
    uint64_t ptr48Plus20Raw = 0;
    const uintptr_t ptr48Plus20 = ptr48Untagged && ReadQwordSafe(ptr48Untagged + 0x20, ptr48Plus20Raw)
                                      ? UntagNodePointer(ptr48Plus20Raw)
                                      : 0;
    if (!node40 && !node60) {
        s.nodeState.isCooldown = false;
        s.nodeState.stateText = "UNKNOWN";
        return;
    }

    if (cfg->hotkeyName == "Z") {
        s.nodeState.stateText = ClassifyZNodeState(s.nodeState, node40, node60, ptr48Plus20);
        s.nodeState.isCooldown = false;
        return;
    }

    if (!s.nodeState.sawReadyBaseline) {
        s.nodeState.readyNode40 = node40;
        s.nodeState.readyNode60 = node60;
        s.nodeState.sawReadyBaseline = true;
    }

    if (!s.nodeState.sawCooldownBaseline &&
        ((s.nodeState.readyNode40 && node40 && node40 != s.nodeState.readyNode40) ||
         (s.nodeState.readyNode60 && node60 && node60 != s.nodeState.readyNode60))) {
        s.nodeState.cooldownNode40 = node40;
        s.nodeState.cooldownNode60 = node60;
        s.nodeState.sawCooldownBaseline = true;
    }

    const bool matchesReady =
        s.nodeState.sawReadyBaseline &&
        (!s.nodeState.readyNode40 || node40 == s.nodeState.readyNode40) &&
        (!s.nodeState.readyNode60 || node60 == s.nodeState.readyNode60);
    const bool matchesCooldown =
        s.nodeState.sawCooldownBaseline &&
        (!s.nodeState.cooldownNode40 || node40 == s.nodeState.cooldownNode40) &&
        (!s.nodeState.cooldownNode60 || node60 == s.nodeState.cooldownNode60);

    if (matchesCooldown) {
        s.nodeState.isCooldown = true;
        s.nodeState.stateText = "NODE_CD";
    } else if (matchesReady) {
        s.nodeState.isCooldown = false;
        s.nodeState.stateText = "NODE_READY";
    } else if (s.nodeState.sawCooldownBaseline) {
        s.nodeState.isCooldown = false;
        s.nodeState.stateText = "UNKNOWN";
    } else {
        s.nodeState.isCooldown = false;
        s.nodeState.stateText = "NODE_LEARNING";
    }
}

void CaptureDebugAssistSnapshot(const char* phaseName, const PacketUseEvent& e, const SkillbarSnapshot& snap) {
    const uintptr_t ptr48Untagged = snap.ptr48Addr ? (snap.ptr48Addr & ~static_cast<uintptr_t>(1)) : 0;
    const uintptr_t inferredBase = snap.ptr60Addr >= 0x60 ? (snap.ptr60Addr - 0x60) : 0;
    const uint8_t* ptr60Payload = snap.rawPtr60 + kPtr60PayloadOffset;
    std::string previousNodeStateText = "NODE_UNKNOWN";
    std::string currentNodeStateText = "NODE_UNKNOWN";
    bool shouldLogPayloadReadyDiff = false;
    std::array<uint8_t, kPtr60PayloadLen> payloadBeforeReady = {};

    std::ostringstream oss;
    oss << "phase=" << phaseName
        << " hotkey=" << e.hotkeyName
        << " archive_key=" << e.archiveKey
        << " tick=" << e.packetTickMs
        << " ctx=0x" << std::hex << snap.ctxAddr
        << " char=0x" << snap.charSkillbarAddr
        << " chcli=0x" << snap.chCliSkillbarAddr
        << " ptr48_raw=0x" << snap.ptr48Addr
        << " ptr48_untagged=0x" << ptr48Untagged
        << " ptr60=0x" << snap.ptr60Addr
        << " inferred_base=0x" << inferredBase;
    if (inferredBase) {
        oss << " " << FormatQwordField(inferredBase + 0x40)
            << " " << FormatQwordField(inferredBase + 0x48)
            << " " << FormatQwordField(inferredBase + 0x58)
            << " " << FormatQwordField(inferredBase + 0x60)
            << " " << FormatQwordField(inferredBase + 0x68);
    }

    if (auto* state = FindStateByHotkey(e.hotkeyName)) {
        previousNodeStateText = state->nodeState.stateText;
        RefreshNodeStateForHotkey(*state, snap);
        currentNodeStateText = state->nodeState.stateText;
        oss << " node_state=" << state->nodeState.stateText;

        const bool isReadyLikeState =
            state->nodeState.stateText == "NODE_READY" ||
            state->nodeState.stateText == "READY_BASELINE";
        const bool cooldownLikeState =
            state->nodeState.stateText == "NODE_CD" ||
            state->nodeState.stateText == "CD_STATE" ||
            state->nodeState.stateText == "ALT40_READY60" ||
            state->nodeState.stateText == "ALT40_ALT60_A" ||
            state->nodeState.stateText == "ALT40_ALT60_B";
        if (cooldownLikeState) {
            std::copy(ptr60Payload, ptr60Payload + kPtr60PayloadLen, state->nodeState.lastCooldownPayload.begin());
            state->nodeState.hasLastCooldownPayload = true;
        } else if (isReadyLikeState &&
                   previousNodeStateText != state->nodeState.stateText &&
                   state->nodeState.hasLastCooldownPayload) {
            payloadBeforeReady = state->nodeState.lastCooldownPayload;
            shouldLogPayloadReadyDiff = true;
        }

        if (g_cooldownPacketWatch.active &&
            g_cooldownPacketWatch.hotkeyName == e.hotkeyName &&
            (e.runId == 0 || g_cooldownPacketWatch.runId == e.runId)) {
            const unsigned long long nowTick = GetTickCount64();
            const unsigned long long offsetMs =
                nowTick > g_cooldownPacketWatch.startedTickMs ? (nowTick - g_cooldownPacketWatch.startedTickMs) : 0;

            if (isReadyLikeState && !g_cooldownPacketWatch.loggedReadyBaseline) {
                LogNodeAnchorLocked("NODE_READY_BASELINE", phaseName, offsetMs, e);
                g_cooldownPacketWatch.loggedReadyBaseline = true;
                g_cooldownPacketWatch.haveNodeReadyBaselineOffset = true;
                g_cooldownPacketWatch.nodeReadyBaselineOffsetMs = offsetMs;
            }
            if (state->nodeState.stateText == "NODE_CD" && previousNodeStateText != "NODE_CD" &&
                !g_cooldownPacketWatch.loggedCooldownStart) {
                LogNodeAnchorLocked("NODE_CD_START", phaseName, offsetMs, e);
                g_cooldownPacketWatch.loggedCooldownStart = true;
                g_cooldownPacketWatch.haveNodeCdOffset = true;
                g_cooldownPacketWatch.nodeCdOffsetMs = offsetMs;
            }
            if (g_cooldownPacketWatch.loggedCooldownStart &&
                isReadyLikeState &&
                previousNodeStateText != state->nodeState.stateText &&
                !g_cooldownPacketWatch.loggedReadyReturn) {
                LogNodeAnchorLocked("NODE_READY", phaseName, offsetMs, e);
                g_cooldownPacketWatch.loggedReadyReturn = true;
                g_cooldownPacketWatch.haveNodeReadyOffset = true;
                g_cooldownPacketWatch.nodeReadyOffsetMs = offsetMs;
            }
        }

        if (isReadyLikeState &&
            (std::strcmp(phaseName, "ready+250ms") == 0 || previousNodeStateText != state->nodeState.stateText)) {
            LogNodeReadyStructureValidation(phaseName, e, inferredBase, ptr48Untagged, snap.ptr60Addr);
        }

        if (e.hotkeyName == "Z") {
            RecordZStateSnapshotLocked(phaseName, previousNodeStateText, state->nodeState.stateText, snap);
        }
    }

    const std::string findingsGuess = BuildFindingsAbilityGuessSummary(snap, e.hotkeyName);
    if (!findingsGuess.empty()) {
        oss << findingsGuess;
    }
    if (e.hotkeyName == "C" || e.hotkeyName == "6") {
        oss << BuildHexDumpSummary("ptr60_raw40", snap.rawPtr60, 0x40);
    }

    g_debugAssistState.lastSnapshotText = oss.str();
    g_debugAssistState.statusText = "Debug Assist captured " + std::string(phaseName) + " for " + e.hotkeyName + ".";
    AppendLogLine("[" + BuildTimestampForLog() + "] [DBGASST ] " + oss.str());

    const bool shouldBreak = g_debugAssistState.breakOnMatch;
    if (!g_debugAssistState.autoMode) {
        g_debugAssistState.armed = false;
    }
    if (shouldBreak && IsDebuggerPresent()) {
        __debugbreak();
    }
}

void QueueDebugAssistPhases(const PacketUseEvent& e, const SkillConfig* cfg) {
    const unsigned long long now = GetTickCount64();
    const bool runBound = g_cooldownPacketWatch.active;
    const unsigned long long runId = runBound ? g_cooldownPacketWatch.runId : 0;
    const unsigned long long baseTickMs = runBound ? g_cooldownPacketWatch.startedTickMs : now;

    struct PlannedPhase {
        const char* name = "";
        unsigned long long dueTickMs = 0;
    };

    std::vector<PlannedPhase> planned;
    planned.reserve(10);

    auto addPhase = [&](const char* name, unsigned long long delayMs) {
        PendingDebugAssistSnapshot pending = {};
        pending.phaseName = name;
        pending.archiveKey = e.archiveKey;
        pending.hotkeyName = e.hotkeyName;
        pending.packetTickMs = e.packetTickMs;
        pending.runId = runId;
        pending.dueTickMs = baseTickMs + delayMs;
        if (pending.dueTickMs < now) {
            pending.dueTickMs = now;
        }
        g_pendingDebugAssistSnapshots.push_back(std::move(pending));
        planned.push_back({name, baseTickMs + delayMs});
    };

    addPhase("+100ms", 100);
    addPhase("+250ms", 250);
    addPhase("+500ms", 500);
    addPhase("+1000ms", 1000);

    unsigned long long predictedReadyMs = 0;
    if (cfg) {
        if (cfg->rechargeMs > 0) {
            predictedReadyMs = static_cast<unsigned long long>(cfg->rechargeMs);
        } else if (cfg->castCdMs > 0) {
            predictedReadyMs = static_cast<unsigned long long>(cfg->castCdMs);
        }
    }
    if (predictedReadyMs > 0) {
        if (predictedReadyMs > 750) addPhase("ready-750ms", predictedReadyMs - 750);
        if (predictedReadyMs > 500) addPhase("ready-500ms", predictedReadyMs - 500);
        if (predictedReadyMs > 250) addPhase("ready-250ms", predictedReadyMs - 250);
        addPhase("ready+250ms", predictedReadyMs + 250);
    }

    std::sort(g_pendingDebugAssistSnapshots.begin(),
              g_pendingDebugAssistSnapshots.end(),
              [](const PendingDebugAssistSnapshot& a, const PendingDebugAssistSnapshot& b) {
                  if (a.dueTickMs != b.dueTickMs) {
                      return a.dueTickMs < b.dueTickMs;
                  }
                  return a.phaseName < b.phaseName;
              });
    std::sort(planned.begin(),
              planned.end(),
              [](const PlannedPhase& a, const PlannedPhase& b) {
                  if (a.dueTickMs != b.dueTickMs) {
                      return a.dueTickMs < b.dueTickMs;
                  }
                  return std::strcmp(a.name, b.name) < 0;
              });

    std::ostringstream phaseNames;
    for (size_t i = 0; i < planned.size(); ++i) {
        if (i != 0) {
            phaseNames << ",";
        }
        phaseNames << planned[i].name;
    }

    std::ostringstream phaseInfo;
    phaseInfo << "[" << BuildTimestampForLog() << "] [DBGPLAN ] hotkey=" << e.hotkeyName
              << " run_bound=" << (runBound ? 1 : 0)
              << " run_id=" << runId
              << " phases=t0," << phaseNames.str();
    if (cfg) {
        phaseInfo << " db_hotkey=" << cfg->hotkeyName
                  << " db_skill=" << cfg->skillName
                  << " db_recharge_ms=" << cfg->rechargeMs
                  << " db_cast_cd_ms=" << cfg->castCdMs
                  << " db_ammo_recharge_ms=" << cfg->ammoRechargeMs;
    }
    AppendLogLine(phaseInfo.str());
}

bool ShouldStartCooldownPacketWatchLocked(const PacketUseEvent& e, unsigned long long now) {
    if (g_cooldownPacketWatch.active) {
        const bool sameHotkey = NormalizeDebugHotkeyName(g_cooldownPacketWatch.hotkeyName) ==
                                NormalizeDebugHotkeyName(e.hotkeyName);
        if (sameHotkey && now >= g_cooldownPacketWatch.startedTickMs &&
            (now - g_cooldownPacketWatch.startedTickMs) <= kCooldownWatchRestartDedupMs) {
            return false;
        }
    }

    const bool sameRecentHotkey = NormalizeDebugHotkeyName(g_lastCooldownWatchHotkey) ==
                                  NormalizeDebugHotkeyName(e.hotkeyName);
    const bool sameRecentArchive = g_lastCooldownWatchArchiveKey == e.archiveKey;
    if (sameRecentHotkey && sameRecentArchive &&
        now >= g_lastCooldownWatchStartTickMs &&
        (now - g_lastCooldownWatchStartTickMs) <= kCooldownWatchRestartDedupMs) {
        return false;
    }

    return true;
}

void StartCooldownPacketWatch(const PacketUseEvent& e, const SkillbarSnapshot& baselineSnapshot) {
    const unsigned long long now = GetTickCount64();
    g_pendingDebugAssistSnapshots.clear();
    g_cooldownPacketWatch.active = true;
    g_cooldownPacketWatch.runId = g_nextCooldownRunId++;
    g_cooldownPacketWatch.hotkeyName = e.hotkeyName;
    g_cooldownPacketWatch.archiveKey = e.archiveKey;
    g_cooldownPacketWatch.startedTickMs = now;
    g_cooldownPacketWatch.untilTickMs = now + kCooldownWatchWindowMs;
    g_cooldownPacketWatch.loggedReadyBaseline = false;
    g_cooldownPacketWatch.loggedCooldownStart = false;
    g_cooldownPacketWatch.loggedReadyReturn = false;
    g_cooldownPacketWatch.haveNodeCdOffset = false;
    g_cooldownPacketWatch.nodeCdOffsetMs = 0;
    g_cooldownPacketWatch.haveNodeReadyBaselineOffset = false;
    g_cooldownPacketWatch.nodeReadyBaselineOffsetMs = 0;
    g_cooldownPacketWatch.haveNodeReadyOffset = false;
    g_cooldownPacketWatch.nodeReadyOffsetMs = 0;
    g_cooldownPacketWatch.dumpedNodeReadyWindow = false;
    g_cooldownPacketWatch.dumpedChCliDiff = false;
    g_cooldownPacketWatch.groupEvents.clear();
    g_cooldownPacketWatch.all0017Packets.clear();
    g_cooldownPacketWatch.all0028Packets.clear();
    g_cooldownPacketWatch.ptrWriteHits.clear();
    g_cooldownPacketWatch.candidateSmsgs.clear();
    g_cooldownPacketWatch.zStateSnapshots.clear();
    g_cooldownPacketWatch.baselineSnapshot = baselineSnapshot;
    g_cooldownPacketWatch.loggedSkillbarT0 = false;
    g_cooldownPacketWatch.loggedSkillbarNodeCd = false;
    g_cooldownPacketWatch.loggedSkillbarNodeReady = false;
    g_chCliSnapshots.clear();
    g_completed0017Groups.clear();
    g_open0017Group.reset();
    g_cooldownInfoBySlot.clear();
    g_lastCooldownWatchStartTickMs = now;
    g_lastCooldownWatchHotkey = e.hotkeyName;
    g_lastCooldownWatchArchiveKey = e.archiveKey;

    if (IsBridgeOnlyModeEnabled()) {
        std::ostringstream press;
        press << "[" << BuildTimestampForLog() << "] [PTRPKTL ] compact"
              << " run_id=" << g_cooldownPacketWatch.runId
              << " offset=+0.000s SKILL_PRESS"
              << " hotkey=" << e.hotkeyName;
        AppendLogLine(press.str());
    } else {
        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [CDHUNT  ] start"
            << " run_id=" << g_cooldownPacketWatch.runId
            << " hotkey=" << e.hotkeyName
            << " window_ms=" << kCooldownWatchWindowMs
            << " detector=candidate_smsg_v2"
            << " headers=" << (kEnableCandidateSmsgTracking ? "0x0001,0x0015" : "<disabled>")
            << " subtype=" << (kEnableCandidateSmsgTracking ? "on" : "off")
            << " field_view_0015=" << (kEnableCandidateSmsgDetailedLog ? "on" : "off")
            << " corr_timeline=" << (kEnablePacketCorrelationTimeline ? "on" : "off")
            << " legacy_stage=" << (kEnableLegacyCooldownStageLogs ? "on" : "off")
            << " legacy_z=" << (kEnableLegacyZStateLogs ? "on" : "off");
        AppendLogLine(oss.str());

        std::ostringstream press;
        press << "[" << BuildTimestampForLog() << "] [CDHUNT  ] SKILL_PRESS"
              << " run_id=" << g_cooldownPacketWatch.runId
              << " hotkey=" << e.hotkeyName
              << " tick=" << now
              << " offset=+0.000s";
        AppendLogLine(press.str());
    }

    PacketUseEvent runEvent = e;
    runEvent.runId = g_cooldownPacketWatch.runId;
    LogSkillbarMemorySnapshotLocked("t0", runEvent, baselineSnapshot, nullptr);
    g_cooldownPacketWatch.loggedSkillbarT0 = baselineSnapshot.valid;
    StartInternalPollingWatcherLocked(runEvent, baselineSnapshot);
}

void StopCooldownPacketWatchIfExpired(unsigned long long now) {
    if (!g_cooldownPacketWatch.active || now < g_cooldownPacketWatch.untilTickMs) {
        return;
    }

    if (!IsBridgeOnlyModeEnabled()) {
        FlushOpen0017GroupLocked();
    }
    DumpPtrPacketTimelineLocked();
    DumpPtrPacketComparisonLocked();
    if constexpr (kEnablePacketCorrelationTimeline) {
        DumpPacketCorrelationTimelineLocked();
    }
    DumpCandidateSmsgSummaryLocked();
    if constexpr (kEnableLegacyCooldownStageLogs) {
        // Temporarily disabled while we focus on candidate SMSG detection
        // instead of old 0x0017/0x0028 timing correlation summaries.
        Dump0028ComparisonTableLocked();
        Dump0017GroupStageTableLocked();
        Dump0028PairSummaryLocked();
        Dump0028XXSummaryLocked();
        DumpNearReadyHitRateSummaryLocked();
        DumpCooldownTimelineLocked();
    }
    if constexpr (kEnableLegacyZStateLogs) {
        // Temporarily disabled to keep the run log focused on the current
        // packet-side investigation and reduce output volume.
        if (g_cooldownPacketWatch.hotkeyName == "Z") {
            if (auto* zState = FindStateByHotkey("Z")) {
                DumpZStateRuleSummaryLocked(*zState);
            }
        }
        DumpZPacketMemoryTimelineLocked();
        DumpZStateSwitchesLocked();
        DumpZStateValidationTableLocked();
        DumpZStateClusterTableLocked();
        DumpZClusterComparisonLocked();
        DumpZStateStabilitySummaryLocked();
    }

    if constexpr (kEnableLegacyZStateLogs) {
        if (g_cooldownPacketWatch.hotkeyName == "Z" && !g_cooldownPacketWatch.zStateSnapshots.empty()) {
        g_recentZStateSnapshots.insert(g_recentZStateSnapshots.end(),
                                       g_cooldownPacketWatch.zStateSnapshots.begin(),
                                       g_cooldownPacketWatch.zStateSnapshots.end());
        constexpr size_t kMaxRecentZSnapshots = 128;
        if (g_recentZStateSnapshots.size() > kMaxRecentZSnapshots) {
            g_recentZStateSnapshots.erase(
            g_recentZStateSnapshots.begin(),
            g_recentZStateSnapshots.begin() +
                static_cast<std::ptrdiff_t>(g_recentZStateSnapshots.size() - kMaxRecentZSnapshots));
        }
    }
    }

    DumpAutoRunSummaryLocked();

    if (!IsBridgeOnlyModeEnabled()) {
        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [CDHUNT  ] stop"
            << " run_id=" << g_cooldownPacketWatch.runId
            << " hotkey=" << g_cooldownPacketWatch.hotkeyName
            << " observed_ms=" << (g_cooldownPacketWatch.untilTickMs - g_cooldownPacketWatch.startedTickMs);
        AppendLogLine(oss.str());
    }
    g_cooldownPacketWatch = {};
    StopInternalPollingWatcherLocked();
    g_completed0017Groups.clear();
    g_open0017Group.reset();
}

void ProcessPendingDebugAssistSnapshots(unsigned long long now) {
    while (!g_pendingDebugAssistSnapshots.empty()) {
        const PendingDebugAssistSnapshot& pending = g_pendingDebugAssistSnapshots.front();
        if (pending.dueTickMs > now) {
            break;
        }

        if (pending.runId != 0 &&
            (!g_cooldownPacketWatch.active || pending.runId != g_cooldownPacketWatch.runId)) {
            g_pendingDebugAssistSnapshots.pop_front();
            continue;
        }

        PacketUseEvent e{};
        e.archiveKey = pending.archiveKey;
        e.hotkeyName = pending.hotkeyName;
        e.packetTickMs = pending.packetTickMs;
        e.runId = pending.runId;
        const SkillbarSnapshot snap = g_skillReader.readNoWait();
        CaptureDebugAssistSnapshot(pending.phaseName.c_str(), e, snap);
        g_pendingDebugAssistSnapshots.pop_front();
    }
}

void DumpCooldownProbe(const char* phaseName,
                       const PacketUseEvent& e,
                       uint32_t mappedSlot,
                       const SkillbarSnapshot& baseline,
                       const SkillbarSnapshot& current) {
    if (!kEnableCooldownProbe) {
        return;
    }
    if (mappedSlot == UINT32_MAX) {
        AppendLogLine("[" + BuildTimestampForLog() + "] [CDPROBE ] phase=" + std::string(phaseName) +
                      " hotkey=" + e.hotkeyName + " archive_key=" + e.archiveKey + " slot=<unmapped>");
        return;
    }

    const SkillRechargeSweepResult gameChar =
        g_skillReader.requestRechargeSweepOnGameThread(SkillbarObjectKind::CharSkillbar, 10, 150);
    const SkillRechargeSweepResult gameChCli =
        g_skillReader.requestRechargeSweepOnGameThread(SkillbarObjectKind::ChCliSkillbar, 10, 150);

    std::ostringstream oss;
    oss << "[" << BuildTimestampForLog() << "] [CDPROBE ] "
        << "phase=" << phaseName
        << " hotkey=" << e.hotkeyName
        << " archive_key=" << e.archiveKey
        << " slot=" << mappedSlot
        << " game_char_slot=" << FormatSweepEntry(gameChar, mappedSlot)
        << " game_chcli_slot=" << FormatSweepEntry(gameChCli, mappedSlot)
        << " game_char_all=" << FormatSweepSummary(gameChar)
        << " game_chcli_all=" << FormatSweepSummary(gameChCli)
        << " ptr48=0x" << std::hex << current.ptr48Addr
        << " ptr60=0x" << current.ptr60Addr
        << std::dec;
    AppendLogLine(oss.str());

    if (baseline.valid && current.valid) {
        AppendLogLine("[" + BuildTimestampForLog() + "] [CDPROBE ] phase=" + std::string(phaseName) + " " +
                      BuildChangedCandidateSummary(baseline.rawPtr48, current.rawPtr48, sizeof(current.rawPtr48), "ptr48"));
        AppendLogLine("[" + BuildTimestampForLog() + "] [CDPROBE ] phase=" + std::string(phaseName) + " " +
                      BuildChangedCandidateSummary(baseline.rawPtr60, current.rawPtr60, sizeof(current.rawPtr60), "ptr60"));
    }
}

bool ReadPtrSafe(uint64_t addr, uint64_t& out) {
    out = 0;
    if (!addr) {
        return false;
    }
    __try {
        out = *reinterpret_cast<uint64_t*>(addr);
        return out != 0;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

void DumpVtableForObject(const char* phaseName,
                         const char* targetName,
                         const PacketUseEvent& e,
                         bool fresh0017,
                         uint64_t charSkillbar,
                         uint64_t obj,
                         int count) {
    uint64_t vtbl = 0;
    const bool haveVtable = ReadPtrSafe(obj, vtbl);

    {
        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [VTBLDUMP] "
            << "phase=" << phaseName
            << " fresh0017=" << (fresh0017 ? 1 : 0)
            << " hotkey=" << e.hotkeyName
            << " archive_key=" << e.archiveKey
            << " tick=" << e.packetTickMs
            << " target=" << targetName
            << " charSkillbar=0x" << std::hex << charSkillbar
            << " obj=0x" << obj
            << " vtbl=";
        if (haveVtable) {
            oss << "0x" << vtbl;
        } else {
            oss << "<invalid>";
        }
        oss
            << std::dec;
        AppendLogLine(oss.str());
    }

    if (!haveVtable) {
        return;
    }

    for (int i = 0; i < count; ++i) {
        uint64_t fn = 0;
        const uint64_t slotAddr = vtbl + static_cast<uint64_t>(i) * 8;
        const bool ok = ReadPtrSafe(slotAddr, fn);
        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [VTBLDUMP] "
            << "phase=" << phaseName
            << " target=" << targetName
            << " obj=0x" << std::hex << obj
            << " fn[" << std::setw(2) << std::setfill('0') << i << "]=";
        if (ok) {
            oss << "0x" << fn;
        } else {
            oss << "<invalid>";
        }
        oss << std::dec;
        AppendLogLine(oss.str());
    }
}

void DumpSkillbarChildrenVtables(const char* phaseName,
                                 const PacketUseEvent& e,
                                 bool fresh0017,
                                 const SkillbarSnapshot& snap) {
    if (!kEnableVerboseVtableDump) {
        return;
    }
    if (!snap.valid || !snap.charSkillbarAddr) {
        AppendLogLine("[" + BuildTimestampForLog() + "] [VTBLDUMP] phase=" + std::string(phaseName) +
                      " fresh0017=" + (fresh0017 ? std::string("1") : std::string("0")) +
                      " hotkey=" + e.hotkeyName + " archive_key=" + e.archiveKey + " snapshot=invalid");
        return;
    }

    {
        std::ostringstream oss;
        oss << "[" << BuildTimestampForLog() << "] [VTBLDUMP] "
            << "phase=" << phaseName
            << " fresh0017=" << (fresh0017 ? 1 : 0)
            << " hotkey=" << e.hotkeyName
            << " archive_key=" << e.archiveKey
            << " tick=" << e.packetTickMs
            << " charSkillbar=0x" << std::hex << snap.charSkillbarAddr
            << " ptr40=0x" << snap.ptr40Addr
            << " ptr48=0x" << snap.ptr48Addr
            << " ptr60=0x" << snap.ptr60Addr
            << std::dec;
        AppendLogLine(oss.str());
    }

    DumpVtableForObject(phaseName, "ptr40", e, fresh0017, snap.charSkillbarAddr, snap.ptr40Addr, 0x20);
    DumpVtableForObject(phaseName, "ptr48", e, fresh0017, snap.charSkillbarAddr, snap.ptr48Addr, 0x20);
    DumpVtableForObject(phaseName, "ptr60", e, fresh0017, snap.charSkillbarAddr, snap.ptr60Addr, 0x20);
}

void QueueVtableDumpPhases(const PacketUseEvent& e, const SkillbarSnapshot& baselineSnapshot) {
    if (!kEnableCooldownProbe) {
        return;
    }
    const unsigned long long base = GetTickCount64();
    const uint32_t mappedSlot = TryMapProbeSlot(e).value_or(UINT32_MAX);
    g_pendingVtableDumps.push_back({"t0", e.archiveKey, e.hotkeyName, e.packetTickMs, base, mappedSlot, baselineSnapshot});
    g_pendingVtableDumps.push_back({"+250ms", e.archiveKey, e.hotkeyName, e.packetTickMs, base + 250, mappedSlot, baselineSnapshot});
    g_pendingVtableDumps.push_back({"+1000ms", e.archiveKey, e.hotkeyName, e.packetTickMs, base + 1000, mappedSlot, baselineSnapshot});
}

void ProcessPendingVtableDumps(unsigned long long now) {
    while (!g_pendingVtableDumps.empty()) {
        const PendingVtableDump& pending = g_pendingVtableDumps.front();
        if (pending.dueTickMs > now) {
            break;
        }

        PacketUseEvent e{};
        e.archiveKey = pending.archiveKey;
        e.hotkeyName = pending.hotkeyName;
        e.packetTickMs = pending.packetTickMs;

        const SkillbarSnapshot snap = g_skillReader.readNoWait();
        DumpSkillbarChildrenVtables(pending.phaseName.c_str(), e, pending.phaseName == "t0", snap);
        DumpCooldownProbe(pending.phaseName.c_str(), e, pending.mappedSlot, pending.baselineSnapshot, snap);
        g_pendingVtableDumps.pop_front();
    }
}

void StartRechargeTimer(TrackedSkillState& s, unsigned long long now) {
    auto* c = GetConfig(s);
    if (!c) return;
    int ms = c->ammoRechargeMs > 0 ? c->ammoRechargeMs : c->rechargeMs;
    if (ms > 0) s.rechargeTimers.push_back({now + static_cast<unsigned long long>(ms)});
}

void HandleUseEvent(TrackedSkillState& s, const PacketUseEvent& e) {
    auto* c = GetConfig(s);
    if (!c) return;

    s.lastArchiveKey = e.archiveKey;
    std::string detail = "archive_key=" + e.archiveKey + " | source=packet";
    if (c->acrExcluded) {
        detail += " | acr_excluded";
    }
    LogEvent("USED", s, detail);

    s.wasCastable = IsCastable(s, e.packetTickMs);
    s.wasFull = IsFull(s, e.packetTickMs);
}

void ProcessTimers(TrackedSkillState& s, unsigned long long now) {
    s.wasCastable = IsCastable(s, now);
    s.wasFull = IsFull(s, now);
}

std::string BuildStatusLineUnlocked(unsigned long long now) {
    (void)now;
    size_t nodeReady = 0;
    size_t nodeCd = 0;
    for (const auto& s : g_skillStates) {
        if (s.nodeState.stateText == "NODE_READY" || s.nodeState.stateText == "READY_BASELINE") ++nodeReady;
        else if (s.nodeState.stateText == "NODE_CD" || s.nodeState.stateText == "CD_STATE") ++nodeCd;
    }
    std::ostringstream oss;
    oss << "Skill CD tracker active. node_ready=" << nodeReady
        << " node_cd=" << nodeCd;
    return oss.str();
}

void RebuildTrackedStatesUnlocked() {
    const auto& table = GetSkillTable();
    g_skillStates.clear();
    g_skillStates.reserve(table.size());
    for (size_t i = 0; i < table.size(); ++i) {
        TrackedSkillState s{};
        s.configIndex = i;
        s.currentCharges = std::max(1, table[i].maxCharges);
        s.nodeState.stateText = "NODE_UNKNOWN";
        g_skillStates.push_back(std::move(s));
    }
}

void WorkerLoop() {
    unsigned long long nextLiveRefreshTickMs = 0;
    unsigned long long nextRuntimeDiagTickMs = 0;
    while (g_running.load(std::memory_order_acquire)) {
        auto now = GetTickCount64();
        unsigned long idleSleepMs = 100;
        bool liveSnapshotNeeded = false;
        bool liveSnapshotExecuted = false;
        if (kx::g_globalPauseMode.load(std::memory_order_acquire)) {
            {
                std::lock_guard<std::mutex> lock(g_stateMutex);
                StopCooldownPacketWatchIfExpired(now);
                g_pendingVtableDumps.clear();
                g_pendingDebugAssistSnapshots.clear();
                g_statusLine = "Skill cooldown tracker paused.";
            }
            Sleep(100);
            continue;
        }
        {
            std::lock_guard<std::mutex> lock(g_stateMutex);
            StopCooldownPacketWatchIfExpired(now);
            ProcessInternalPollingWatcherLocked(now);
            if (PacketUseEvent e = SnapshotFreshUseEvent(); e.packetTickMs != 0) {
                g_lastProcessedPacketTickMs = e.packetTickMs;
                const SkillbarSnapshot baselineSnapshot = g_skillReader.readNoWait();
                if (!IsBridgeOnlyModeEnabled()) {
                    QueueVtableDumpPhases(e, baselineSnapshot);
                }
                const SkillConfig* cfg = FindConfigByArchiveKey(e.archiveKey);
                if (!cfg) cfg = FindConfigByHotkey(e.hotkeyName);
                const std::string normalizedHotkey = NormalizeDebugHotkeyName(e.hotkeyName);
                const bool shouldStartWatch =
                    (cfg != nullptr || IsBridgeOnlyModeEnabled()) &&
                    ShouldStartCooldownPacketWatchLocked(e, now);
                if (shouldStartWatch) {
                    StartCooldownPacketWatch(e, baselineSnapshot);
                }
                const bool shouldAutoCapture =
                    kEnableAutoDebugAssistSnapshots &&
                    g_debugAssistState.autoMode &&
                    g_debugAssistState.enabled &&
                    IsAutoDebugHotkey(normalizedHotkey);
                const bool shouldManualCapture = g_debugAssistState.enabled && g_debugAssistState.armed &&
                    normalizedHotkey == NormalizeDebugHotkeyName(g_debugAssistState.targetHotkey);
                if (!IsBridgeOnlyModeEnabled() && (shouldAutoCapture || shouldManualCapture)) {
                    CaptureDebugAssistSnapshot("t0", e, baselineSnapshot);
                    QueueDebugAssistPhases(e, cfg);
                }
                if (cfg) {
                    if (auto* state = FindStateByHotkey(cfg->hotkeyName)) {
                        HandleUseEvent(*state, e);
                    }
                } else {
                    AppendLogLine("[" + BuildTimestampForLog() + "] [USED    ] " + e.hotkeyName + " | archive_key=" + e.archiveKey + " | unmapped skill config");
                }
            }

            if (!IsBridgeOnlyModeEnabled()) {
                ProcessPendingVtableDumps(now);
                ProcessPendingDebugAssistSnapshots(now);
            } else {
                g_pendingVtableDumps.clear();
                g_pendingDebugAssistSnapshots.clear();
            }

            liveSnapshotNeeded =
                (g_cooldownPacketWatch.active && !g_cooldownPacketWatch.loggedReadyReturn) ||
                (!IsBridgeOnlyModeEnabled() &&
                 (!g_pendingDebugAssistSnapshots.empty() || !g_pendingVtableDumps.empty()));
            if (g_internalPollingWatcher.active) {
                idleSleepMs = static_cast<unsigned long>(kInternalPollIntervalMs);
            }
            if (liveSnapshotNeeded && now >= nextLiveRefreshTickMs) {
                const SkillbarSnapshot liveSnapshot = g_skillReader.readNoWait();
                std::string watchedOldStateText = "UNKNOWN";
                std::string watchedNewStateText = "UNKNOWN";
                std::string zOldStateText = "UNKNOWN";
                if (g_cooldownPacketWatch.active) {
                    if (auto* watchedState = FindStateByHotkey(g_cooldownPacketWatch.hotkeyName)) {
                        watchedOldStateText = watchedState->nodeState.stateText;
                    }
                }
                for (auto& s : g_skillStates) {
                    const auto* cfg = GetConfig(s);
                    if (cfg && cfg->hotkeyName == "Z") {
                        zOldStateText = s.nodeState.stateText;
                    }
                    RefreshNodeStateForHotkey(s, liveSnapshot);
                }
                if (g_cooldownPacketWatch.active) {
                    if (auto* watchedState = FindStateByHotkey(g_cooldownPacketWatch.hotkeyName)) {
                        watchedNewStateText = watchedState->nodeState.stateText;
                        PacketUseEvent watchEvent{};
                        watchEvent.archiveKey = g_cooldownPacketWatch.archiveKey;
                        watchEvent.hotkeyName = g_cooldownPacketWatch.hotkeyName;
                        watchEvent.packetTickMs = now;
                        watchEvent.runId = g_cooldownPacketWatch.runId;

                        if (!g_cooldownPacketWatch.loggedSkillbarNodeCd &&
                            IsCooldownLikeNodeState(watchedNewStateText) &&
                            watchedOldStateText != watchedNewStateText) {
                            LogSkillbarMemorySnapshotLocked("node_cd",
                                                            watchEvent,
                                                            liveSnapshot,
                                                            &g_cooldownPacketWatch.baselineSnapshot);
                            g_cooldownPacketWatch.loggedSkillbarNodeCd = true;
                        }

                        if (!g_cooldownPacketWatch.loggedSkillbarNodeReady &&
                            g_cooldownPacketWatch.loggedCooldownStart &&
                            IsReadyLikeNodeState(watchedNewStateText) &&
                            watchedOldStateText != watchedNewStateText) {
                            LogSkillbarMemorySnapshotLocked("node_ready",
                                                            watchEvent,
                                                            liveSnapshot,
                                                            &g_cooldownPacketWatch.baselineSnapshot);
                            g_cooldownPacketWatch.loggedSkillbarNodeReady = true;
                        }
                    }
                }
                if (g_cooldownPacketWatch.active && g_cooldownPacketWatch.hotkeyName == "Z") {
                    if (auto* zState = FindStateByHotkey("Z")) {
                        std::ostringstream phaseName;
                        phaseName << "live@" << FormatElapsedSeconds(
                            now > g_cooldownPacketWatch.startedTickMs ? (now - g_cooldownPacketWatch.startedTickMs) : 0);
                        RecordZStateSnapshotLocked(phaseName.str().c_str(),
                                                   zOldStateText,
                                                   zState->nodeState.stateText,
                                                   liveSnapshot);
                    }
                }
                nextLiveRefreshTickMs = now + 250;
                idleSleepMs = 75;
                liveSnapshotExecuted = true;
            }
            if (g_internalPollingWatcher.active) {
                idleSleepMs = static_cast<unsigned long>(kInternalPollIntervalMs);
            }
            for (auto& s : g_skillStates) ProcessTimers(s, now);
            g_statusLine = BuildStatusLineUnlocked(now);

            if (!IsBridgeOnlyModeEnabled() &&
                !kx::g_globalPauseMode.load(std::memory_order_acquire) &&
                now >= nextRuntimeDiagTickMs) {
                LogRuntimeDiagnostics(now,
                                      liveSnapshotNeeded,
                                      liveSnapshotExecuted,
                                      nextLiveRefreshTickMs,
                                      idleSleepMs);
                nextRuntimeDiagTickMs = now + 1000;
            }
        }
        Sleep(idleSleepMs);
    }
}

} // namespace

bool Initialize() {
    if (g_running.exchange(true, std::memory_order_acq_rel)) return true;
    RuntimeDiagFilePath().clear();
    EnsureRuntimeDiagFilePath();
    AppendRuntimeDiagLine("[" + BuildTimestampForLog() + "] [RUNTIME ] initialize");
    kx::SkillDb::Initialize();
    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        g_statusLine = "Skill cooldown tracker starting...";
        RebuildTrackedStatesUnlocked();
        g_pendingVtableDumps.clear();
        g_pendingDebugAssistSnapshots.clear();
        g_debugAssistState.statusText = "Debug Assist auto mode ready. Press one tracked hotkey once.";
        g_debugAssistState.lastSnapshotText = "No debug snapshot captured yet.";
    }
    g_lastProcessedPacketTickMs = 0;
    g_worker = std::thread(WorkerLoop);
    return true;
}

void Shutdown() {
    if (!g_running.exchange(false, std::memory_order_acq_rel)) return;
    if (g_worker.joinable()) g_worker.join();
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_pendingVtableDumps.clear();
    g_pendingDebugAssistSnapshots.clear();
    StopInternalPollingWatcherLocked();
    AppendRuntimeDiagLine("[" + BuildTimestampForLog() + "] [RUNTIME ] shutdown");
}

std::string GetStatusLine() {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return g_statusLine;
}

std::string GetApiStatusLine() {
    return kx::SkillDb::GetApiStatusLine();
}

std::string GetDbStatusLine() {
    return kx::SkillDb::GetDbStatusLine();
}

bool ReloadBuildData() {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    const bool ok = kx::SkillDb::ReloadBuildData();
    RebuildTrackedStatesUnlocked();
    g_pendingVtableDumps.clear();
    g_pendingDebugAssistSnapshots.clear();
    return ok;
}

bool RefreshApiData() {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    const bool ok = kx::SkillDb::RefreshApiData();
    RebuildTrackedStatesUnlocked();
    g_pendingVtableDumps.clear();
    g_pendingDebugAssistSnapshots.clear();
    return ok;
}

bool SaveCurrentProfile() {
    return kx::SkillDb::SaveCurrentProfile();
}

bool OpenOverridesFile() {
    return kx::SkillDb::OpenOverridesFile();
}

bool IsBridgeOnlyMode() {
    return IsBridgeOnlyModeEnabled();
}

void SetBridgeOnlyMode(bool enabled) {
    // Deprecated: bridge-only integration removed. No-op maintained for compatibility.
    AppendLogLine("[" + BuildTimestampForLog() + "] [MODE    ] BridgeOnly mode request ignored (deprecated)");
}

DebugAssistState GetDebugAssistState() {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return g_debugAssistState;
}

EstimatedCooldownDisplay GetEstimatedCooldownDisplay() {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    EstimatedCooldownDisplay out{};
    if (!g_estimatedCooldownState.available) {
        return out;
    }

    out.available = true;
    out.estimated = g_estimatedCooldownState.estimated;
    out.hotkey = g_estimatedCooldownState.hotkey;
    out.skillName = g_estimatedCooldownState.skillName;
    out.durationMs = g_estimatedCooldownState.durationMs;
    out.ptr40Value = g_estimatedCooldownState.ptr40Value;
    out.ptr60Value = g_estimatedCooldownState.ptr60Value;

    if (!g_estimatedCooldownState.cooldownActive || g_estimatedCooldownState.cdStartTickMs == 0 || out.durationMs <= 0) {
        out.stateText = "READY";
        out.remainingMs = 0;
        out.progress01 = 1.0f;
        return out;
    }

    const unsigned long long now = GetTickCount64();
    const unsigned long long elapsed = now > g_estimatedCooldownState.cdStartTickMs
        ? (now - g_estimatedCooldownState.cdStartTickMs)
        : 0;
    const int remaining = out.durationMs - static_cast<int>((std::min)(
        elapsed,
        static_cast<unsigned long long>((std::numeric_limits<int>::max)())));
    out.remainingMs = (std::max)(remaining, 0);
    out.stateText = out.remainingMs > 0 ? "COOLDOWN" : "READY";
    const float elapsedRatio = out.durationMs > 0
        ? static_cast<float>((std::min)(elapsed, static_cast<unsigned long long>(out.durationMs))) / static_cast<float>(out.durationMs)
        : 1.0f;
    out.progress01 = std::clamp(elapsedRatio, 0.0f, 1.0f);
    if (out.remainingMs == 0) {
        out.progress01 = 1.0f;
    }
    return out;
}

void SetDebugAssistEnabled(bool enabled) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_debugAssistState.enabled = enabled;
    if (!enabled) {
        g_debugAssistState.armed = false;
        g_debugAssistState.statusText = "Debug Assist disabled.";
    } else if (g_debugAssistState.autoMode) {
        g_debugAssistState.statusText = "Debug Assist auto mode ready. Press one tracked hotkey once.";
    } else if (g_debugAssistState.statusText.empty() || g_debugAssistState.statusText == "Debug Assist disabled.") {
        g_debugAssistState.statusText = "Debug Assist enabled. Arm it, then press the target hotkey once.";
    }
}

void SetDebugAssistBreakOnMatch(bool enabled) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_debugAssistState.breakOnMatch = enabled;
}

void SetDebugAssistTargetHotkey(const std::string& hotkey) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_debugAssistState.targetHotkey = NormalizeDebugHotkeyName(hotkey);
    g_debugAssistState.statusText = "Debug Assist target set to " + g_debugAssistState.targetHotkey + ".";
}

void SetDebugAssistAutoMode(bool enabled) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_debugAssistState.autoMode = enabled;
    if (enabled) {
        g_debugAssistState.armed = false;
        g_debugAssistState.statusText = "Debug Assist auto mode ready. Press one tracked hotkey once.";
    } else {
        g_debugAssistState.statusText = "Debug Assist manual mode ready. Arm it, then press the target hotkey once.";
    }
}

void ArmDebugAssistOnce() {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_debugAssistState.enabled = true;
    g_debugAssistState.autoMode = false;
    g_debugAssistState.armed = true;
    g_debugAssistState.statusText = "Debug Assist armed for " + g_debugAssistState.targetHotkey + ". Press that hotkey once.";
}

void ClearDebugAssistSnapshot() {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_debugAssistState.lastSnapshotText = "No debug snapshot captured yet.";
    if (g_debugAssistState.enabled) {
        g_debugAssistState.statusText = g_debugAssistState.autoMode
            ? "Debug Assist auto mode ready. Press one tracked hotkey once."
            : "Debug Assist ready.";
    }
}

void NoteReceivedPacket(const PacketInfo& packet) {
    if (packet.direction != PacketDirection::Received ||
        packet.specialType != InternalPacketType::NORMAL) {
        return;
    }

    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (!g_cooldownPacketWatch.active) {
        return;
    }

    const unsigned long long now = GetTickCount64();
    if (now > g_cooldownPacketWatch.untilTickMs) {
        StopCooldownPacketWatchIfExpired(now);
        return;
    }

    const unsigned long long elapsedMs =
        now > g_cooldownPacketWatch.startedTickMs ? (now - g_cooldownPacketWatch.startedTickMs) : 0;

    if (!IsBridgeOnlyModeEnabled() && ShouldTrackCandidateSmsg(packet)) {
        g_cooldownPacketWatch.candidateSmsgs.push_back({
            elapsedMs,
            packet.rawHeaderId,
            packet.name,
            packet.data
        });
        if (g_cooldownPacketWatch.candidateSmsgs.size() > kMaxCandidateSmsgRecords) {
            g_cooldownPacketWatch.candidateSmsgs.erase(g_cooldownPacketWatch.candidateSmsgs.begin());
        }
    }

    if (packet.rawHeaderId == static_cast<uint16_t>(kx::SMSG_HeaderId::SKILL_UPDATE)) {
        g_cooldownPacketWatch.all0017Packets.push_back({elapsedMs, packet.data});
        if (g_cooldownPacketWatch.all0017Packets.size() > kMax0017Packets) {
            g_cooldownPacketWatch.all0017Packets.erase(g_cooldownPacketWatch.all0017Packets.begin());
        }

        if (!IsBridgeOnlyModeEnabled() &&
            g_cooldownPacketWatch.haveNodeReadyOffset &&
            !g_cooldownPacketWatch.dumpedNodeReadyWindow &&
            elapsedMs > g_cooldownPacketWatch.nodeReadyOffsetMs + 3000) {
            DumpNodeReadyWindowLocked();
        }
    }

    if (IsBridgeOnlyModeEnabled()) {
        if (packet.rawHeaderId == static_cast<uint16_t>(kx::SMSG_HeaderId::AGENT_ATTRIBUTE_UPDATE)) {
            g_cooldownPacketWatch.all0028Packets.push_back({elapsedMs, packet.rawHeaderId, packet.name, packet.data});
            if (g_cooldownPacketWatch.all0028Packets.size() > kMax0028Packets) {
                g_cooldownPacketWatch.all0028Packets.erase(g_cooldownPacketWatch.all0028Packets.begin());
            }
        }
        return;
    }

    if (g_open0017Group.has_value()) {
        const Packet0017Group& current = g_open0017Group.value();
        if (elapsedMs > current.lastElapsedMs && (elapsedMs - current.lastElapsedMs) > 200) {
            FlushOpen0017GroupLocked();
        }
    }

    if (packet.rawHeaderId == static_cast<uint16_t>(kx::SMSG_HeaderId::SKILL_UPDATE)) {
        if (!g_open0017Group.has_value()) {
            Packet0017Group group;
            group.firstElapsedMs = elapsedMs;
            group.lastElapsedMs = elapsedMs;
            group.entries.push_back({elapsedMs, packet.data});
            g_open0017Group = std::move(group);
            return;
        }

        Packet0017Group& current = g_open0017Group.value();
        if (elapsedMs > current.lastElapsedMs && (elapsedMs - current.lastElapsedMs) > 200) {
            FlushOpen0017GroupLocked();
            Packet0017Group next;
            next.firstElapsedMs = elapsedMs;
            next.lastElapsedMs = elapsedMs;
            next.entries.push_back({elapsedMs, packet.data});
            g_open0017Group = std::move(next);
            return;
        }

        current.lastElapsedMs = elapsedMs;
        current.entries.push_back({elapsedMs, packet.data});
        return;
    }

    if (packet.rawHeaderId == static_cast<uint16_t>(kx::SMSG_HeaderId::AGENT_ATTRIBUTE_UPDATE)) {
        g_cooldownPacketWatch.all0028Packets.push_back({elapsedMs, packet.rawHeaderId, packet.name, packet.data});
        if (g_cooldownPacketWatch.all0028Packets.size() > kMax0028Packets) {
            g_cooldownPacketWatch.all0028Packets.erase(g_cooldownPacketWatch.all0028Packets.begin());
        }
    }
}

void NotePtrWriteHit(PtrWriteHitKind kind,
                     std::uintptr_t hitAddr,
                     uint64_t newValue,
                     std::uintptr_t rip,
                     uint32_t slot,
                     uint32_t phase) {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    if (!g_cooldownPacketWatch.active) {
        return;
    }

    const unsigned long long now = GetTickCount64();
    if (now > g_cooldownPacketWatch.untilTickMs) {
        StopCooldownPacketWatchIfExpired(now);
        return;
    }

    const unsigned long long elapsedMs =
        now > g_cooldownPacketWatch.startedTickMs ? (now - g_cooldownPacketWatch.startedTickMs) : 0;

    const auto oldIt = g_lastWatchValues.find(hitAddr);
    const bool haveOldValue = oldIt != g_lastWatchValues.end();
    const uint64_t oldValue = haveOldValue ? oldIt->second : 0;
    RecordPtrWriteHitLocked(kind,
                            hitAddr,
                            haveOldValue,
                            oldValue,
                            newValue,
                            static_cast<uintptr_t>(rip),
                            slot,
                            phase,
                            "external_bridge");

    std::ostringstream oss;
    oss << "[" << BuildTimestampForLog() << "] [KXBRIDGE] "
        << "run_id=" << g_cooldownPacketWatch.runId
        << " offset=" << FormatElapsedSeconds(elapsedMs)
        << " hit kind=" << PtrWriteHitKindToString(kind)
        << " hit_addr=0x" << std::hex << static_cast<uintptr_t>(hitAddr)
        << " old_value=";
    if (haveOldValue) {
        oss << "0x" << oldValue;
    } else {
        oss << "<unknown>";
    }
    oss
        << " new_value=0x" << newValue
        << " rip=0x" << std::hex << static_cast<uintptr_t>(rip) << std::dec
        << " slot=";
    if (slot == UINT32_MAX) {
        oss << "na";
    } else {
        oss << slot;
    }
    oss << " phase=" << FormatBridgePhase(phase)
        << " phase_code=" << phase;
    AppendLogLine(oss.str());
}

} // namespace kx::SkillCooldown
