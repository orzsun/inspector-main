#include "LogParser.h"

#include <iomanip>
#include <iostream>
#include <set>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

void PrintUsage() {
    std::cout
        << "packet - external GW2 packet/memory analysis helper\n\n"
        << "Commands:\n"
        << "  packet summary --log <path>\n"
        << "  packet latest --log-root <dir>\n"
        << "  packet x64 --log <path> --run <id>\n";
}

std::optional<std::string> GetArgValue(int argc, char** argv, const std::string& name) {
    for (int i = 0; i + 1 < argc; ++i) {
        if (argv[i] == name) {
            return std::string(argv[i + 1]);
        }
    }
    return std::nullopt;
}

void PrintSummary(const packet::ParsedLog& parsed) {
    std::cout << "source: " << parsed.sourcePath << "\n";
    std::cout << "runs: " << parsed.runs.size() << "\n\n";
    for (const auto& run : parsed.runs) {
        std::cout << "run_" << std::setw(2) << std::setfill('0') << run.runId << std::setfill(' ')
                  << " hotkey=" << run.hotkey
                  << " skill_press_tick=" << (run.skillPressTick.has_value() ? std::to_string(*run.skillPressTick) : "na")
                  << " node_cd=" << (run.nodeCdOffsetSec.has_value() ? std::to_string(*run.nodeCdOffsetSec) + "s" : "na")
                  << " node_ready=" << (run.nodeReadyOffsetSec.has_value() ? std::to_string(*run.nodeReadyOffsetSec) + "s" : "na")
                  << " skmem=" << run.skmem.size()
                  << " slots=" << run.slots.size()
                  << "\n";
    }
}

const packet::MemorySnapshot* FindBestSnapshot(const packet::RunRecord& run) {
    for (const auto& snap : run.skmem) {
        if (snap.phase == "t0") {
            return &snap;
        }
    }
    if (!run.skmem.empty()) {
        return &run.skmem.front();
    }
    return nullptr;
}

std::optional<uint64_t> FindFocusSlotAddress(const packet::RunRecord& run, int slotIndex) {
    if (slotIndex < 0) {
        return std::nullopt;
    }
    for (const auto& slot : run.slots) {
        if (slot.slotIndex == slotIndex && slot.phase == "t0") {
            return slot.addr;
        }
    }
    for (const auto& slot : run.slots) {
        if (slot.slotIndex == slotIndex) {
            return slot.addr;
        }
    }
    return std::nullopt;
}

