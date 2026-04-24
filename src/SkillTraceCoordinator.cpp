#include "SkillTraceCoordinator.h"

#include "AppState.h"
#include "Log.h"
#include "SkillDb.h"
#include "SkillMemoryReader.h"

#include <windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iomanip>
#include <mutex>
#include <optional>
#include <set>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

extern HINSTANCE dll_handle;

namespace kx::SkillTrace {
namespace {

constexpr const char* kProbeRootDirName = "skillbar-probe";
constexpr unsigned long long kProbePollMs = 100;
constexpr unsigned long long kProbeTimeoutMs = 20000;
constexpr size_t kStableSampleCount = 3;

enum class ProbePhase {
    Idle,
    CapturingBaseline,
    WaitingForChange,
    CapturingChanged,
    Completed,
    TimedOut,
};

struct ProbeSnapshot {
    unsigned long long tickMs = 0;
    SkillbarSnapshot snap;
};

struct RankedOffsetCandidate {
    size_t offset = 0;
    int confidence = 0;
    int changedCount = 0;
    int untouchedCount = 0;
    int plausibleCount = 0;
    bool stableBefore = false;
    bool stableAfter = false;
    bool valuesUnique = false;
    std::array<uint32_t, 5> beforeValues = {};
    std::array<uint32_t, 5> afterValues = {};
    std::array<uint64_t, 5> beforeU64 = {};
    std::array<uint64_t, 5> afterU64 = {};
};

struct SkillbarProbeSession {
    uint64_t sessionId = 0;
    std::string scenarioName;
    ProbePhase phase = ProbePhase::Idle;
    unsigned long long startedTickMs = 0;
    unsigned long long phaseStartedTickMs = 0;
    unsigned long long lastPollTickMs = 0;
    std::vector<ProbeSnapshot> baselineSamples;
    std::vector<ProbeSnapshot> changedSamples;
    ProbeSnapshot baselineStable;
    ProbeSnapshot changedStable;
    std::string outputDir;
    std::string summary;
};

std::atomic<bool> g_running = false;
std::thread g_worker;
std::mutex g_stateMutex;
std::mutex g_probeMutex;
std::string g_statusLine = "Skillbar auto-identification idle.";
std::string g_lastTraceSummary = "Legacy auto trace disabled; use skillbar probe.";
std::string g_probeStatusLine = "Skillbar probe idle.";
std::string g_lastProbeSummary = "No skillbar probe completed yet.";
std::optional<SkillbarProbeSession> g_activeProbe;
std::vector<LiveSkillbarRow> g_liveRows;
uint64_t g_nextProbeSessionId = 1;
std::optional<RankedOffsetCandidate> g_bestWeaponCandidate;
std::optional<RankedOffsetCandidate> g_bestProfessionCandidate;
unsigned long long g_lastAutoProbeHotkeyTickMs = 0;

uint32_t ReadLe32Ui(const uint8_t* data, size_t offset) {
    return static_cast<uint32_t>(data[offset]) |
           (static_cast<uint32_t>(data[offset + 1]) << 8) |
           (static_cast<uint32_t>(data[offset + 2]) << 16) |
           (static_cast<uint32_t>(data[offset + 3]) << 24);
}

uint64_t ReadLe64Ui(const uint8_t* data, size_t offset) {
    uint64_t value = 0;
    for (size_t i = 0; i < 8; ++i) {
        value |= (static_cast<uint64_t>(data[offset + i]) << (i * 8));
    }
    return value;
}

std::string BuildByteDumpText(const uint8_t* data, size_t size) {
    std::ostringstream oss;
    for (size_t row = 0; row < size / 16; ++row) {
        const size_t off = row * 16;
        char line[160];
        snprintf(line, sizeof(line),
                 "%03zX: %02X %02X %02X %02X  %02X %02X %02X %02X  "
                 "%02X %02X %02X %02X  %02X %02X %02X %02X",
                 off,
                 data[off + 0], data[off + 1], data[off + 2], data[off + 3],
                 data[off + 4], data[off + 5], data[off + 6], data[off + 7],
                 data[off + 8], data[off + 9], data[off + 10], data[off + 11],
                 data[off + 12], data[off + 13], data[off + 14], data[off + 15]);
        oss << line << '\n';
    }
    return oss.str();
}

std::string HotkeyForSlotIndex(int slotIndex) {
    static const char* names[] = {
        "Weapon1(1)", "Weapon2(2)", "Weapon3(3)", "Weapon4(4)", "Weapon5(5)",
        "Profession1(F1)", "Profession2(F2)", "Profession3(F3)", "Profession4(F4)", "Profession5(F5)"
    };
    if (slotIndex < 0 || slotIndex >= static_cast<int>(std::size(names))) {
        return "Unknown";
    }
    return names[slotIndex];
}

std::string GetDllDirectory() {
    char modulePath[MAX_PATH] = {};
    if (!GetModuleFileNameA(dll_handle, modulePath, MAX_PATH)) {
        return ".";
    }
    std::string path = modulePath;
    size_t slash = path.find_last_of("\\/");
    return (slash == std::string::npos) ? "." : path.substr(0, slash);
}

void WriteTextFile(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::trunc);
    if (out) {
        out << text;
    }
}

bool IsPlausibleSkillId(uint32_t value) {
    return value >= 1000u && value <= 250000u;
}

uint64_t BuildSnapshotSignature(const SkillbarSnapshot& snap) {
    const uint64_t fnvOffset = 1469598103934665603ull;
    const uint64_t fnvPrime = 1099511628211ull;
    uint64_t hash = fnvOffset;
    const auto mix = [&](const uint8_t* bytes, size_t size) {
        for (size_t i = 0; i < size; ++i) {
            hash ^= bytes[i];
            hash *= fnvPrime;
        }
    };

    if (!snap.valid) {
        return 0;
    }
    mix(snap.rawCharSkillbarWide, sizeof(snap.rawCharSkillbarWide));
    mix(snap.rawPtr48, sizeof(snap.rawPtr48));
    mix(snap.rawPtr60, sizeof(snap.rawPtr60));
    return hash;
}

bool SnapshotSeriesStable(const std::vector<ProbeSnapshot>& samples) {
    if (samples.size() < kStableSampleCount) {
        return false;
    }
    const uint64_t first = BuildSnapshotSignature(samples.front().snap);
    if (!first) {
        return false;
    }
    for (size_t i = 1; i < samples.size(); ++i) {
        if (BuildSnapshotSignature(samples[i].snap) != first) {
            return false;
        }
    }
    return true;
}

bool HasAnyStructuralChange(const SkillbarSnapshot& baseline, const SkillbarSnapshot& current) {
    if (!baseline.valid || !current.valid) {
        return false;
    }
    return std::memcmp(baseline.rawCharSkillbarWide, current.rawCharSkillbarWide, sizeof(baseline.rawCharSkillbarWide)) != 0 ||
           std::memcmp(baseline.rawPtr48, current.rawPtr48, sizeof(baseline.rawPtr48)) != 0 ||
           std::memcmp(baseline.rawPtr60, current.rawPtr60, sizeof(baseline.rawPtr60)) != 0;
}

bool SlotsStableAtOffset(const std::vector<ProbeSnapshot>& samples, int slotStart, int slotEnd, size_t offset) {
    if (samples.empty()) {
        return false;
    }
    for (int slot = slotStart; slot <= slotEnd; ++slot) {
        const uint32_t first = ReadLe32Ui(samples.front().snap.slots[slot].rawBytes, offset);
        for (size_t i = 1; i < samples.size(); ++i) {
            if (ReadLe32Ui(samples[i].snap.slots[slot].rawBytes, offset) != first) {
                return false;
            }
        }
    }
    return true;
}

RankedOffsetCandidate BuildRankedCandidate(const SkillbarProbeSession& session, int slotStart, int slotEnd, size_t offset) {
    RankedOffsetCandidate candidate = {};
    candidate.offset = offset;
    candidate.stableBefore = SlotsStableAtOffset(session.baselineSamples, slotStart, slotEnd, offset);
    candidate.stableAfter = SlotsStableAtOffset(session.changedSamples, slotStart, slotEnd, offset);

    std::set<uint32_t> uniqueAfter;
    for (int slot = slotStart; slot <= slotEnd; ++slot) {
        const size_t local = static_cast<size_t>(slot - slotStart);
        const uint32_t before = ReadLe32Ui(session.baselineStable.snap.slots[slot].rawBytes, offset);
        const uint32_t after = ReadLe32Ui(session.changedStable.snap.slots[slot].rawBytes, offset);
        candidate.beforeValues[local] = before;
        candidate.afterValues[local] = after;
        candidate.beforeU64[local] = ReadLe64Ui(session.baselineStable.snap.slots[slot].rawBytes, offset);
        candidate.afterU64[local] = ReadLe64Ui(session.changedStable.snap.slots[slot].rawBytes, offset);

        if (before != after) {
            ++candidate.changedCount;
        }
        if (IsPlausibleSkillId(before) || IsPlausibleSkillId(after)) {
            ++candidate.plausibleCount;
        }
        if (after != 0) {
            uniqueAfter.insert(after);
        }
    }

    for (int slot = 0; slot < static_cast<int>(session.baselineStable.snap.slots.size()); ++slot) {
        if (slot >= slotStart && slot <= slotEnd) {
            continue;
        }
        if (ReadLe32Ui(session.baselineStable.snap.slots[slot].rawBytes, offset) ==
            ReadLe32Ui(session.changedStable.snap.slots[slot].rawBytes, offset)) {
            ++candidate.untouchedCount;
        }
    }

    candidate.valuesUnique = uniqueAfter.size() >= static_cast<size_t>((std::max)(1, candidate.changedCount));
    candidate.confidence += candidate.changedCount * 3;
    candidate.confidence += candidate.untouchedCount;
    candidate.confidence += candidate.plausibleCount * 2;
    if (candidate.stableBefore) candidate.confidence += 2;
    if (candidate.stableAfter) candidate.confidence += 2;
    if (candidate.valuesUnique) candidate.confidence += 2;
    return candidate;
}

std::vector<RankedOffsetCandidate> RankOffsetCandidates(const SkillbarProbeSession& session, int slotStart, int slotEnd) {
    std::vector<RankedOffsetCandidate> ranked;
    if (!session.baselineStable.snap.valid || !session.changedStable.snap.valid) {
        return ranked;
    }

    for (size_t offset = 0; offset + 8 <= sizeof(session.baselineStable.snap.slots[0].rawBytes); offset += 4) {
        ranked.push_back(BuildRankedCandidate(session, slotStart, slotEnd, offset));
    }

    std::sort(ranked.begin(), ranked.end(), [](const RankedOffsetCandidate& a, const RankedOffsetCandidate& b) {
        if (a.confidence != b.confidence) return a.confidence > b.confidence;
        if (a.changedCount != b.changedCount) return a.changedCount > b.changedCount;
        return a.offset < b.offset;
    });
    if (ranked.size() > 8) {
        ranked.resize(8);
    }
    return ranked;
}

std::string BuildCandidateSummaryText(const char* title, const std::vector<RankedOffsetCandidate>& ranked, int slotStart, int slotEnd) {
    std::ostringstream oss;
    oss << title << "\n";
    if (ranked.empty()) {
        oss << "  No ranked candidates.\n";
        return oss.str();
    }

    for (const auto& candidate : ranked) {
        oss << "  offset=0x" << std::hex << candidate.offset << std::dec
            << " confidence=" << candidate.confidence
            << " changed=" << candidate.changedCount
            << " untouched=" << candidate.untouchedCount
            << " plausible=" << candidate.plausibleCount
            << " stable_before=" << (candidate.stableBefore ? "yes" : "no")
            << " stable_after=" << (candidate.stableAfter ? "yes" : "no")
            << " unique=" << (candidate.valuesUnique ? "yes" : "no") << "\n";
        for (int slot = slotStart; slot <= slotEnd; ++slot) {
            const size_t idx = static_cast<size_t>(slot - slotStart);
            oss << "    " << HotkeyForSlotIndex(slot)
                << " before_u32=" << candidate.beforeValues[idx]
                << " after_u32=" << candidate.afterValues[idx]
                << " before_u64=0x" << std::hex << candidate.beforeU64[idx]
                << " after_u64=0x" << candidate.afterU64[idx]
                << std::dec << "\n";
        }
    }
    return oss.str();
}

std::string BuildProbeSnapshotText(const ProbeSnapshot& snapshot) {
    std::ostringstream out;
    out << "TickMs: " << snapshot.tickMs << "\n";
    out << "Ctx: 0x" << std::hex << snapshot.snap.ctxAddr << "\n";
    out << "ChCliContext: 0x" << snapshot.snap.chCliContextAddr << "\n";
    out << "LocalChar: 0x" << snapshot.snap.localCharAddr << "\n";
    out << "ChCliSkillbar: 0x" << snapshot.snap.chCliSkillbarAddr << "\n";
    out << "CharSkillbar: 0x" << snapshot.snap.charSkillbarAddr << std::dec << "\n";
    out << "PrimaryCharSkillbar\n" << BuildByteDumpText(snapshot.snap.rawCharSkillbar, sizeof(snapshot.snap.rawCharSkillbar)) << "\n";
    out << "WideCharSkillbar\n" << BuildByteDumpText(snapshot.snap.rawCharSkillbarWide, sizeof(snapshot.snap.rawCharSkillbarWide)) << "\n";
    out << "ChCliSkillbar\n" << BuildByteDumpText(snapshot.snap.rawChCliSkillbar, sizeof(snapshot.snap.rawChCliSkillbar)) << "\n";
    for (const auto& slot : snapshot.snap.slots) {
        out << "Slot[" << slot.slotIndex << "] addr=0x" << std::hex << slot.slotAddr << std::dec << "\n";
        out << BuildByteDumpText(slot.rawBytes, sizeof(slot.rawBytes));
    }
    return out.str();
}

std::string BuildWideDiffText(const ProbeSnapshot& before, const ProbeSnapshot& after) {
    std::ostringstream oss;
    std::vector<size_t> changedOffsets;
    for (size_t i = 0; i < sizeof(before.snap.rawCharSkillbarWide); ++i) {
        if (before.snap.rawCharSkillbarWide[i] != after.snap.rawCharSkillbarWide[i]) {
            changedOffsets.push_back(i);
        }
    }

    oss << "Wide diff groups\n";
    if (changedOffsets.empty()) {
        oss << "  No wide diff detected.\n";
        return oss.str();
    }

    size_t idx = 0;
    while (idx < changedOffsets.size()) {
        size_t start = changedOffsets[idx];
        size_t end = start;
        while (idx + 1 < changedOffsets.size() && changedOffsets[idx + 1] == end + 1) {
            ++idx;
            end = changedOffsets[idx];
        }

        oss << "  [" << std::hex << start << ".." << end << std::dec << "] len=" << (end - start + 1);
        if (start + 4 <= sizeof(before.snap.rawCharSkillbarWide)) {
            oss << " u32_before=" << ReadLe32Ui(before.snap.rawCharSkillbarWide, start)
                << " u32_after=" << ReadLe32Ui(after.snap.rawCharSkillbarWide, start);
        }
        if (start + 8 <= sizeof(before.snap.rawCharSkillbarWide)) {
            oss << " u64_before=0x" << std::hex << ReadLe64Ui(before.snap.rawCharSkillbarWide, start)
                << " u64_after=0x" << ReadLe64Ui(after.snap.rawCharSkillbarWide, start)
                << std::dec;
        }
        oss << "\n";
        ++idx;
    }
    return oss.str();
}

void UpdateLiveRowsFromSnapshot(const SkillbarSnapshot& snap,
                                const std::optional<RankedOffsetCandidate>& bestWeaponCandidate,
                                const std::optional<RankedOffsetCandidate>& bestProfessionCandidate) {
    if (!snap.valid) {
        return;
    }

    std::vector<LiveSkillbarRow> rows;
    rows.reserve(10);
    const auto appendRows = [&](int slotStart, int slotEnd, const std::optional<RankedOffsetCandidate>& best, const char* sourceLabel) {
        if (!best.has_value()) {
            return;
        }
        for (int slot = slotStart; slot <= slotEnd; ++slot) {
            LiveSkillbarRow row{};
            row.hotkeyName = HotkeyForSlotIndex(slot);
            row.slotIndex = slot;
            row.apiId = static_cast<int>(ReadLe32Ui(snap.slots[slot].rawBytes, best->offset));
            row.confidence = best->confidence;
            row.skillName = row.apiId > 0 ? kx::SkillDb::ResolveSkillName(row.apiId) : "unresolved";
            std::ostringstream source;
            source << sourceLabel << " @0x" << std::hex << best->offset;
            row.source = source.str();
            rows.push_back(std::move(row));
        }
    };

    appendRows(0, 4, bestWeaponCandidate, "weapon_probe");
    appendRows(5, 9, bestProfessionCandidate, "profession_probe");

    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_liveRows = std::move(rows);
    g_statusLine = g_liveRows.empty()
        ? "Skillbar auto-identification active. waiting_for_probe"
        : ("Skillbar auto-identification active. live_slots=" + std::to_string(g_liveRows.size()) + "/10");
}

void FinalizeProbeSession(SkillbarProbeSession& session, bool timedOut) {
    const std::string rootDir = GetDllDirectory() + "\\" + kProbeRootDirName;
    CreateDirectoryA(rootDir.c_str(), nullptr);
    const std::string scenarioDir = rootDir + "\\" + session.scenarioName;
    CreateDirectoryA(scenarioDir.c_str(), nullptr);

    SYSTEMTIME st = {};
    GetLocalTime(&st);
    char dirName[64];
    snprintf(dirName,
             sizeof(dirName),
             "%04u%02u%02u_%02u%02u%02u_%03u",
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    session.outputDir = scenarioDir + "\\" + dirName;
    CreateDirectoryA(session.outputDir.c_str(), nullptr);

    const auto weaponRanked = RankOffsetCandidates(session, 0, 4);
    const auto professionRanked = RankOffsetCandidates(session, 5, 9);
    std::optional<RankedOffsetCandidate> bestWeaponCandidate;
    std::optional<RankedOffsetCandidate> bestProfessionCandidate;
    if (!weaponRanked.empty()) {
        bestWeaponCandidate = weaponRanked.front();
    }
    if (!professionRanked.empty()) {
        bestProfessionCandidate = professionRanked.front();
    }
    g_bestWeaponCandidate = bestWeaponCandidate;
    g_bestProfessionCandidate = bestProfessionCandidate;
    if (session.changedStable.snap.valid) {
        UpdateLiveRowsFromSnapshot(session.changedStable.snap, bestWeaponCandidate, bestProfessionCandidate);
    } else if (session.baselineStable.snap.valid) {
        UpdateLiveRowsFromSnapshot(session.baselineStable.snap, bestWeaponCandidate, bestProfessionCandidate);
    }

    std::ostringstream summary;
    summary << "scenario=" << session.scenarioName
            << " session_id=" << session.sessionId
            << " result=" << (timedOut ? "timeout" : "success") << "\n";
    summary << "context resolved: ctx=0x" << std::hex << session.baselineStable.snap.ctxAddr
            << " chcli=0x" << session.baselineStable.snap.chCliContextAddr
            << " local=0x" << session.baselineStable.snap.localCharAddr
            << " pChCliSkillbar=0x" << session.baselineStable.snap.chCliSkillbarAddr
            << " pCharSkillbar=0x" << session.baselineStable.snap.charSkillbarAddr
            << std::dec << "\n";
    summary << "snapshot captured: baseline=" << session.baselineSamples.size()
            << " changed=" << session.changedSamples.size() << "\n";
    summary << BuildCandidateSummaryText("[candidate slot offsets] weapon 1-5", weaponRanked, 0, 4);
    summary << BuildCandidateSummaryText("[candidate slot offsets] profession F1-F5", professionRanked, 5, 9);
    if (session.baselineStable.snap.valid && session.changedStable.snap.valid) {
        summary << "diff generated\n";
        summary << BuildWideDiffText(session.baselineStable, session.changedStable);
    }
    session.summary = summary.str();

    WriteTextFile(session.outputDir + "\\summary.txt", session.summary);
    if (session.baselineStable.snap.valid) {
        WriteTextFile(session.outputDir + "\\baseline_snapshot.txt", BuildProbeSnapshotText(session.baselineStable));
    }
    if (session.changedStable.snap.valid) {
        WriteTextFile(session.outputDir + "\\changed_snapshot.txt", BuildProbeSnapshotText(session.changedStable));
        char diffBuf[8192] = {};
        SkillMemoryReader::diffDump(session.baselineStable.snap, session.changedStable.snap, diffBuf, sizeof(diffBuf));
        WriteTextFile(session.outputDir + "\\primary_diff.txt", diffBuf);
        WriteTextFile(session.outputDir + "\\wide_diff.txt", BuildWideDiffText(session.baselineStable, session.changedStable));
    }

    kx::Log::Info("[SKILLBAR_PROBE] diff generated scenario=%s dir=%s", session.scenarioName.c_str(), session.outputDir.c_str());
    kx::Log::Info("[SKILLBAR_PROBE] candidate slot offsets found scenario=%s weapon=%s profession=%s",
                  session.scenarioName.c_str(),
                  weaponRanked.empty() ? "none" : "yes",
                  professionRanked.empty() ? "none" : "yes");

    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_probeStatusLine = timedOut
        ? ("Skillbar probe timed out: " + session.scenarioName)
        : ("Skillbar probe complete: " + session.scenarioName);
    g_lastProbeSummary = session.summary;
}

void ProcessProbe(unsigned long long nowTick) {
    if (kx::g_globalPauseMode.load(std::memory_order_acquire)) {
        bool canceled = false;
        {
            std::lock_guard<std::mutex> probeLock(g_probeMutex);
            canceled = g_activeProbe.has_value();
            g_activeProbe.reset();
        }
        if (canceled) {
            const char* reason = "global_pause";
            kx::Log::Info("[SKILLBAR_PROBE] canceled reason=%s", reason);
            std::lock_guard<std::mutex> lock(g_stateMutex);
            g_probeStatusLine = std::string("Skillbar probe canceled: ") + reason + ".";
        }
        return;
    }

    std::lock_guard<std::mutex> probeLock(g_probeMutex);
    if (!g_activeProbe.has_value()) {
        return;
    }

    auto& session = *g_activeProbe;
    if (session.phase == ProbePhase::CapturingBaseline) {
        if (nowTick < session.lastPollTickMs + kProbePollMs) {
            return;
        }
        session.lastPollTickMs = nowTick;
        SkillbarSnapshot snap = g_skillReader.readNoWait();
        if (!snap.valid) {
            return;
        }
        session.baselineSamples.push_back({nowTick, snap});
        if (session.baselineSamples.size() > kStableSampleCount) {
            session.baselineSamples.erase(session.baselineSamples.begin());
        }

        kx::Log::Info("[SKILLBAR_PROBE] snapshot captured scenario=%s phase=baseline charskillbar=0x%p",
                      session.scenarioName.c_str(),
                      reinterpret_cast<void*>(snap.charSkillbarAddr));
        if (session.baselineSamples.size() == kStableSampleCount && SnapshotSeriesStable(session.baselineSamples)) {
            session.baselineStable = session.baselineSamples.back();
            session.phaseStartedTickMs = nowTick;
            session.phase = (session.scenarioName == "idle_baseline") ? ProbePhase::Completed : ProbePhase::WaitingForChange;
            kx::Log::Info("[SKILLBAR_PROBE] context resolved scenario=%s ctx=0x%p local=0x%p pChCliSkillbar=0x%p pCharSkillbar=0x%p",
                          session.scenarioName.c_str(),
                          reinterpret_cast<void*>(snap.ctxAddr),
                          reinterpret_cast<void*>(snap.localCharAddr),
                          reinterpret_cast<void*>(snap.chCliSkillbarAddr),
                          reinterpret_cast<void*>(snap.charSkillbarAddr));
            if (session.phase == ProbePhase::Completed) {
                FinalizeProbeSession(session, false);
                g_activeProbe.reset();
            }
        }
        return;
    }

    if (nowTick > session.phaseStartedTickMs + kProbeTimeoutMs) {
        session.phase = ProbePhase::TimedOut;
        FinalizeProbeSession(session, true);
        g_activeProbe.reset();
        return;
    }

    if (nowTick < session.lastPollTickMs + kProbePollMs) {
        return;
    }
    session.lastPollTickMs = nowTick;
    SkillbarSnapshot snap = g_skillReader.readNoWait();
    if (!snap.valid) {
        return;
    }

    if (session.phase == ProbePhase::WaitingForChange) {
        if (HasAnyStructuralChange(session.baselineStable.snap, snap)) {
            session.changedSamples.clear();
            session.changedSamples.push_back({nowTick, snap});
            session.phase = ProbePhase::CapturingChanged;
            session.phaseStartedTickMs = nowTick;
            kx::Log::Info("[SKILLBAR_PROBE] snapshot captured scenario=%s phase=change_detected charskillbar=0x%p",
                          session.scenarioName.c_str(),
                          reinterpret_cast<void*>(snap.charSkillbarAddr));
        }
        return;
    }

    if (!HasAnyStructuralChange(session.baselineStable.snap, snap)) {
        session.changedSamples.clear();
        session.phase = ProbePhase::WaitingForChange;
        return;
    }

    session.changedSamples.push_back({nowTick, snap});
    if (session.changedSamples.size() > kStableSampleCount) {
        session.changedSamples.erase(session.changedSamples.begin());
    }
    if (session.changedSamples.size() == kStableSampleCount && SnapshotSeriesStable(session.changedSamples)) {
        session.changedStable = session.changedSamples.back();
        session.phase = ProbePhase::Completed;
        FinalizeProbeSession(session, false);
        g_activeProbe.reset();
    }
}

const char* GetAutoScenarioNameForHotkey(int hotkeyId) {
    switch (static_cast<kx::SkillHotkeyId>(hotkeyId)) {
    case kx::SkillHotkeyId::Profession1: return "auto_f1";
    case kx::SkillHotkeyId::Profession2: return "auto_f2";
    case kx::SkillHotkeyId::Profession3: return "auto_f3";
    default: return nullptr;
    }
}

void StartAutoProbeForHotkey(int hotkeyId, unsigned long long hotkeyTickMs, unsigned long long nowTick) {
    const char* scenarioName = GetAutoScenarioNameForHotkey(hotkeyId);
    if (!scenarioName || hotkeyTickMs == 0) {
        return;
    }

    SkillbarSnapshot baseline = g_skillReader.readNoWait();
    SkillbarProbeSession session{};
    session.scenarioName = scenarioName;
    session.startedTickMs = nowTick;
    session.phaseStartedTickMs = nowTick;
    session.lastPollTickMs = 0;

    if (baseline.valid) {
        session.baselineStable = {nowTick, baseline};
        session.baselineSamples.push_back(session.baselineStable);
        session.phase = ProbePhase::WaitingForChange;
        kx::Log::Info("[SKILLBAR_PROBE] auto begin scenario=%s hotkey=%s tick=%llu baseline=0x%p",
                      scenarioName,
                      kx::GetSkillHotkeyName(hotkeyId),
                      hotkeyTickMs,
                      reinterpret_cast<void*>(baseline.charSkillbarAddr));
    } else {
        session.phase = ProbePhase::CapturingBaseline;
        kx::Log::Info("[SKILLBAR_PROBE] auto begin scenario=%s hotkey=%s tick=%llu baseline=pending",
                      scenarioName,
                      kx::GetSkillHotkeyName(hotkeyId),
                      hotkeyTickMs);
    }

    {
        std::lock_guard<std::mutex> probeLock(g_probeMutex);
        if (g_activeProbe.has_value()) {
            return;
        }
        if (hotkeyTickMs == g_lastAutoProbeHotkeyTickMs) {
            return;
        }
        g_lastAutoProbeHotkeyTickMs = hotkeyTickMs;
        session.sessionId = g_nextProbeSessionId++;
        g_activeProbe = session;
    }
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_probeStatusLine = std::string("Auto probe armed from ") + kx::GetSkillHotkeyName(hotkeyId);
}

void WorkerLoop() {
    while (g_running.load(std::memory_order_acquire)) {
        const unsigned long long nowTick = GetTickCount64();
        ProcessProbe(nowTick);
        Sleep(50);
    }
}

} // namespace

bool Initialize() {
    if (g_running.exchange(true, std::memory_order_acq_rel)) {
        return true;
    }

    {
        std::lock_guard<std::mutex> lock(g_stateMutex);
        g_statusLine = "Skillbar auto-identification idle. auto probe paused.";
        g_lastTraceSummary = "Auto hotkey probe paused; manual skillbar probe only.";
        g_probeStatusLine = "Skillbar probe idle.";
        g_lastProbeSummary = "No skillbar probe completed yet.";
        g_liveRows.clear();
    }
    {
        std::lock_guard<std::mutex> probeLock(g_probeMutex);
        g_bestWeaponCandidate.reset();
        g_bestProfessionCandidate.reset();
        g_activeProbe.reset();
        g_nextProbeSessionId = 1;
        g_lastAutoProbeHotkeyTickMs = 0;
    }
    g_worker = std::thread(WorkerLoop);
    return true;
}

void Shutdown() {
    if (!g_running.exchange(false, std::memory_order_acq_rel)) {
        return;
    }
    if (g_worker.joinable()) {
        g_worker.join();
    }
    std::lock_guard<std::mutex> probeLock(g_probeMutex);
    g_activeProbe.reset();
}

std::string GetStatusLine() {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return g_statusLine;
}

std::string GetLastTraceSummary() {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return g_lastTraceSummary;
}

std::string GetProbeStatusLine() {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return g_probeStatusLine;
}

std::string GetLastProbeSummary() {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return g_lastProbeSummary;
}

bool StartSkillbarProbe(const char* scenarioName) {
    const std::string name = scenarioName ? scenarioName : "";
    if (name.empty() ||
        !g_running.load(std::memory_order_acquire) ||
        kx::g_globalPauseMode.load(std::memory_order_acquire)) {
        return false;
    }

    SkillbarProbeSession session{};
    session.scenarioName = name;
    session.phase = ProbePhase::CapturingBaseline;
    session.startedTickMs = GetTickCount64();
    session.phaseStartedTickMs = session.startedTickMs;
    {
        std::lock_guard<std::mutex> probeLock(g_probeMutex);
        session.sessionId = g_nextProbeSessionId++;
        g_activeProbe = session;
    }

    kx::Log::Info("[SKILLBAR_PROBE] begin scenario=%s session_id=%llu",
                  name.c_str(),
                  static_cast<unsigned long long>(session.sessionId));
    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_probeStatusLine = "Skillbar probe arming baseline: " + name;
    return true;
}

void CancelSkillbarProbe(const char* reason) {
    const std::string reasonText = reason && reason[0] ? reason : "canceled";
    bool canceled = false;
    {
        std::lock_guard<std::mutex> probeLock(g_probeMutex);
        canceled = g_activeProbe.has_value();
        g_activeProbe.reset();
    }

    std::lock_guard<std::mutex> lock(g_stateMutex);
    g_probeStatusLine = canceled
        ? ("Skillbar probe canceled: " + reasonText)
        : "Skillbar probe idle.";
    if (canceled) {
        kx::Log::Info("[SKILLBAR_PROBE] canceled reason=%s", reasonText.c_str());
    }
}

std::vector<LiveSkillbarRow> GetLiveSkillbarRows() {
    std::lock_guard<std::mutex> lock(g_stateMutex);
    return g_liveRows;
}

} // namespace kx::SkillTrace
