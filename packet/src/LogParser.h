#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace packet {

struct MemorySnapshot {
    std::string phase;
    uint32_t phaseCode = 0;
    uint64_t ctx = 0;
    uint64_t chcliCtx = 0;
    uint64_t local = 0;
    uint64_t chcliSkillbar = 0;
    uint64_t charSkillbar = 0;
    uint64_t ptr40Value = 0;
    uint64_t ptr48Value = 0;
    uint64_t ptr60Value = 0;
    uint64_t ptr40Slot = 0;
    uint64_t ptr48Slot = 0;
    uint64_t ptr60Slot = 0;
    uint64_t focusSlotAddr = 0;
    uint64_t n48Addr = 0;
    uint64_t n48FromCur40Addr = 0;
    uint64_t n48FromCur40Value = 0;
    uint64_t n48FromCur60Addr = 0;
    uint64_t n48FromCur60Value = 0;
    uint64_t objAddr = 0;
    uint64_t obj8Addr = 0;
    uint64_t ptr48Raw = 0;
    uint64_t slotPreAddr = 0;
    uint64_t upstreamEntryAddr = 0;
    uint64_t ptr40Target = 0;
    int focusSlotIndex = -1;
};

struct SlotSnapshot {
    std::string phase;
    int slotIndex = -1;
    std::string hotkey;
    uint64_t addr = 0;
    uint64_t runId = 0;
};

struct RunRecord {
    uint64_t runId = 0;
    std::string hotkey;
    std::optional<uint64_t> skillPressTick;
    std::optional<double> skillPressOffsetSec;
    std::optional<double> nodeCdOffsetSec;
    std::optional<double> nodeReadyOffsetSec;
    std::vector<MemorySnapshot> skmem;
    std::vector<SlotSnapshot> slots;
};

struct ParsedLog {
    std::vector<RunRecord> runs;
    std::string sourcePath;
};

ParsedLog ParseLogFile(const std::string& path);
std::optional<std::string> FindLatestLog(const std::string& rootDir);

} // namespace packet