void PrintX64Vars(const packet::ParsedLog& parsed, uint64_t targetRunId) {
    const packet::RunRecord* run = nullptr;
    for (const auto& candidate : parsed.runs) {
        if (candidate.runId == targetRunId) {
            run = &candidate;
            break;
        }
    }
    if (!run) {
        std::cerr << "run not found: " << targetRunId << "\n";
        std::exit(2);
    }

    const auto* snap = FindBestSnapshot(*run);
    if (!snap) {
        std::cerr << "run has no SKMEM snapshot: " << targetRunId << "\n";
        std::exit(3);
    }

    uint64_t ptr40Slot = snap->ptr40Slot;
    uint64_t ptr60Slot = snap->ptr60Slot;
    if (!ptr40Slot && snap->charSkillbar) {
        ptr40Slot = snap->charSkillbar + 0x40;
    }
    if (!ptr60Slot && snap->charSkillbar) {
        ptr60Slot = snap->charSkillbar + 0x60;
    }

    uint64_t focusSlotAddr = snap->focusSlotAddr;
    if (!focusSlotAddr) {
        focusSlotAddr = FindFocusSlotAddress(*run, snap->focusSlotIndex).value_or(0);
    }

    std::cout << std::hex << std::uppercase;
    std::cout << "run_id          = " << std::dec << run->runId << "\n" << std::hex;
    std::cout << "hotkey          = " << run->hotkey << "\n";
    std::cout << "phase           = " << snap->phase << "\n";
    std::cout << "phase_code      = " << std::dec << snap->phaseCode << "\n" << std::hex;
    std::cout << "ctx             = 0x" << snap->ctx << "\n";
    std::cout << "chcli_ctx       = 0x" << snap->chcliCtx << "\n";
    std::cout << "local           = 0x" << snap->local << "\n";
    std::cout << "chcli_skillbar  = 0x" << snap->chcliSkillbar << "\n";
    std::cout << "char_skillbar   = 0x" << snap->charSkillbar << "\n";
    std::cout << "ptr40_value     = 0x" << snap->ptr40Value << "\n";
    std::cout << "ptr48_value     = 0x" << snap->ptr48Value << "\n";
    std::cout << "ptr60_value     = 0x" << snap->ptr60Value << "\n";
    std::cout << "ptr40_slot      = 0x" << ptr40Slot << "\n";
    std::cout << "ptr60_slot      = 0x" << ptr60Slot << "\n";
    std::cout << "ptr48_raw       = 0x" << snap->ptr48Raw << "\n";
    std::cout << "n48_addr        = 0x" << snap->n48Addr << "\n";
    std::cout << "n48_from_cur40_addr = 0x" << snap->n48FromCur40Addr << "\n";
    std::cout << "n48_from_cur40_value= 0x" << snap->n48FromCur40Value << "\n";
    std::cout << "n48_from_cur60_addr = 0x" << snap->n48FromCur60Addr << "\n";
    std::cout << "n48_from_cur60_value= 0x" << snap->n48FromCur60Value << "\n";
    std::cout << "focus_slot      = " << std::dec << snap->focusSlotIndex << "\n" << std::hex;
    if (focusSlotAddr != 0) {
        std::cout << "focus_slot_addr = 0x" << focusSlotAddr << "\n";
    } else {
        std::cout << "focus_slot_addr = <unknown>\n";
    }
    std::cout << "obj_addr        = 0x" << snap->objAddr << "\n";
    std::cout << "obj8_addr       = 0x" << snap->obj8Addr << "\n";
    std::cout << "slot_pre_addr   = 0x" << snap->slotPreAddr << "\n";
    std::cout << "upstream_entry_addr=0x" << snap->upstreamEntryAddr << "\n";
    std::cout << "ptr40_target    = 0x" << snap->ptr40Target << "\n";

    struct WatchCandidate {
        const char* name;
        uint32_t kind;
        uint64_t addr;
    };
    const WatchCandidate priority[] = {
        {"PTR40",     2, ptr40Slot},
        {"PTR60",     4, ptr60Slot},
        {"N48_CUR40", 5, snap->n48FromCur40Addr ? snap->n48FromCur40Addr : snap->n48Addr},
        {"N48_CUR60", 5, snap->n48FromCur60Addr},
        {"SLOT",      1, focusSlotAddr},
    };

    std::set<uint64_t> seen;
    std::cout << "watch_targets   =\n";
    for (const auto& candidate : priority) {
        if (candidate.addr == 0 || seen.count(candidate.addr) != 0) {
            continue;
        }
        seen.insert(candidate.addr);
        std::cout << "  kind=" << candidate.name
                  << " kind_code=" << std::dec << candidate.kind << std::hex
                  << " addr=0x" << candidate.addr
                  << "\n";
    }
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        PrintUsage();
        return 1;
    }

    const std::string command = argv[1];
    if (command == "summary") {
        const auto logPath = GetArgValue(argc, argv, "--log");
        if (!logPath.has_value()) {
            PrintUsage();
            return 1;
        }
        const auto parsed = packet::ParseLogFile(*logPath);
        PrintSummary(parsed);
        return 0;
    }

    if (command == "latest") {
        const auto root = GetArgValue(argc, argv, "--log-root");
        if (!root.has_value()) {
            PrintUsage();
            return 1;
        }
        const auto latest = packet::FindLatestLog(*root);
        if (!latest.has_value()) {
            std::cerr << "no .log files found in " << *root << "\n";
            return 2;
        }
        const auto parsed = packet::ParseLogFile(*latest);
        PrintSummary(parsed);
        return 0;
    }

    if (command == "x64") {
        const auto logPath = GetArgValue(argc, argv, "--log");
        const auto runValue = GetArgValue(argc, argv, "--run");
        if (!logPath.has_value() || !runValue.has_value()) {
            PrintUsage();
            return 1;
        }
        const auto parsed = packet::ParseLogFile(*logPath);
        uint64_t runId = 0;
        try {
            runId = std::stoull(*runValue);
        } catch (const std::exception&) {
            std::cerr << "invalid --run value: " << *runValue << "\n";
            return 1;
        }
        PrintX64Vars(parsed, runId);
        return 0;
    }

    PrintUsage();
    return 1;
}
