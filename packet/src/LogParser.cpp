#include "LogParser.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <unordered_map>

namespace packet {
namespace {

using namespace std::string_literals;

std::string ReadWholeFile(const std::string& path) {
    std::ifstream in(path);
    if (!in) {
        return {};
    }
    std::ostringstream oss;
    oss << in.rdbuf();
    return oss.str();
}

uint64_t ParseHexU64(const std::string& value) {
    return value.empty() ? 0ULL : std::stoull(value, nullptr, 16);
}

double ParseOffsetSeconds(const std::string& text) {
    return text.empty() ? 0.0 : std::stod(text);
}

std::string Trim(const std::string& value) {
    size_t begin = 0;
    while (begin < value.size() &&
           std::isspace(static_cast<unsigned char>(value[begin])) != 0) {
        ++begin;
    }

    size_t end = value.size();
    while (end > begin &&
           std::isspace(static_cast<unsigned char>(value[end - 1])) != 0) {
        --end;
    }
    return value.substr(begin, end - begin);
}

std::optional<uint64_t> ParseHexFieldValue(const std::string& text) {
    if (text.empty()) {
        return std::nullopt;
    }
    std::string value = text;
    if (value.rfind("0x", 0) == 0 || value.rfind("0X", 0) == 0) {
        value = value.substr(2);
    }
    if (value.empty() || value == "<unknown>" || value == "na") {
        return std::nullopt;
    }
    try {
        return std::stoull(value, nullptr, 16);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<int> ParseIntFieldValue(const std::string& text) {
    if (text.empty() || text == "na") {
        return std::nullopt;
    }
    try {
        return std::stoi(text);
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<uint32_t> ParseUIntFieldValue(const std::string& text) {
    if (text.empty() || text == "na") {
        return std::nullopt;
    }
    try {
        return static_cast<uint32_t>(std::stoul(text));
    } catch (...) {
        return std::nullopt;
    }
}

RunRecord* FindRun(std::vector<RunRecord>& runs, uint64_t runId) {
    for (auto& run : runs) {
        if (run.runId == runId) {
            return &run;
        }
    }
    return nullptr;
}

RunRecord& GetOrCreateRun(std::vector<RunRecord>& runs, uint64_t runId, const std::string& hotkey) {
    if (auto* existing = FindRun(runs, runId)) {
        if (existing->hotkey.empty()) {
            existing->hotkey = hotkey;
        }
        return *existing;
    }

    RunRecord run;
    run.runId = runId;
    run.hotkey = hotkey;
    runs.push_back(run);
    return runs.back();
}

MemorySnapshot& GetOrCreateSnapshotForPhase(RunRecord& run, const std::string& phase) {
    if (!phase.empty()) {
        for (auto it = run.skmem.rbegin(); it != run.skmem.rend(); ++it) {
            if (it->phase == phase) {
                return *it;
            }
        }
    }

    run.skmem.push_back(MemorySnapshot{});
    run.skmem.back().phase = phase;
    return run.skmem.back();
}

void ApplyKvField(MemorySnapshot& snap, const std::string& key, const std::string& value) {
    if (key == "phase") {
        snap.phase = value;
        if (const auto phaseCode = ParseUIntFieldValue(value); phaseCode.has_value()) {
            snap.phaseCode = phaseCode.value();
        }
        return;
    }
    if (key == "focus_slot") {
        snap.focusSlotIndex = ParseIntFieldValue(value).value_or(-1);
        return;
    }
    if (key == "ctx") {
        snap.ctx = ParseHexFieldValue(value).value_or(0);
        return;
    }
    if (key == "chcli_ctx") {
        snap.chcliCtx = ParseHexFieldValue(value).value_or(0);
        return;
    }
    if (key == "local") {
        snap.local = ParseHexFieldValue(value).value_or(0);
        return;
    }
    if (key == "chcli_skillbar") {
        snap.chcliSkillbar = ParseHexFieldValue(value).value_or(0);
        return;
    }
    if (key == "char_skillbar") {
        snap.charSkillbar = ParseHexFieldValue(value).value_or(0);
        return;
    }
    if (key == "ptr40" || key == "ptr40_value") {
        snap.ptr40Value = ParseHexFieldValue(value).value_or(0);
        return;
    }
    if (key == "ptr48") {
        snap.ptr48Value = ParseHexFieldValue(value).value_or(0);
        snap.ptr48Raw = snap.ptr48Value;
        return;
    }
    if (key == "ptr60" || key == "ptr60_value") {
        snap.ptr60Value = ParseHexFieldValue(value).value_or(0);
        return;
    }
    if (key == "ptr40_slot") {
        snap.ptr40Slot = ParseHexFieldValue(value).value_or(0);
        return;
    }
    if (key == "ptr48_slot") {
        snap.ptr48Slot = ParseHexFieldValue(value).value_or(0);
        return;
    }
    if (key == "ptr60_slot") {
        snap.ptr60Slot = ParseHexFieldValue(value).value_or(0);
        return;
    }
    if (key == "focus_slot_addr") {
        snap.focusSlotAddr = ParseHexFieldValue(value).value_or(0);
        return;
    }
    if (key == "n48_addr") {
        snap.n48Addr = ParseHexFieldValue(value).value_or(0);
        return;
    }
    if (key == "n48_from_cur40_addr") {
        snap.n48FromCur40Addr = ParseHexFieldValue(value).value_or(0);
        if (!snap.n48Addr) {
            snap.n48Addr = snap.n48FromCur40Addr;
        }
        return;
    }
    if (key == "n48_from_cur40_value") {
        snap.n48FromCur40Value = ParseHexFieldValue(value).value_or(0);
        return;
    }
    if (key == "n48_from_cur60_addr") {
        snap.n48FromCur60Addr = ParseHexFieldValue(value).value_or(0);
        return;
    }
    if (key == "n48_from_cur60_value") {
        snap.n48FromCur60Value = ParseHexFieldValue(value).value_or(0);
        return;
    }
    if (key == "obj_addr") {
        snap.objAddr = ParseHexFieldValue(value).value_or(0);
        return;
    }
    if (key == "obj8_addr") {
        snap.obj8Addr = ParseHexFieldValue(value).value_or(0);
        return;
    }
    if (key == "ptr48_raw") {
        snap.ptr48Raw = ParseHexFieldValue(value).value_or(0);
        return;
    }
    if (key == "slot_pre_addr") {
        snap.slotPreAddr = ParseHexFieldValue(value).value_or(0);
        return;
    }
    if (key == "upstream_entry_addr") {
        snap.upstreamEntryAddr = ParseHexFieldValue(value).value_or(0);
        return;
    }
    if (key == "ptr40_target") {
        snap.ptr40Target = ParseHexFieldValue(value).value_or(0);
        return;
    }
}

void ParseAndApplyKeyValueFields(const std::string& line, MemorySnapshot& snap) {
    static const std::regex kAnyKvRe(R"(([A-Za-z0-9_]+)=([^\s]+))");
    auto begin = std::sregex_iterator(line.begin(), line.end(), kAnyKvRe);
    auto end = std::sregex_iterator();
    for (auto it = begin; it != end; ++it) {
        const std::string key = Trim((*it)[1].str());
        const std::string value = Trim((*it)[2].str());
        ApplyKvField(snap, key, value);
    }
}

} // namespace

ParsedLog ParseLogFile(const std::string& path) {
    ParsedLog parsed;
    parsed.sourcePath = path;

    const std::string text = ReadWholeFile(path);
    if (text.empty()) {
        return parsed;
    }

    static const std::regex kSkillPressRe(
        R"(\[CDHUNT\s+\]\s+SKILL_PRESS\s+run_id=(\d+)\s+hotkey=([^\s]+)\s+tick=(\d+)\s+offset=\+([0-9.]+)s)");
    static const std::regex kNodeAnchorRe(
        R"(\[NODE\s*\]\s+(NODE_CD_START|NODE_READY)\s+run_id=(\d+)\s+hotkey=([^\s]+)\s+phase=[^\s]+\s+offset=\+([0-9.]+)s)");
    static const std::regex kSkmemRe(
        R"(\[SKMEM\s+\]\s+phase=([^\s]+)\s+run_id=(\d+)\s+hotkey=([^\s]+)\s+focus_slot=([^\s]+)\s+ctx=0x([0-9a-fA-F]+)\s+chcli_ctx=0x([0-9a-fA-F]+)\s+local=0x([0-9a-fA-F]+)\s+chcli_skillbar=0x([0-9a-fA-F]+)\s+char_skillbar=0x([0-9a-fA-F]+)\s+ptr40=0x([0-9a-fA-F]+)\s+ptr48=0x([0-9a-fA-F]+)\s+ptr60=0x([0-9a-fA-F]+))");
    static const std::regex kSlotRe(
        R"(\[SKMEM\s+\]\s+phase=([^\s]+)\s+slot=(\d+)\s+hotkey=([^\s]+)\s+addr=0x([0-9a-fA-F]+))");
    static const std::regex kRunIdRe(R"(run_id=(\d+))");
    static const std::regex kHotkeyRe(R"(hotkey=([^\s]+))");
    static const std::regex kPhaseRe(R"(phase=([^\s]+))");

    std::istringstream input(text);
    std::string line;
    uint64_t lastSeenRunId = 0;
    std::unordered_map<std::string, uint64_t> lastSeenRunByHotkey;
    while (std::getline(input, line)) {
        std::smatch match;
        if (std::regex_search(line, match, kSkillPressRe)) {
            auto& run = GetOrCreateRun(parsed.runs, std::stoull(match[1].str()), match[2].str());
            run.skillPressTick = std::stoull(match[3].str());
            run.skillPressOffsetSec = ParseOffsetSeconds(match[4].str());
            lastSeenRunId = run.runId;
            lastSeenRunByHotkey[run.hotkey] = run.runId;
            continue;
        }

        if (std::regex_search(line, match, kNodeAnchorRe)) {
            auto& run = GetOrCreateRun(parsed.runs, std::stoull(match[2].str()), match[3].str());
            const double offset = ParseOffsetSeconds(match[4].str());
            if (match[1].str() == "NODE_CD_START") {
                run.nodeCdOffsetSec = offset;
            } else if (match[1].str() == "NODE_READY") {
                run.nodeReadyOffsetSec = offset;
            }
            lastSeenRunId = run.runId;
            lastSeenRunByHotkey[run.hotkey] = run.runId;
            continue;
        }

        if (std::regex_search(line, match, kSkmemRe)) {
            auto& run = GetOrCreateRun(parsed.runs, std::stoull(match[2].str()), match[3].str());
            MemorySnapshot snap;
            snap.phase = match[1].str();
            snap.focusSlotIndex = (match[4].str() == "na") ? -1 : std::stoi(match[4].str());
            snap.ctx = ParseHexU64(match[5].str());
            snap.chcliCtx = ParseHexU64(match[6].str());
            snap.local = ParseHexU64(match[7].str());
            snap.chcliSkillbar = ParseHexU64(match[8].str());
            snap.charSkillbar = ParseHexU64(match[9].str());
            snap.ptr40Value = ParseHexU64(match[10].str());
            snap.ptr48Value = ParseHexU64(match[11].str());
            snap.ptr48Raw = snap.ptr48Value;
            snap.ptr60Value = ParseHexU64(match[12].str());
            run.skmem.push_back(snap);
            lastSeenRunId = run.runId;
            lastSeenRunByHotkey[run.hotkey] = run.runId;
            continue;
        }

        if (line.find("[SKMEM") != std::string::npos ||
            line.find("[SKBP") != std::string::npos) {
            std::smatch runMatch;
            uint64_t resolvedRunId = 0;
            std::string hotkey;
            std::string phase;

            if (std::regex_search(line, runMatch, kRunIdRe)) {
                resolvedRunId = std::stoull(runMatch[1].str());
            }
            std::smatch hotkeyMatch;
            if (std::regex_search(line, hotkeyMatch, kHotkeyRe)) {
                hotkey = hotkeyMatch[1].str();
            }
            std::smatch phaseMatch;
            if (std::regex_search(line, phaseMatch, kPhaseRe)) {
                phase = phaseMatch[1].str();
            }

            if (resolvedRunId == 0 && !hotkey.empty()) {
                auto byHotkey = lastSeenRunByHotkey.find(hotkey);
                if (byHotkey != lastSeenRunByHotkey.end()) {
                    resolvedRunId = byHotkey->second;
                }
            }
            if (resolvedRunId == 0) {
                resolvedRunId = lastSeenRunId;
            }
            if (resolvedRunId != 0) {
                auto& run = GetOrCreateRun(parsed.runs, resolvedRunId, hotkey);
                auto& snap = GetOrCreateSnapshotForPhase(run, phase);
                ParseAndApplyKeyValueFields(line, snap);
                if (run.hotkey.empty() && !hotkey.empty()) {
                    run.hotkey = hotkey;
                }
                lastSeenRunId = resolvedRunId;
                if (!run.hotkey.empty()) {
                    lastSeenRunByHotkey[run.hotkey] = run.runId;
                }
            }
        }

        if (std::regex_search(line, match, kSlotRe)) {
            const std::string slotHotkey = match[3].str();
            uint64_t resolvedRunId = 0;
            auto byHotkey = lastSeenRunByHotkey.find(slotHotkey);
            if (byHotkey != lastSeenRunByHotkey.end()) {
                resolvedRunId = byHotkey->second;
            } else if (lastSeenRunId != 0) {
                resolvedRunId = lastSeenRunId;
            } else {
                continue;
            }
            auto* run = FindRun(parsed.runs, resolvedRunId);
            if (!run) {
                continue;
            }
            SlotSnapshot slot;
            slot.phase = match[1].str();
            slot.slotIndex = std::stoi(match[2].str());
            slot.hotkey = slotHotkey;
            slot.addr = ParseHexU64(match[4].str());
            slot.runId = resolvedRunId;
            run->slots.push_back(slot);
            continue;
        }
    }

    std::sort(parsed.runs.begin(), parsed.runs.end(), [](const RunRecord& a, const RunRecord& b) {
        return a.runId < b.runId;
    });
    return parsed;
}

std::optional<std::string> FindLatestLog(const std::string& rootDir) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::exists(rootDir, ec) || ec) {
        return std::nullopt;
    }
    if (!fs::is_directory(rootDir, ec) || ec) {
        return std::nullopt;
    }

    std::optional<fs::directory_entry> latest;
    for (const auto& entry : fs::directory_iterator(rootDir, ec)) {
        if (ec) {
            return std::nullopt;
        }
        if (!entry.is_regular_file() || entry.path().extension() != ".log") {
            continue;
        }
        if (!latest.has_value() || entry.last_write_time() > latest->last_write_time()) {
            latest = entry;
        }
    }
    if (!latest.has_value()) {
        return std::nullopt;
    }
    return latest->path().string();
}

} // namespace packet
