#define NOMINMAX

#include "ImGuiManager.h"
#include "../libs/ImGui/imgui.h"
#include "../libs/ImGui/imgui_impl_win32.h"
#include "../libs/ImGui/imgui_impl_dx11.h"
#include "PacketData.h" // Include for PacketInfo, g_packetLog, g_packetLogMutex
#include "AppState.h"   // Include for UI state, filter state, hook status
#include "GuiStyle.h"  // Include for custom styling functions
#include "FormattingUtils.h"
#include "FilterUtils.h"
#include "PacketHeaders.h" // Need this for iterating known headers
#include "Config.h"
#include "PacketParser.h"
#include "SkillMemoryReader.h"
#include "SkillTraceCoordinator.h"
#include "SkillCooldownTracker.h"

#include <vector>
#include <mutex>
#include <deque>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <random>
#include <ctime>   // For formatting time
#include <string>
#include <map>     // For std::map used in filtering
#include <array>
#include <windows.h> // Required for ShellExecuteA
#include <algorithm>

extern HINSTANCE dll_handle;

namespace {
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

kx::SkillUsePacketCapture SnapshotSkillUsePacket() {
    std::lock_guard<std::mutex> lock(kx::g_lastSkillUsePacketMutex);
    return kx::g_lastSkillUsePacket;
}

struct TimedSkillSnapshot;

std::string BuildSkillUsePacketHex(const kx::SkillUsePacketCapture& capture) {
    if (!capture.valid || capture.packetSize <= 0) {
        return "No CMSG_USE_SKILL packet captured yet.";
    }

    std::ostringstream oss;
    for (int row = 0; row < capture.packetSize; row += 16) {
        char line[128];
        char bytesText[64] = {};
        int cursor = 0;
        const int rowSize = std::min(16, capture.packetSize - row);
        for (int i = 0; i < rowSize; ++i) {
            cursor += snprintf(bytesText + cursor,
                               sizeof(bytesText) - static_cast<size_t>(cursor),
                               (i == 7) ? "%02X  " : "%02X ",
                               capture.bytes[static_cast<size_t>(row + i)]);
        }
        snprintf(line, sizeof(line), "%03X: %s", row, bytesText);
        oss << line << '\n';
    }
    return oss.str();
}

std::string BuildSkillUsePacketFieldTable(const kx::SkillUsePacketCapture& capture) {
    if (!capture.valid || capture.packetSize <= 0) {
        return "No CMSG_USE_SKILL packet captured yet.";
    }

    const kx::SkillUseTailFields tailFields = kx::ParseSkillUseTailFields(capture);

    std::ostringstream oss;
    oss << "[0x0017] UseSkill Packet Breakdown\n";
    if (capture.packetSize >= 2) {
        oss << "  opcode      u16le @ 0 : 0x"
            << std::hex << std::setw(4) << std::setfill('0')
            << static_cast<unsigned>(static_cast<uint16_t>(capture.bytes[0] | (capture.bytes[1] << 8)))
            << std::dec << "\n";
    }
    if (capture.packetSize >= 4) {
        oss << "  field_02    u16le @ 2 : 0x"
            << std::hex << std::setw(4) << std::setfill('0')
            << static_cast<unsigned>(static_cast<uint16_t>(capture.bytes[2] | (capture.bytes[3] << 8)))
            << std::dec << "\n";
    }
    if (capture.packetSize >= 8) {
        const uint32_t tickSeq = static_cast<uint32_t>(capture.bytes[4]) |
                                 (static_cast<uint32_t>(capture.bytes[5]) << 8) |
                                 (static_cast<uint32_t>(capture.bytes[6]) << 16) |
                                 (static_cast<uint32_t>(capture.bytes[7]) << 24);
        oss << "  tick_seq    u32le @ 4 : " << tickSeq
            << " (0x" << std::hex << std::setw(8) << std::setfill('0') << tickSeq << std::dec << ")\n";
    }
    if (capture.packetSize >= 13) {
        oss << "  entity_id   bytes @ 8 : ";
        for (int i = 8; i <= 12; ++i) {
            oss << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<unsigned>(capture.bytes[static_cast<size_t>(i)])
                << (i == 12 ? "" : " ");
        }
        oss << std::dec << "  (varint candidate)\n";
    }
    if (capture.packetSize >= 14) {
        oss << "  tail_prefix u8   @ 13: 0x"
            << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(tailFields.prefix)
            << std::dec << "\n";
    }
    if (capture.packetSize >= 15) {
        oss << "  tail_tag    u8   @ 14: 0x"
            << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(tailFields.tag)
            << std::dec;
        if (tailFields.tag == 0x29) {
            oss << "  (primary mapping)";
        } else if (tailFields.tag == 0x25) {
            oss << "  (secondary mapping)";
        }
        oss << "\n";
    }
    if (capture.packetSize >= 16) {
        oss << "  tail_val    u8   @ 15: 0x"
            << std::hex << std::setw(2) << std::setfill('0') << static_cast<unsigned>(tailFields.value)
            << std::dec << " (" << static_cast<unsigned>(tailFields.value) << ")\n";
    }

    oss << "\nByte groups\n";
    for (int offset = 0; offset < capture.packetSize; offset += 2) {
        const int remaining = capture.packetSize - offset;
        oss << "  [" << std::setw(2) << std::setfill('0') << offset << "] ";
        if (remaining >= 2) {
            const uint16_t v16 = static_cast<uint16_t>(capture.bytes[static_cast<size_t>(offset)] |
                                                       (capture.bytes[static_cast<size_t>(offset + 1)] << 8));
            oss << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<unsigned>(capture.bytes[static_cast<size_t>(offset)]) << " "
                << std::setw(2) << static_cast<unsigned>(capture.bytes[static_cast<size_t>(offset + 1)])
                << std::dec << "  u16_le=" << v16;
        } else {
            oss << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<unsigned>(capture.bytes[static_cast<size_t>(offset)]) << std::dec;
        }
        oss << "\n";
    }

    oss << "\nSliding u32\n";
    for (int offset = 0; offset + 4 <= capture.packetSize; offset += 4) {
        const uint32_t v32 = static_cast<uint32_t>(capture.bytes[static_cast<size_t>(offset)]) |
                             (static_cast<uint32_t>(capture.bytes[static_cast<size_t>(offset + 1)]) << 8) |
                             (static_cast<uint32_t>(capture.bytes[static_cast<size_t>(offset + 2)]) << 16) |
                             (static_cast<uint32_t>(capture.bytes[static_cast<size_t>(offset + 3)]) << 24);
        oss << "  [" << std::setw(2) << std::setfill('0') << offset << "] u32_le=" << v32
            << " (0x" << std::hex << std::setw(8) << std::setfill('0') << v32 << std::dec << ")\n";
    }

    return oss.str();
}

uint8_t GetUseSkillTailTag(const kx::SkillUsePacketCapture& capture) {
    return kx::ParseSkillUseTailFields(capture).tag;
}

uint8_t GetUseSkillTailValue(const kx::SkillUsePacketCapture& capture) {
    return kx::ParseSkillUseTailFields(capture).value;
}

std::string GetUseSkillTailClass(const kx::SkillUsePacketCapture& capture) {
    const uint8_t tailTag = GetUseSkillTailTag(capture);
    if (tailTag == 0x29) return "Primary(0x29)";
    if (tailTag == 0x25) return "Secondary(0x25)";
    if (!capture.valid) return "None";
    return "Other";
}

std::string BuildRechargeTestText(const SkillRechargeTestResult& result) {
    std::ostringstream oss;
    oss << SkillMemoryReader::GetSkillbarObjectKindName(result.objectKind)
        << " slot=" << result.slot
        << " thread=" << (result.onGameThread ? "game" : "other")
        << " executed=" << (result.executed ? "yes" : "no")
        << " returned=" << (result.returnedValue ? "true" : "false")
        << " faulted=" << (result.callFaulted ? "yes" : "no")
        << " object=0x" << std::hex << result.objectPtr
        << " vtable=0x" << result.vtablePtr
        << " fn[7]=0x" << result.functionPtr
        << std::dec;
    return oss.str();
}

std::string BuildRawDumpText(const SkillbarSnapshot& snap) {
    std::ostringstream oss;
    for (int row = 0; row < 0x188 / 16; ++row) {
        const int off = row * 16;
        char line[128];
        snprintf(line, sizeof(line),
                 "%03X: %02X %02X %02X %02X  %02X %02X %02X %02X  "
                 "%02X %02X %02X %02X  %02X %02X %02X %02X",
                 off,
                 snap.rawCharSkillbar[off + 0], snap.rawCharSkillbar[off + 1],
                 snap.rawCharSkillbar[off + 2], snap.rawCharSkillbar[off + 3],
                 snap.rawCharSkillbar[off + 4], snap.rawCharSkillbar[off + 5],
                 snap.rawCharSkillbar[off + 6], snap.rawCharSkillbar[off + 7],
                 snap.rawCharSkillbar[off + 8], snap.rawCharSkillbar[off + 9],
                 snap.rawCharSkillbar[off + 10], snap.rawCharSkillbar[off + 11],
                 snap.rawCharSkillbar[off + 12], snap.rawCharSkillbar[off + 13],
                 snap.rawCharSkillbar[off + 14], snap.rawCharSkillbar[off + 15]);
        oss << line << '\n';
    }
    return oss.str();
}

std::string BuildByteDumpText(const uint8_t* data, size_t size) {
    std::ostringstream oss;
    for (size_t row = 0; row < size / 16; ++row) {
        const size_t off = row * 16;
        char line[128];
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

std::string BuildSlotSummaryText(const SkillbarSnapshot& snap) {
    std::ostringstream oss;
    for (size_t i = 0; i < snap.slots.size(); ++i) {
        const auto& slot = snap.slots[i];
        const uint32_t v0 = ReadLe32Ui(slot.rawBytes, 0x00);
        const uint32_t v4 = ReadLe32Ui(slot.rawBytes, 0x04);
        const uint32_t v8 = ReadLe32Ui(slot.rawBytes, 0x08);
        const uint32_t vC = ReadLe32Ui(slot.rawBytes, 0x0C);
        const uint64_t p0 = ReadLe64Ui(slot.rawBytes, 0x00);
        const uint64_t p8 = ReadLe64Ui(slot.rawBytes, 0x08);
        oss << "Slot[" << i << "] "
            << "u32@00=" << v0
            << " u32@04=" << v4
            << " u32@08=" << v8
            << " u32@0C=" << vC
            << " u64@00=0x" << std::hex << p0
            << " u64@08=0x" << p8
            << std::dec << '\n';
    }
    return oss.str();
}

void RenderSlotCandidateTable(const SkillbarSnapshot& snap) {
    if (ImGui::BeginTable("SkillSlotCandidates", 7, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Slot");
        ImGui::TableSetupColumn("u32@00");
        ImGui::TableSetupColumn("u32@04");
        ImGui::TableSetupColumn("u32@08");
        ImGui::TableSetupColumn("u32@0C");
        ImGui::TableSetupColumn("u64@00");
        ImGui::TableSetupColumn("u64@08");
        ImGui::TableHeadersRow();

        for (size_t i = 0; i < snap.slots.size(); ++i) {
            const auto& slot = snap.slots[i];
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%zu", i);
            ImGui::TableNextColumn();
            ImGui::Text("%u", ReadLe32Ui(slot.rawBytes, 0x00));
            ImGui::TableNextColumn();
            ImGui::Text("%u", ReadLe32Ui(slot.rawBytes, 0x04));
            ImGui::TableNextColumn();
            ImGui::Text("%u", ReadLe32Ui(slot.rawBytes, 0x08));
            ImGui::TableNextColumn();
            ImGui::Text("%u", ReadLe32Ui(slot.rawBytes, 0x0C));
            ImGui::TableNextColumn();
            ImGui::Text("0x%llX", static_cast<unsigned long long>(ReadLe64Ui(slot.rawBytes, 0x00)));
            ImGui::TableNextColumn();
            ImGui::Text("0x%llX", static_cast<unsigned long long>(ReadLe64Ui(slot.rawBytes, 0x08)));
        }

        ImGui::EndTable();
    }
}

void RenderPointerObjectSummary(const char* label, uintptr_t ptrValue, const uint8_t* rawBytes, size_t rawSize) {
    ImGui::Text("%s : 0x%016llX", label, static_cast<unsigned long long>(ptrValue));
    ImGui::Text("  u32@00=%u u32@04=%u u32@08=%u u32@0C=%u",
                ReadLe32Ui(rawBytes, 0x00),
                ReadLe32Ui(rawBytes, 0x04),
                ReadLe32Ui(rawBytes, 0x08),
                ReadLe32Ui(rawBytes, 0x0C));
    ImGui::Text("  u64@00=0x%016llX u64@08=0x%016llX",
                static_cast<unsigned long long>(ReadLe64Ui(rawBytes, 0x00)),
                static_cast<unsigned long long>(ReadLe64Ui(rawBytes, 0x08)));
    ImGui::Text("  u64@10=0x%016llX u64@18=0x%016llX",
                static_cast<unsigned long long>(ReadLe64Ui(rawBytes, 0x10)),
                static_cast<unsigned long long>(ReadLe64Ui(rawBytes, 0x18)));
}

std::string BuildSmallU32CandidatesText(const uint8_t* rawBytes, size_t rawSize) {
    std::ostringstream oss;
    bool found = false;
    for (size_t offset = 0; offset + 4 <= rawSize; offset += 4) {
        const uint32_t value = ReadLe32Ui(rawBytes, offset);
        if (value < 0x10000u) {
            found = true;
            oss << "  u32 @ 0x" << std::hex << std::setw(2) << std::setfill('0') << offset
                << " = 0x" << std::setw(8) << value
                << std::dec << " (" << value << ")\n";
        }
    }
    if (!found) {
        oss << "  No u32 values under 0x10000 on 4-byte boundaries.\n";
    }
    return oss.str();
}

struct PointerObjectSampleView {
    uintptr_t ptrValue = 0;
    const uint8_t* rawBytes = nullptr;
    size_t rawSize = 0;
};

PointerObjectSampleView GetPointerObjectView(const SkillbarSnapshot& snap, int kind) {
    PointerObjectSampleView view = {};
    if (kind == 48) {
        view.ptrValue = ReadLe64Ui(snap.rawCharSkillbar, 0x48);
        view.rawBytes = snap.rawPtr48;
        view.rawSize = sizeof(snap.rawPtr48);
    } else if (kind == 60) {
        view.ptrValue = ReadLe64Ui(snap.rawCharSkillbar, 0x60);
        view.rawBytes = snap.rawPtr60;
        view.rawSize = sizeof(snap.rawPtr60);
    } else {
        view.ptrValue = ReadLe64Ui(snap.rawCharSkillbar, 0x40);
        view.rawBytes = snap.rawPtr40;
        view.rawSize = sizeof(snap.rawPtr40);
    }
    return view;
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

struct TimedSkillSnapshot {
    double offsetSeconds = 0.0;
    SkillbarSnapshot snapshot;
    int lastSkillHotkey = -1;
    std::string lastSkillHotkeyName;
    unsigned long long lastSkillHotkeyTickMs = 0;
    uint16_t lastOutgoingHeader = 0;
    int lastOutgoingSize = 0;
    int lastOutgoingBufferState = 0;
    unsigned long long lastOutgoingTickMs = 0;
    kx::SkillUsePacketCapture skillUsePacket;
    std::string diffFromFirst;
};

bool IsSkillUsePacketFresh(const TimedSkillSnapshot& sample) {
    return sample.skillUsePacket.valid &&
           sample.lastSkillHotkeyTickMs != 0 &&
           sample.skillUsePacket.tickMs >= sample.lastSkillHotkeyTickMs;
}

std::string BuildUseSkillSampleTable(const std::vector<TimedSkillSnapshot>& samples, uint8_t targetTailTag) {
    std::ostringstream oss;
    bool found = false;
    for (size_t i = 0; i < samples.size(); ++i) {
        const auto& sample = samples[i];
        if (!IsSkillUsePacketFresh(sample) || GetUseSkillTailTag(sample.skillUsePacket) != targetTailTag) {
            continue;
        }
        found = true;
        oss << "Sample[" << i << "]"
            << " hotkey=\"" << sample.lastSkillHotkeyName << "\""
            << " hotkeyTick=" << sample.lastSkillHotkeyTickMs
            << " tail=0x" << std::hex << std::setw(2) << std::setfill('0')
            << static_cast<unsigned>(GetUseSkillTailValue(sample.skillUsePacket))
            << std::dec
            << " ptr48=0x" << std::hex << ReadLe64Ui(sample.snapshot.rawCharSkillbar, 0x48)
            << " ptr60=0x" << ReadLe64Ui(sample.snapshot.rawCharSkillbar, 0x60)
            << std::dec << "\n";
    }
    if (!found) {
        oss << "No samples in this class.\n";
    }
    return oss.str();
}

std::optional<kx::PacketInfo> GetLatestReceivedPacketByHeader(uint16_t header) {
    std::lock_guard<std::mutex> lock(kx::g_packetLogMutex);
    for (auto it = kx::g_packetLog.rbegin(); it != kx::g_packetLog.rend(); ++it) {
        if (it->direction == kx::PacketDirection::Received &&
            it->specialType == kx::InternalPacketType::NORMAL &&
            it->rawHeaderId == header) {
            return *it;
        }
    }
    return std::nullopt;
}

std::string BuildRecentCooldownPacketTimeline() {
    std::lock_guard<std::mutex> lock(kx::g_packetLogMutex);

    std::vector<kx::PacketInfo> hits;
    hits.reserve(16);
    for (auto it = kx::g_packetLog.rbegin(); it != kx::g_packetLog.rend() && hits.size() < 16; ++it) {
        if (it->direction != kx::PacketDirection::Received ||
            it->specialType != kx::InternalPacketType::NORMAL) {
            continue;
        }

        if (it->rawHeaderId == static_cast<uint16_t>(kx::SMSG_HeaderId::AGENT_STATE_BULK) ||
            it->rawHeaderId == static_cast<uint16_t>(kx::SMSG_HeaderId::SKILL_UPDATE) ||
            it->rawHeaderId == static_cast<uint16_t>(kx::SMSG_HeaderId::AGENT_ATTRIBUTE_UPDATE)) {
            hits.push_back(*it);
        }
    }

    if (hits.empty()) {
        return "No recent received 0x0016 / 0x0017 / 0x0028 packets captured yet.";
    }

    std::reverse(hits.begin(), hits.end());

    std::ostringstream oss;
    for (const auto& packet : hits) {
        const auto tt = std::chrono::system_clock::to_time_t(packet.timestamp);
        tm localTm{};
        localtime_s(&localTm, &tt);
        const auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(
            packet.timestamp.time_since_epoch()).count() % 1000;

        char timeBuf[32];
        snprintf(timeBuf,
                 sizeof(timeBuf),
                 "%02d:%02d:%02d.%03lld",
                 localTm.tm_hour,
                 localTm.tm_min,
                 localTm.tm_sec,
                 static_cast<long long>(millis));

        oss << timeBuf
            << "  hdr=0x" << std::hex << std::setw(4) << std::setfill('0') << packet.rawHeaderId
            << std::dec
            << "  size=" << packet.size
            << "  name=" << packet.name
            << '\n';
    }

    return oss.str();
}

void RenderLatestSmsgPanel(const char* visibleTitle, const char* textId, uint16_t header, const char* observationHint) {
    if (!ImGui::CollapsingHeader(visibleTitle)) {
        return;
    }

    const auto latestPacket = GetLatestReceivedPacketByHeader(header);
    if (!latestPacket.has_value()) {
        ImGui::TextWrapped("No packet with header 0x%04X has been captured in the current log yet.", header);
        ImGui::TextWrapped("%s", observationHint);
        return;
    }

    ImGui::Text("Latest header : 0x%04X", header);
    ImGui::Text("Packet size   : %d", latestPacket->size);
    ImGui::TextWrapped("%s", observationHint);

    auto parsed = kx::Parsing::GetParsedDataTooltipString(*latestPacket);
    std::string text = parsed.has_value() ? parsed.value() : "No parser output available.";
    ImGui::InputTextMultiline(
        textId,
        text.data(),
        text.size() + 1,
        ImVec2(-1, ImGui::GetTextLineHeight() * 14),
        ImGuiInputTextFlags_ReadOnly);
}

std::string BuildMonotonicCandidateText(const std::vector<TimedSkillSnapshot>& samples, int kind) {
    std::ostringstream oss;
    if (samples.size() < 2) {
        oss << "Need at least 2 samples.\n";
        return oss.str();
    }

    const auto firstView = GetPointerObjectView(samples.front().snapshot, kind);
    if (!firstView.ptrValue) {
        oss << "Pointer is null in the first sample.\n";
        return oss.str();
    }

    for (size_t i = 1; i < samples.size(); ++i) {
        if (GetPointerObjectView(samples[i].snapshot, kind).ptrValue != firstView.ptrValue) {
            oss << "Pointer switched during this sample window; monotonic analysis is unreliable.\n";
            return oss.str();
        }
    }

    bool found = false;
    for (size_t offset = 0; offset + 4 <= firstView.rawSize; offset += 4) {
        bool nonDecreasing = true;
        bool nonIncreasing = true;
        uint32_t firstValue = 0;
        uint32_t lastValue = 0;
        for (size_t i = 0; i < samples.size(); ++i) {
            const auto view = GetPointerObjectView(samples[i].snapshot, kind);
            const uint32_t value = ReadLe32Ui(view.rawBytes, offset);
            if (i == 0) firstValue = value;
            if (i > 0) {
                const auto prevView = GetPointerObjectView(samples[i - 1].snapshot, kind);
                const uint32_t prev = ReadLe32Ui(prevView.rawBytes, offset);
                if (value < prev) nonDecreasing = false;
                if (value > prev) nonIncreasing = false;
            }
            lastValue = value;
        }
        if ((nonDecreasing || nonIncreasing) && firstValue != lastValue) {
            found = true;
            oss << "  u32 @ 0x" << std::hex << std::setw(2) << std::setfill('0') << offset
                << std::dec << " : " << firstValue << " -> " << lastValue
                << (nonDecreasing ? " (non-decreasing)" : " (non-increasing)") << "\n";
        }
    }

    for (size_t offset = 0; offset + 8 <= firstView.rawSize; offset += 8) {
        bool nonDecreasing = true;
        bool nonIncreasing = true;
        uint64_t firstValue = 0;
        uint64_t lastValue = 0;
        for (size_t i = 0; i < samples.size(); ++i) {
            const auto view = GetPointerObjectView(samples[i].snapshot, kind);
            const uint64_t value = ReadLe64Ui(view.rawBytes, offset);
            if (i == 0) firstValue = value;
            if (i > 0) {
                const auto prevView = GetPointerObjectView(samples[i - 1].snapshot, kind);
                const uint64_t prev = ReadLe64Ui(prevView.rawBytes, offset);
                if (value < prev) nonDecreasing = false;
                if (value > prev) nonIncreasing = false;
            }
            lastValue = value;
        }
        if ((nonDecreasing || nonIncreasing) && firstValue != lastValue) {
            found = true;
            oss << "  u64 @ 0x" << std::hex << std::setw(2) << std::setfill('0') << offset
                << " : 0x" << firstValue << " -> 0x" << lastValue
                << std::dec
                << (nonDecreasing ? " (non-decreasing)" : " (non-increasing)") << "\n";
        }
    }

    if (!found) {
        oss << "No monotonic aligned u32/u64 candidates found.\n";
    }
    return oss.str();
}

void RenderPrimaryPointerPanel(const char* label,
                               uintptr_t ptrValue,
                               const uint8_t* rawBytes,
                               size_t rawSize,
                               const std::string& monotonicText) {
    ImGui::Text("%s address : 0x%016llX", label, static_cast<unsigned long long>(ptrValue));
    std::string smallU32 = BuildSmallU32CandidatesText(rawBytes, rawSize);
    std::string hexDump = BuildByteDumpText(rawBytes, rawSize);
    ImGui::TextUnformatted("Small u32 candidates (< 0x10000):");
    ImGui::InputTextMultiline((std::string("##") + label + "_smallu32").c_str(),
                              smallU32.data(),
                              smallU32.size() + 1,
                              ImVec2(-1, ImGui::GetTextLineHeight() * 8),
                              ImGuiInputTextFlags_ReadOnly);
    ImGui::TextUnformatted("Monotonic candidates:");
    ImGui::InputTextMultiline((std::string("##") + label + "_mono").c_str(),
                              const_cast<char*>(monotonicText.c_str()),
                              monotonicText.size() + 1,
                              ImVec2(-1, ImGui::GetTextLineHeight() * 6),
                              ImGuiInputTextFlags_ReadOnly);
    ImGui::TextUnformatted("Hex dump (0x80 bytes):");
    ImGui::InputTextMultiline((std::string("##") + label + "_hex").c_str(),
                              hexDump.data(),
                              hexDump.size() + 1,
                              ImVec2(-1, ImGui::GetTextLineHeight() * 10),
                              ImGuiInputTextFlags_ReadOnly);
}

void SaveSkillLog(const std::vector<TimedSkillSnapshot>& samples,
                  const std::string& sessionLabel) {
    if (samples.empty()) return;

    std::string logDir = GetDllDirectory() + "\\skill-log";
    CreateDirectoryA(logDir.c_str(), nullptr);

    SYSTEMTIME st = {};
    GetLocalTime(&st);
    char fileName[128];
    snprintf(fileName,
             sizeof(fileName),
             "skill_capture_%04u%02u%02u_%02u%02u%02u_%03u.txt",
             st.wYear, st.wMonth, st.wDay,
             st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);

    std::ofstream out(logDir + "\\" + fileName, std::ios::out | std::ios::trunc);
    if (!out) return;

    out << std::fixed << std::setprecision(3);
    out << "SessionLabel: " << sessionLabel << "\n";
    out << "SampleCount: " << samples.size() << "\n\n";

    for (size_t i = 0; i < samples.size(); ++i) {
        const auto& sample = samples[i];
        const auto& snap = sample.snapshot;

        out << std::dec;
        out << "Sample[" << i << "]\n";
        out << "OffsetSeconds: " << sample.offsetSeconds << "\n";
        out << "LastSkillHotkey: \"" << sample.lastSkillHotkeyName << "\"\n";
        out << "LastSkillHotkeyTickMs: " << sample.lastSkillHotkeyTickMs << "\n";
        out << "LastOutgoingHeader: 0x" << std::hex << sample.lastOutgoingHeader << "\n";
        out << "LastOutgoingSize: " << std::dec << sample.lastOutgoingSize << "\n";
        out << "LastOutgoingBufferState: " << sample.lastOutgoingBufferState << "\n";
        out << "LastOutgoingTickMs: " << sample.lastOutgoingTickMs << "\n";
        out << "LastUseSkillValid: " << (sample.skillUsePacket.valid ? "true" : "false") << "\n";
        out << "LastUseSkillTickMs: " << sample.skillUsePacket.tickMs << "\n";
        out << "LastUseSkillOffset: " << sample.skillUsePacket.packetOffset << "\n";
        out << "LastUseSkillCapturedBytes: " << sample.skillUsePacket.packetSize << "\n";
        out << "LastUseSkillTotalBufferSize: " << sample.skillUsePacket.totalBufferSize << "\n";
        out << "LastUseSkillFresh: " << (IsSkillUsePacketFresh(sample) ? "true" : "false") << "\n";
        out << "LastUseSkillClass: " << GetUseSkillTailClass(sample.skillUsePacket) << "\n";
        out << "CharSkillbar: 0x" << std::hex << snap.charSkillbarAddr << "\n";
        out << "AsContext   : 0x" << std::hex << snap.asContextAddr << "\n";
        out << std::dec << "Position    : "
            << snap.position[0] << ", " << snap.position[1] << ", " << snap.position[2] << "\n";
        out << "u32@0x10    : " << ReadLe32Ui(snap.rawCharSkillbar, 0x10) << "\n";
        out << "u32@0x18    : " << ReadLe32Ui(snap.rawCharSkillbar, 0x18) << "\n";
        out << "u32@0x24    : " << ReadLe32Ui(snap.rawCharSkillbar, 0x24) << "\n";
        out << "u64@0x08    : 0x" << std::hex << ReadLe64Ui(snap.rawCharSkillbar, 0x08) << "\n";
        out << "u64@0x40    : 0x" << std::hex << ReadLe64Ui(snap.rawCharSkillbar, 0x40) << "\n";
        out << "u64@0x48    : 0x" << std::hex << ReadLe64Ui(snap.rawCharSkillbar, 0x48) << "\n";
        out << "u64@0x60    : 0x" << std::hex << ReadLe64Ui(snap.rawCharSkillbar, 0x60) << "\n\n";
        out << "SlotSummary\n" << BuildSlotSummaryText(snap) << "\n";
        out << "UseSkillPacket\n" << BuildSkillUsePacketHex(sample.skillUsePacket) << "\n";
        out << "UseSkillFieldTable\n" << BuildSkillUsePacketFieldTable(sample.skillUsePacket) << "\n";
        out << "Ptr48SmallU32\n" << BuildSmallU32CandidatesText(snap.rawPtr48, sizeof(snap.rawPtr48)) << "\n";
        out << "Ptr60SmallU32\n" << BuildSmallU32CandidatesText(snap.rawPtr60, sizeof(snap.rawPtr60)) << "\n";
        out << "Ptr40Summary\n" << BuildByteDumpText(snap.rawPtr40, sizeof(snap.rawPtr40)) << "\n";
        out << "Ptr48Summary\n" << BuildByteDumpText(snap.rawPtr48, sizeof(snap.rawPtr48)) << "\n";
        out << "Ptr60Summary\n" << BuildByteDumpText(snap.rawPtr60, sizeof(snap.rawPtr60)) << "\n";
        out << "RawDump\n" << BuildRawDumpText(snap) << "\n";
        if (!sample.diffFromFirst.empty()) {
            out << "DiffFromFirst\n" << sample.diffFromFirst << "\n";
        }
        out << "\n";
    }

    out << "UseSkillPrimarySamples\n" << BuildUseSkillSampleTable(samples, 0x29) << "\n";
    out << "UseSkillSecondarySamples\n" << BuildUseSkillSampleTable(samples, 0x25) << "\n";
    out << "Ptr48MonotonicCandidates\n" << BuildMonotonicCandidateText(samples, 48) << "\n";
    out << "Ptr60MonotonicCandidates\n" << BuildMonotonicCandidateText(samples, 60) << "\n";
}
}

// Initialize static members
int ImGuiManager::m_selectedPacketLogIndex = -1;
std::string ImGuiManager::m_parsedPayloadBuffer = "";
std::string ImGuiManager::m_fullLogEntryBuffer = "";

bool ImGuiManager::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, HWND hwnd) {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags = ImGuiConfigFlags_NoMouseCursorChange;

    GUIStyle::LoadAppFont();
    GUIStyle::ApplyCustomStyle();

    if (!ImGui_ImplWin32_Init(hwnd)) return false;
    if (!ImGui_ImplDX11_Init(device, context)) return false;

    return true;
}

void ImGuiManager::NewFrame() {
    ImGui_ImplDX11_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
}

void ImGuiManager::Render(ID3D11DeviceContext* context, ID3D11RenderTargetView* mainRenderTargetView) {
    ImGui::EndFrame();
    ImGui::Render();
    context->OMSetRenderTargets(1, &mainRenderTargetView, NULL);
    ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
}


// --- Helper Functions for Rendering UI Sections ---

void ImGuiManager::RenderHints() {
    // Hints moved to top
    ImGui::TextDisabled("Hint: Press INSERT to show/hide window.");
    ImGui::TextDisabled("Hint: Press DELETE / END / Numpad Del to unload DLL.");
    ImGui::Separator();
}

void ImGuiManager::RenderInfoSection() {
    // --- Info Section ---
    if (ImGui::CollapsingHeader("Info")) {
        ImGui::Text("KX Packet Inspector by Krixx");
        ImGui::Text("Visit kxtools.xyz for more tools!");
        ImGui::Separator();

        // GitHub Link
        ImGui::Text("GitHub:");
        ImGui::SameLine();
        if (ImGui::Button("Repository")) {
            ShellExecuteA(NULL, "open", "https://github.com/Krixx1337/kx-packet-inspector", NULL, NULL, SW_SHOWNORMAL);
        }

        // kxtools.xyz Link
        ImGui::Text("Website:");
        ImGui::SameLine();
        if (ImGui::Button("kxtools.xyz")) {
             ShellExecuteA(NULL, "open", "https://kxtools.xyz", NULL, NULL, SW_SHOWNORMAL);
        }

        // Discord Link
        ImGui::Text("Discord:");
        ImGui::SameLine();
        if (ImGui::Button("Join Server")) {
             ShellExecuteA(NULL, "open", "https://discord.gg/z92rnB4kHm", NULL, NULL, SW_SHOWNORMAL);
        }
        ImGui::Spacing();
    }
}

void ImGuiManager::RenderStatusControlsSection() {
    // --- Status & Controls Section ---
    if (ImGui::CollapsingHeader("Status")) {
        // Status content
        const char* presentStatusStr = (kx::g_presentHookStatus == kx::HookStatus::OK) ? "OK" : "Failed";
        ImGui::Text("Present Hook: %s", presentStatusStr);

        const char* msgSendStatusStr;
        switch (kx::g_msgSendHookStatus) {
            case kx::HookStatus::OK:       msgSendStatusStr = "OK"; break;
            case kx::HookStatus::Failed:   msgSendStatusStr = "Failed"; break;
            case kx::HookStatus::Unknown:
            default:                       msgSendStatusStr = "Not Found/Hooked"; break;
        }
        ImGui::Text("MsgSend Hook: %s", msgSendStatusStr);

        if (kx::g_msgSendAddress != 0) {
            ImGui::Text("MsgSend Address: 0x%p", (void*)kx::g_msgSendAddress);
            ImGui::SameLine();
            if (ImGui::SmallButton("Copy##SendAddr")) {
                char addrBuf[32];
                snprintf(addrBuf, sizeof(addrBuf), "0x%p", (void*)kx::g_msgSendAddress);
                ImGui::SetClipboardText(addrBuf);
            }
        } else {
            ImGui::Text("MsgSend Address: N/A");
        }

        const char* msgRecvStatusStr;
         switch (kx::g_msgRecvHookStatus) {
            case kx::HookStatus::OK:       msgRecvStatusStr = "OK"; break;
            case kx::HookStatus::Failed:   msgRecvStatusStr = "Failed"; break;
            case kx::HookStatus::Unknown:  msgRecvStatusStr = "Not Found/Hooked"; break;
            default:                       msgRecvStatusStr = "Unknown Status"; break;
        }
        ImGui::Text("MsgRecv Hook: %s", msgRecvStatusStr);

        if (kx::g_msgRecvAddress != 0) {
            ImGui::Text("MsgRecv Address: 0x%p", (void*)kx::g_msgRecvAddress);
            ImGui::SameLine();
            if (ImGui::SmallButton("Copy##RecvAddr")) {
                char addrBuf[32];
                snprintf(addrBuf, sizeof(addrBuf), "0x%p", (void*)kx::g_msgRecvAddress);
                ImGui::SetClipboardText(addrBuf);
            }
        } else {
            ImGui::Text("MsgRecv Address: N/A");
        }
    }
}

void ImGuiManager::RenderBuffProbeSection() {
    if (ImGui::CollapsingHeader("Buff Probe")) {
        RenderLatestSmsgPanel(
            "Latest 0x0028",
            "##Latest0028Text",
            static_cast<uint16_t>(kx::SMSG_HeaderId::AGENT_ATTRIBUTE_UPDATE),
            "Watch this while gaining and losing Alacrity. Compare baseline, apply, and remove windows.");
        RenderLatestSmsgPanel(
            "Latest 0x0016",
            "##Latest0016Text",
            static_cast<uint16_t>(kx::SMSG_HeaderId::AGENT_STATE_BULK),
            "Use this to line up 0x0016 timing with 0x0028 and check whether bulk state packets accompany Alacrity changes.");
        RenderLatestSmsgPanel(
            "Latest 0x0017",
            "##Latest0017Text",
            static_cast<uint16_t>(kx::SMSG_HeaderId::SKILL_UPDATE),
            "Watch this right after spending a charge, entering cooldown, and when the skill becomes ready again.");
        std::string cooldownTimeline = BuildRecentCooldownPacketTimeline();
        ImGui::TextUnformatted("Recent Cooldown Packet Timeline");
        ImGui::InputTextMultiline(
            "##CooldownPacketTimeline",
            cooldownTimeline.data(),
            cooldownTimeline.size() + 1,
            ImVec2(-1, ImGui::GetTextLineHeight() * 8),
            ImGuiInputTextFlags_ReadOnly);
    }
}

void ImGuiManager::RenderSkillCooldownMonitorSection() {
    if (!ImGui::CollapsingHeader("Skill CD Monitor", ImGuiTreeNodeFlags_DefaultOpen)) {
        return;
    }

    static bool s_showLegacyCdMonitorControls = false;

    ImGui::Text("Tracker      : %s", kx::SkillCooldown::GetStatusLine().c_str());
    ImGui::Text("API          : %s", kx::SkillCooldown::GetApiStatusLine().c_str());
    ImGui::Text("DB           : %s", kx::SkillCooldown::GetDbStatusLine().c_str());
    ImGui::Text("Live Skillbar: %s", kx::SkillTrace::GetStatusLine().c_str());
    ImGui::Text("Probe        : %s", kx::SkillTrace::GetProbeStatusLine().c_str());
    const auto estimated = kx::SkillCooldown::GetEstimatedCooldownDisplay();
    if (estimated.available) {
        ImGui::Separator();
        ImGui::TextUnformatted("Estimated Cooldown (Internal Poll)");
        ImGui::Text("hotkey=%s skill=%s", estimated.hotkey.c_str(), estimated.skillName.c_str());
        ImGui::Text("state=%s remaining=%.2fs (%s)",
                    estimated.stateText.c_str(),
                    static_cast<double>(estimated.remainingMs) / 1000.0,
                    estimated.estimated ? "estimated" : "observed");
        ImGui::Text("duration=%.2fs", static_cast<double>(estimated.durationMs) / 1000.0);
        ImGui::ProgressBar(estimated.progress01, ImVec2(-1.0f, 0.0f));
        ImGui::Text("ptr40_value=0x%llX ptr60_value=0x%llX",
                    static_cast<unsigned long long>(estimated.ptr40Value),
                    static_cast<unsigned long long>(estimated.ptr60Value));
    }
    const bool globalPaused = kx::g_globalPauseMode.load(std::memory_order_acquire);
    if (ImGui::Button(globalPaused ? "Global Pause: ON" : "Global Pause: OFF")) {
        const bool nextPaused = !globalPaused;
        kx::g_globalPauseMode.store(nextPaused, std::memory_order_release);
        kx::g_capturePaused.store(nextPaused, std::memory_order_release);
        if (nextPaused) {
            kx::SkillTrace::CancelSkillbarProbe("global pause");
        }
    }
    if (globalPaused) {
        ImGui::TextUnformatted("Global Pause is active.");
    }
    if (ImGui::Button("Reload Build")) {
        kx::SkillCooldown::ReloadBuildData();
    }
    ImGui::SameLine();
    if (ImGui::Button("Edit Overrides")) {
        kx::SkillCooldown::OpenOverridesFile();
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh API")) {
        kx::SkillCooldown::RefreshApiData();
    }
    ImGui::SameLine();
    if (ImGui::Button("Save Profile")) {
        kx::SkillCooldown::SaveCurrentProfile();
    }
    ImGui::Separator();
    ImGui::Checkbox("Show Legacy Monitor Controls", &s_showLegacyCdMonitorControls);
    if (!s_showLegacyCdMonitorControls) {
        ImGui::TextUnformatted("Legacy controls are hidden (probe buttons/debug assist).");
    } else {
        ImGui::TextUnformatted("Legacy controls are currently disabled in internal watcher mode.");
    }

#if 0
    // Legacy probe triggers are temporarily disabled in internal watcher mode.
    ImGui::BeginDisabled(globalPaused);
    if (ImGui::Button("Probe Idle Baseline")) {
        kx::SkillTrace::StartSkillbarProbe("idle_baseline");
    }
    ImGui::SameLine();
    if (ImGui::Button("Probe Weapon Swap")) {
        kx::SkillTrace::StartSkillbarProbe("weapon_swap");
    }
    ImGui::SameLine();
    if (ImGui::Button("Probe Tome Enter")) {
        kx::SkillTrace::StartSkillbarProbe("tome_enter");
    }
    ImGui::SameLine();
    if (ImGui::Button("Probe Tome Exit")) {
        kx::SkillTrace::StartSkillbarProbe("tome_exit");
    }
    ImGui::SameLine();
    if (ImGui::Button("Probe Skillbar Change")) {
        kx::SkillTrace::StartSkillbarProbe("skillbar_change");
    }
    ImGui::EndDisabled();

    // Legacy debug assist controls are temporarily disabled in internal watcher mode.
    static const char* kDebugAssistHotkeys[] = { "6", "Z", "X", "C", "Q" };
    auto debugAssist = kx::SkillCooldown::GetDebugAssistState();
    int selectedHotkey = 3;
    for (int i = 0; i < IM_ARRAYSIZE(kDebugAssistHotkeys); ++i) {
        if (debugAssist.targetHotkey == kDebugAssistHotkeys[i]) {
            selectedHotkey = i;
            break;
        }
    }
    ImGui::Separator();
    ImGui::TextUnformatted("Debug Assist");
    ImGui::BeginDisabled(globalPaused);
    bool debugAssistEnabled = debugAssist.enabled;
    if (ImGui::Checkbox("Enable Debug Assist", &debugAssistEnabled)) {
        kx::SkillCooldown::SetDebugAssistEnabled(debugAssistEnabled);
        debugAssist = kx::SkillCooldown::GetDebugAssistState();
    }
    ImGui::SameLine();
    bool autoMode = debugAssist.autoMode;
    if (ImGui::Checkbox("Auto Trigger", &autoMode)) {
        kx::SkillCooldown::SetDebugAssistAutoMode(autoMode);
        debugAssist = kx::SkillCooldown::GetDebugAssistState();
    }
    ImGui::SameLine();
    if (ImGui::Combo("Target Hotkey", &selectedHotkey, kDebugAssistHotkeys, IM_ARRAYSIZE(kDebugAssistHotkeys))) {
        kx::SkillCooldown::SetDebugAssistTargetHotkey(kDebugAssistHotkeys[selectedHotkey]);
        debugAssist = kx::SkillCooldown::GetDebugAssistState();
    }
    if (ImGui::Button("Arm Debug Assist Once")) {
        kx::SkillCooldown::ArmDebugAssistOnce();
        debugAssist = kx::SkillCooldown::GetDebugAssistState();
    }
    ImGui::SameLine();
    if (ImGui::Button("Clear Debug Snapshot")) {
        kx::SkillCooldown::ClearDebugAssistSnapshot();
        debugAssist = kx::SkillCooldown::GetDebugAssistState();
    }
    ImGui::EndDisabled();
    ImGui::TextWrapped("Status       : %s", debugAssist.statusText.c_str());
    ImGui::TextWrapped("Snapshot     : %s", debugAssist.lastSnapshotText.c_str());
    ImGui::TextUnformatted("Note         : Auto Trigger arms debug assist for a single tracked hotkey press.");
    ImGui::TextUnformatted("Note         : CDPROBE sweep is disabled while using Debug Assist.");
#endif

    const auto liveRows = kx::SkillTrace::GetLiveSkillbarRows();
    if (!liveRows.empty()) {
        ImGui::Separator();
        ImGui::TextUnformatted("Live Skillbar Rows");
        if (ImGui::BeginTable("LiveSkillbarTable", 4, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("Slot");
            ImGui::TableSetupColumn("Skill");
            ImGui::TableSetupColumn("API ID");
            ImGui::TableSetupColumn("Source");
            ImGui::TableHeadersRow();
            for (const auto& row : liveRows) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::Text("%s", row.hotkeyName.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%s", row.skillName.c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%d", row.apiId);
                ImGui::TableNextColumn();
                ImGui::Text("%s (conf=%d)", row.source.c_str(), row.confidence);
            }
            ImGui::EndTable();
        }
    }
}

void ImGuiManager::RenderFilteringSection() {
    if (ImGui::CollapsingHeader("Filtering")) {
		// Reset Filters Button
		if (ImGui::Button("Reset Filters")) {
		    kx::g_packetFilterMode = kx::FilterMode::ShowAll;
		    kx::g_packetDirectionFilterMode = kx::DirectionFilterMode::ShowAll;
		    for (auto& pair : kx::g_packetHeaderFilterSelection) {
		        pair.second = false; // Uncheck all header filters
		    }
		    for (auto& pair : kx::g_specialPacketFilterSelection) {
		        pair.second = false; // Uncheck all special filters
		    }
		}
		ImGui::Separator(); // Add separator after the button

        // --- Global Direction Filter ---
        ImGui::Text("Show Direction:"); ImGui::SameLine();
        ImGui::RadioButton("All##Dir", reinterpret_cast<int*>(&kx::g_packetDirectionFilterMode), static_cast<int>(kx::DirectionFilterMode::ShowAll)); ImGui::SameLine();
        ImGui::RadioButton("Sent##Dir", reinterpret_cast<int*>(&kx::g_packetDirectionFilterMode), static_cast<int>(kx::DirectionFilterMode::ShowSentOnly)); ImGui::SameLine();
        ImGui::RadioButton("Received##Dir", reinterpret_cast<int*>(&kx::g_packetDirectionFilterMode), static_cast<int>(kx::DirectionFilterMode::ShowReceivedOnly));
        ImGui::Separator();

        // --- Header/Type Filter Mode ---
        ImGui::Text("Filter Mode:"); ImGui::SameLine();
        ImGui::RadioButton("Show All Types", reinterpret_cast<int*>(&kx::g_packetFilterMode), static_cast<int>(kx::FilterMode::ShowAll)); ImGui::SameLine();
        ImGui::RadioButton("Include Checked", reinterpret_cast<int*>(&kx::g_packetFilterMode), static_cast<int>(kx::FilterMode::IncludeOnly)); ImGui::SameLine();
        ImGui::RadioButton("Exclude Checked", reinterpret_cast<int*>(&kx::g_packetFilterMode), static_cast<int>(kx::FilterMode::Exclude));

        // --- Checkbox Section (only if mode is Include/Exclude) ---
        if (kx::g_packetFilterMode != kx::FilterMode::ShowAll) {
            ImGui::Separator();
            ImGui::BeginChild("FilterCheckboxRegion", ImVec2(0, 150.0f), true);
            ImGui::Indent();

            // Group Checkboxes
            if (ImGui::TreeNode("Sent Headers (CMSG)")) {
                for (auto& pair : kx::g_packetHeaderFilterSelection) {
                    if (pair.first.first == kx::PacketDirection::Sent) {
                        uint16_t headerId = pair.first.second;
                        bool& selected = pair.second;
                        std::string name = kx::GetPacketName(kx::PacketDirection::Sent, headerId); // Get name again for display
                        ImGui::Checkbox(name.c_str(), &selected);
                    }
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Received Headers (SMSG)")) {
                // Check if any SMSG headers are actually defined besides placeholder
                bool hasSmsgHeaders = false;
                for (const auto& pair : kx::g_packetHeaderFilterSelection) {
                    if (pair.first.first == kx::PacketDirection::Received) {
                        hasSmsgHeaders = true;
                        break;
                    }
                }

                if (hasSmsgHeaders) {
                    for (auto& pair : kx::g_packetHeaderFilterSelection) {
                        if (pair.first.first == kx::PacketDirection::Received) {
                            uint16_t headerId = pair.first.second;
                            bool& selected = pair.second;
                            std::string name = kx::GetPacketName(kx::PacketDirection::Received, headerId);
                            ImGui::Checkbox(name.c_str(), &selected);
                        }
                    }
                }
                else {
                    ImGui::TextDisabled(" (No known SMSG headers defined yet)");
                }
                ImGui::TreePop();
            }

            if (ImGui::TreeNode("Special Types")) {
                for (auto& pair : kx::g_specialPacketFilterSelection) {
                    kx::InternalPacketType type = pair.first;
                    bool& selected = pair.second;
                    std::string name = kx::GetSpecialPacketTypeName(type);
                    ImGui::Checkbox(name.c_str(), &selected);
                }
                ImGui::TreePop();
            }

            ImGui::Unindent();
            ImGui::EndChild();
        }
        ImGui::Separator();
    }
    ImGui::Spacing();
}

// Helper function to render a single row in the packet log
void ImGuiManager::RenderSinglePacketLogRow(const kx::PacketInfo& packet, int display_index, int original_log_index) {
    std::string displayLogEntry = kx::Utils::FormatDisplayLogEntryString(packet);

    ImGui::PushID(display_index);

    // --- Color Coding ---
    ImVec4 textColor;
    if (packet.direction == kx::PacketDirection::Sent) {
        textColor = ImVec4(0.4f, 0.7f, 1.0f, 1.0f); // Light Blue for Sent
    } else { // Received
        textColor = ImVec4(0.4f, 1.0f, 0.7f, 1.0f); // Light Green for Received
    }
    ImGui::PushStyleColor(ImGuiCol_Text, textColor);
    // --- End Color Coding ---

    // Calculate widths for layout to prevent the selectable from consuming the button's space.
    float button_width = ImGui::CalcTextSize("Copy").x + ImGui::GetStyle().FramePadding.x * 2.0f;
    float selectable_width = ImGui::GetContentRegionAvail().x - button_width - ImGui::GetStyle().ItemSpacing.x;

    // Use Selectable with an explicit width to leave space for the button
    bool is_selected = (m_selectedPacketLogIndex == original_log_index);
    if (ImGui::Selectable(displayLogEntry.c_str(), is_selected, ImGuiSelectableFlags_AllowDoubleClick, ImVec2(selectable_width, 0))) {
        m_selectedPacketLogIndex = original_log_index;
        // Clear buffer to force re-parsing when a new packet is selected
        m_parsedPayloadBuffer.clear();
        m_fullLogEntryBuffer.clear(); // Clear full log entry buffer
    }
    ImGui::PopStyleColor(); // Pop text color style

    // Place the button on the same line; it will now fit in the space reserved for it.
    ImGui::SameLine();
    if (ImGui::SmallButton("Copy")) {
        std::string fullLogEntry = kx::Utils::FormatFullLogEntryString(packet);
        ImGui::SetClipboardText(fullLogEntry.c_str());
    }

    ImGui::PopID();
}

// Acquires lock, applies filters, and returns only source indices to avoid payload copies.
std::vector<int> ImGuiManager::GetFilteredPacketIndicesSnapshot(size_t& out_total_packets) {
    std::vector<int> filtered_indices;
    out_total_packets = 0;
    {
        std::lock_guard<std::mutex> lock(kx::g_packetLogMutex);
        out_total_packets = kx::g_packetLog.size();
        filtered_indices = kx::Filtering::GetFilteredPacketIndices(kx::g_packetLog);
    }
    return filtered_indices;
}

// Renders the control buttons (Clear, Copy All) and checkbox (Pause) for the packet log.
void ImGuiManager::RenderPacketLogControls(size_t displayed_count, size_t total_count, const std::vector<int>& original_indices_snapshot) {
    // Define danger colors locally for the Clear Log button
    const ImVec4 dangerRed       = ImVec4(220.0f / 255.0f, 53.0f / 255.0f, 69.0f / 255.0f, 1.0f);
    const ImVec4 dangerRedHover  = ImVec4(std::min(dangerRed.x * 1.1f, 1.0f), std::min(dangerRed.y * 1.1f, 1.0f), std::min(dangerRed.z * 1.1f, 1.0f), 1.0f);
    const ImVec4 dangerRedActive = ImVec4(std::max(dangerRed.x * 0.9f, 0.0f), std::max(dangerRed.y * 0.9f, 0.0f), std::max(dangerRed.z * 0.9f, 0.0f), 1.0f);

    // Apply danger color to Clear Log button
    ImGui::PushStyleColor(ImGuiCol_Button, dangerRed);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, dangerRedHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, dangerRedActive);

    if (ImGui::Button("Clear Log")) {
        std::lock_guard<std::mutex> lock(kx::g_packetLogMutex);
        std::deque<kx::PacketInfo>().swap(kx::g_packetLog);
        m_selectedPacketLogIndex = -1; // Reset selected index
        m_parsedPayloadBuffer.clear(); // Clear parsed buffer
        m_fullLogEntryBuffer.clear(); // Clear full log entry buffer
    }

    ImGui::PopStyleColor(3); // Restore default button colors

    ImGui::SameLine();
    if (ImGui::Button("Copy All")) {
        if (!original_indices_snapshot.empty()) {
            std::stringstream ss;
            std::lock_guard<std::mutex> lock(kx::g_packetLogMutex);
            for (int index : original_indices_snapshot) {
                if (index >= 0 && static_cast<size_t>(index) < kx::g_packetLog.size()) {
                    ss << kx::Utils::FormatFullLogEntryString(kx::g_packetLog[index]) << "\n";
                }
            }
            ImGui::SetClipboardText(ss.str().c_str());
        }
    }

    ImGui::SameLine();
    bool pauseCapture = kx::g_capturePaused.load(std::memory_order_acquire);
    if (ImGui::Checkbox("Pause Capture", &pauseCapture)) {
        kx::g_capturePaused.store(pauseCapture, std::memory_order_release);
    }
}

// Renders the list of packets using ImGuiListClipper for efficiency.
void ImGuiManager::RenderPacketListWithClipping(const std::vector<int>& original_indices_snapshot) {
    if (!original_indices_snapshot.empty())
    {
        ImGuiListClipper clipper;
        clipper.Begin(static_cast<int>(original_indices_snapshot.size()));
		while (clipper.Step())
        {
			for (int display_index = clipper.DisplayStart; display_index < clipper.DisplayEnd; ++display_index)
			{
                const int originalIndex = original_indices_snapshot[display_index];
                kx::PacketInfo packetSnapshot;
                bool valid = false;
                {
                    std::lock_guard<std::mutex> lock(kx::g_packetLogMutex);
                    if (originalIndex >= 0 && static_cast<size_t>(originalIndex) < kx::g_packetLog.size()) {
                        packetSnapshot = kx::g_packetLog[originalIndex];
                        valid = true;
                    }
                }
                if (valid) {
				    RenderSinglePacketLogRow(packetSnapshot, display_index, originalIndex);
                }
			}
		}
        clipper.End();
    }
}

void ImGuiManager::RenderPacketLogSection() {
    // 1. Get filtered source indices
    size_t total_packets = 0;
    std::vector<int> original_indices_snapshot = GetFilteredPacketIndicesSnapshot(total_packets);

    // 2. Display statistics
    ImGui::Text("Packet Log (Showing: %zu / Total: %zu)", original_indices_snapshot.size(), total_packets);

    // 3. Render controls
    RenderPacketLogControls(original_indices_snapshot.size(), total_packets, original_indices_snapshot);

    ImGui::Spacing();

    // 4. Render scrolling list region
    float available_height = ImGui::GetContentRegionAvail().y;
    float log_section_height = available_height * 0.65f; // Allocate 65% of available height to log

    // Ensure a minimum height for the log section
    if (log_section_height < ImGui::GetTextLineHeight() * 10) { // Minimum 10 lines
        log_section_height = ImGui::GetTextLineHeight() * 10;
    }

    ImGui::BeginChild("PacketLogScrollingRegion", ImVec2(0, log_section_height), true, ImGuiWindowFlags_HorizontalScrollbar);

    // 5. Render packet list using clipper
    RenderPacketListWithClipping(original_indices_snapshot);

    // 6. Handle auto-scrolling
    if (!original_indices_snapshot.empty() && ImGui::GetScrollY() >= ImGui::GetScrollMaxY()) {
        ImGui::SetScrollHereY(1.0f);
    }

    ImGui::EndChild();
}

void ImGuiManager::RenderSelectedPacketDetailsSection() {
    if (ImGui::CollapsingHeader("Selected Packet Details", ImGuiTreeNodeFlags_DefaultOpen)) {
        if (m_selectedPacketLogIndex != -1) {
            std::lock_guard<std::mutex> lock(kx::g_packetLogMutex);
            if (m_selectedPacketLogIndex >= 0 && m_selectedPacketLogIndex < kx::g_packetLog.size()) {
                const kx::PacketInfo& selectedPacket = kx::g_packetLog[m_selectedPacketLogIndex];

                // Only re-parse if the buffer is empty (first time selected or cleared)
                // or if the selected packet has changed (though m_selectedPacketLogIndex handles this)
                if (m_parsedPayloadBuffer.empty() || m_fullLogEntryBuffer.empty()) { // Check both buffers
                    m_fullLogEntryBuffer = kx::Utils::FormatFullLogEntryString(selectedPacket); // Populate full log entry
                    auto parsedDataOpt = kx::Parsing::GetParsedDataTooltipString(selectedPacket);
                    if (parsedDataOpt.has_value()) {
                        m_parsedPayloadBuffer = parsedDataOpt.value();
                    } else {
                        m_parsedPayloadBuffer = "No specific parser available for this packet type.";
                    }
                }

                ImGui::Text("Full Log Entry:");
                ImGui::InputTextMultiline("##FullLogEntry", (char*)m_fullLogEntryBuffer.c_str(), m_fullLogEntryBuffer.size() + 1, ImVec2(-1, ImGui::GetTextLineHeight() * 3), ImGuiInputTextFlags_ReadOnly);

                ImGui::Text("Parsed Payload:");
                ImGui::InputTextMultiline("##ParsedPayload", (char*)m_parsedPayloadBuffer.c_str(), m_parsedPayloadBuffer.size() + 1, ImVec2(-1, ImGui::GetTextLineHeight() * 10), ImGuiInputTextFlags_ReadOnly);

                if (ImGui::Button("Copy All Details")) {
                    std::stringstream ss;
                    ss << "Full Log Entry:\n" << m_fullLogEntryBuffer << "\n\n";
                    ss << "Parsed Payload:\n" << m_parsedPayloadBuffer;
                    ImGui::SetClipboardText(ss.str().c_str());
                }

            } else {
                m_selectedPacketLogIndex = -1; // Invalid index, reset
                m_parsedPayloadBuffer.clear();
                m_fullLogEntryBuffer.clear(); // Clear full log entry buffer
                ImGui::Text("Selected packet no longer exists (e.g., log cleared).");
            }
        } else {
            ImGui::Text("Select a packet from the log above to view its parsed details here.");
        }
    }
}


// --- Main Window Rendering Function ---

void ImGuiManager::RenderPacketInspectorWindow() {
    // Check if the window should be closed (user clicked 'x').
    if (!kx::g_isInspectorWindowOpen.load(std::memory_order_acquire)) {
        return;
    }

    std::string windowTitle = "KX Packet Inspector v";
    windowTitle += kx::APP_VERSION;

    // Set minimum window size constraints before calling Begin
    ImGui::SetNextWindowSizeConstraints(ImVec2(350.0f, 200.0f), ImVec2(FLT_MAX, FLT_MAX));
    bool inspectorWindowOpen = kx::g_isInspectorWindowOpen.load(std::memory_order_acquire);
    ImGui::Begin(windowTitle.c_str(), &inspectorWindowOpen);
    kx::g_isInspectorWindowOpen.store(inspectorWindowOpen, std::memory_order_release);

    RenderHints();
    RenderInfoSection();
    RenderStatusControlsSection();
    RenderSkillCooldownMonitorSection();
    RenderBuffProbeSection();
    RenderSkillMemorySection();

    ImGui::End();
}


void ImGuiManager::RenderUI()
{
    // Only render the inspector window if the visibility flag is set
    if (kx::g_showInspectorWindow.load(std::memory_order_acquire)) {
        RenderPacketInspectorWindow();
    }

    // You can add other UI elements here if needed
    // ImGui::ShowDemoWindow(); // Keep demo window for reference if needed during dev
}

void ImGuiManager::Shutdown() {
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiManager::RenderSkillMemorySection() {
    if (ImGui::CollapsingHeader("Skill Memory")) {
        static SkillbarSnapshot s_snapBefore;
        static std::vector<TimedSkillSnapshot> s_samples;
        static std::array<double, 6> s_captureOffsets = { 0.0, 0.5, 1.0, 2.0, 4.0, 8.0 };
        static bool s_multiCaptureArmed = false;
        static double s_captureStartAt = 0.0;
        static size_t s_nextCaptureIndex = 0;
        static std::string s_diffOutput = "No diff captured yet.";
        static std::string s_captureStatus = "Idle.";
        static bool s_initResult = false;
        static bool s_initTried = false;
        static uintptr_t s_dbgModuleBase = 0;
        static uintptr_t s_dbgFnAddr = 0;
        static uintptr_t s_dbgCtxAddr = 0;
        static uintptr_t s_dbgChCliCtx = 0;
        static uintptr_t s_dbgLocalChar = 0;
        static float s_dbgPos[3] = {};
        static std::string s_lastUseSkillHex = "No CMSG_USE_SKILL packet captured yet.";
        static std::string s_lastUseSkillFields = "No CMSG_USE_SKILL packet captured yet.";
        static std::string s_primaryUseSkillSamples = "No samples captured yet.";
        static std::string s_secondaryUseSkillSamples = "No samples captured yet.";
        static int s_rechargeTestSlot = 0;
        static std::string s_rechargeCurrentChCli = "No recharge test run yet.";
        static std::string s_rechargeCurrentChar = "No recharge test run yet.";
        static std::string s_rechargeGameChCli = "No recharge test run yet.";
        static std::string s_rechargeGameChar = "No recharge test run yet.";
        static bool s_showLegacySkillMemoryTools = false;

        auto runProbe = [&]() {
            s_initTried = true;
            s_initResult = false;
            s_dbgModuleBase = 0;
            s_dbgFnAddr = 0;
            s_dbgCtxAddr = 0;
            s_dbgChCliCtx = 0;
            s_dbgLocalChar = 0;
            std::memset(s_dbgPos, 0, sizeof(s_dbgPos));

            HMODULE hMod = GetModuleHandleA("Gw2-64.exe");
            if (!hMod) hMod = GetModuleHandleA("Gw2.exe");
            if (!hMod) hMod = GetModuleHandleA(nullptr);
            s_dbgModuleBase = reinterpret_cast<uintptr_t>(hMod);

            s_initResult = g_skillReader.init();
            s_dbgFnAddr = g_skillReader.getCtxFnAddr();
            s_dbgCtxAddr = g_skillReader.refreshContextCollection();

            if (s_dbgCtxAddr) {
                __try {
                    s_dbgChCliCtx = *reinterpret_cast<uintptr_t*>(s_dbgCtxAddr + 0x98);
                    if (s_dbgChCliCtx) {
                        s_dbgLocalChar = *reinterpret_cast<uintptr_t*>(s_dbgChCliCtx + 0x98);
                        if (s_dbgLocalChar) {
                            std::memcpy(s_dbgPos, reinterpret_cast<void*>(s_dbgLocalChar + 0x480), sizeof(s_dbgPos));
                        }
                    }
                }
                __except (EXCEPTION_EXECUTE_HANDLER) {
                    s_dbgChCliCtx = 0;
                    s_dbgLocalChar = 0;
                    std::memset(s_dbgPos, 0, sizeof(s_dbgPos));
                }
            }
        };

        if (ImGui::Button("Re-init & Probe") || !s_initTried) {
            runProbe();
        }

        auto snap = g_skillReader.readNoWait();
        const double now = ImGui::GetTime();

        if (s_multiCaptureArmed && s_nextCaptureIndex < s_captureOffsets.size()) {
            while (s_nextCaptureIndex < s_captureOffsets.size() &&
                   now >= s_captureStartAt + s_captureOffsets[s_nextCaptureIndex]) {
                TimedSkillSnapshot timedSample;
                timedSample.offsetSeconds = s_captureOffsets[s_nextCaptureIndex];
                timedSample.snapshot = snap;
                timedSample.lastSkillHotkey = kx::g_lastSkillHotkey.load(std::memory_order_acquire);
                timedSample.lastSkillHotkeyName = kx::GetSkillHotkeyName(timedSample.lastSkillHotkey);
                timedSample.lastSkillHotkeyTickMs = kx::g_lastSkillHotkeyTickMs.load(std::memory_order_acquire);
                timedSample.lastOutgoingHeader = kx::g_lastOutgoingHeader.load(std::memory_order_acquire);
                timedSample.lastOutgoingSize = kx::g_lastOutgoingSize.load(std::memory_order_acquire);
                timedSample.lastOutgoingBufferState = kx::g_lastOutgoingBufferState.load(std::memory_order_acquire);
                timedSample.lastOutgoingTickMs = kx::g_lastOutgoingTickMs.load(std::memory_order_acquire);
                timedSample.skillUsePacket = SnapshotSkillUsePacket();

                if (!s_samples.empty()) {
                    char diffBuf[4096];
                    memset(diffBuf, 0, sizeof(diffBuf));
                    SkillMemoryReader::diffDump(s_samples.front().snapshot, timedSample.snapshot, diffBuf, sizeof(diffBuf));
                    timedSample.diffFromFirst = diffBuf;
                }

                s_samples.push_back(timedSample);
                ++s_nextCaptureIndex;
            }

            if (s_nextCaptureIndex >= s_captureOffsets.size()) {
                s_multiCaptureArmed = false;
                if (!s_samples.empty()) {
                    s_diffOutput = s_samples.back().diffFromFirst.empty()
                        ? "Sampling complete. First sample has no diff."
                        : s_samples.back().diffFromFirst;
                    SaveSkillLog(s_samples, "0s,0.5s,1s,2s,4s,8s");
                    s_primaryUseSkillSamples = BuildUseSkillSampleTable(s_samples, 0x29);
                    s_secondaryUseSkillSamples = BuildUseSkillSampleTable(s_samples, 0x25);
                    s_captureStatus = "Multi-sample complete. Saved to skill-log.";
                } else {
                    s_diffOutput = "Sampling completed without valid samples.";
                    s_captureStatus = "Sampling failed.";
                }
            }
        }

        ImGui::Text("Capture      : %s", s_captureStatus.c_str());
        ImGui::Text("Auto trace   : %s", kx::SkillTrace::GetStatusLine().c_str());
        ImGui::Text("Probe status : %s", kx::SkillTrace::GetProbeStatusLine().c_str());
        ImGui::Text("Last hotkey  : %s",
                    kx::GetSkillHotkeyName(kx::g_lastSkillHotkey.load(std::memory_order_acquire)));
        {
            const auto useSkillPacket = SnapshotSkillUsePacket();
            s_lastUseSkillHex = BuildSkillUsePacketHex(useSkillPacket);
            s_lastUseSkillFields = BuildSkillUsePacketFieldTable(useSkillPacket);
            ImGui::Text("Last use-skill: %s @ %llu",
                        GetUseSkillTailClass(useSkillPacket).c_str(),
                        useSkillPacket.tickMs);
        }
        if (s_multiCaptureArmed && s_nextCaptureIndex < s_captureOffsets.size()) {
            ImGui::Text("Next sample  : %.2f sec (target %.2f sec, %zu/%zu captured)",
                        std::max(0.0, (s_captureStartAt + s_captureOffsets[s_nextCaptureIndex]) - now),
                        s_captureOffsets[s_nextCaptureIndex],
                        s_samples.size(),
                        s_captureOffsets.size());
        }
        ImGui::Separator();
        ImGui::Checkbox("Show Legacy Research Controls", &s_showLegacySkillMemoryTools);
        if (!s_showLegacySkillMemoryTools) {
            ImGui::TextUnformatted("Legacy controls are hidden (multi-sample/probe/recharge/raw dumps).");
            ImGui::Text("char_skillbar=0x%llX ptr40=0x%llX ptr48=0x%llX ptr60=0x%llX",
                        static_cast<unsigned long long>(snap.charSkillbarAddr),
                        static_cast<unsigned long long>(snap.ptr40Addr),
                        static_cast<unsigned long long>(snap.ptr48Addr),
                        static_cast<unsigned long long>(snap.ptr60Addr));
            return;
        }

        if (ImGui::Button("Start Multi Sample (0,0.5,1,2,4,8s)")) {
            s_snapBefore = snap;
            s_samples.clear();
            s_multiCaptureArmed = snap.valid;
            s_captureStartAt = now;
            s_nextCaptureIndex = 0;
            s_primaryUseSkillSamples = "No samples captured yet.";
            s_secondaryUseSkillSamples = "No samples captured yet.";
            {
                std::lock_guard<std::mutex> lock(kx::g_lastSkillUsePacketMutex);
                kx::g_lastSkillUsePacket = {};
            }
            kx::g_lastSkillHotkey.store(static_cast<int>(kx::SkillHotkeyId::None), std::memory_order_release);
            kx::g_lastSkillHotkeyTickMs.store(0, std::memory_order_release);
            s_diffOutput = snap.valid
                ? "Multi-sample armed. Trigger one skill once, then wait for completion."
                : "Current snapshot invalid. Probe again before sampling.";
            s_captureStatus = snap.valid
                ? "Sampler armed. Trigger one skill once and do not press anything else."
                : "Sampler could not start because the current snapshot is invalid.";
        }

        if (ImGui::Button("Probe Idle Baseline##skillmem")) {
            kx::SkillTrace::StartSkillbarProbe("idle_baseline");
        }
        ImGui::SameLine();
        if (ImGui::Button("Probe Weapon Swap##skillmem")) {
            kx::SkillTrace::StartSkillbarProbe("weapon_swap");
        }
        ImGui::SameLine();
        if (ImGui::Button("Probe Tome Enter##skillmem")) {
            kx::SkillTrace::StartSkillbarProbe("tome_enter");
        }
        ImGui::SameLine();
        if (ImGui::Button("Probe Tome Exit##skillmem")) {
            kx::SkillTrace::StartSkillbarProbe("tome_exit");
        }

        ImGui::Text("Diff Output:");
        ImGui::InputTextMultiline("##SkillMemoryDiff",
                                  s_diffOutput.data(),
                                  s_diffOutput.size() + 1,
                                  ImVec2(-1, ImGui::GetTextLineHeight() * 16),
                                  ImGuiInputTextFlags_ReadOnly);

        ImGui::Separator();
        ImGui::TextUnformatted("IsRecharging vtable[7] Validation");
        ImGui::SliderInt("Test slot", &s_rechargeTestSlot, 0, 9);
        if (ImGui::Button("Run Recharge Test")) {
            const uint32_t slot = static_cast<uint32_t>(s_rechargeTestSlot);
            s_rechargeCurrentChCli = BuildRechargeTestText(
                g_skillReader.testIsRechargingFromSnapshot(snap, SkillbarObjectKind::ChCliSkillbar, slot, false));
            s_rechargeCurrentChar = BuildRechargeTestText(
                g_skillReader.testIsRechargingFromSnapshot(snap, SkillbarObjectKind::CharSkillbar, slot, false));
            s_rechargeGameChCli = BuildRechargeTestText(
                g_skillReader.requestIsRechargingOnGameThread(SkillbarObjectKind::ChCliSkillbar, slot, 150));
            s_rechargeGameChar = BuildRechargeTestText(
                g_skillReader.requestIsRechargingOnGameThread(SkillbarObjectKind::CharSkillbar, slot, 150));
        }
        ImGui::TextWrapped("Suggested validation order: fixed Firebrand build, verify slot 0..4 idle reads false, then press 6/Z/X/C/Q and immediately re-test the mapped slot. If the non-game-thread call stays false while the game-thread call changes, treat that as thread-affinity.");
        ImGui::BulletText("Current thread / ChCliSkillbar: %s", s_rechargeCurrentChCli.c_str());
        ImGui::BulletText("Current thread / CharSkillbar : %s", s_rechargeCurrentChar.c_str());
        ImGui::BulletText("Game thread / ChCliSkillbar   : %s", s_rechargeGameChCli.c_str());
        ImGui::BulletText("Game thread / CharSkillbar    : %s", s_rechargeGameChar.c_str());
        ImGui::TextWrapped("Packet tail mapping reference: 0x29:00 -> slot 0, 0x29:01 -> slot 1, 0x25:83 -> slot 2, 0x29:04 -> slot 4, 0x29:0C -> profession slot pending.");

        if (ImGui::CollapsingHeader("Advanced")) {
            const std::string traceSummary = kx::SkillTrace::GetLastTraceSummary();
            const std::string probeSummary = kx::SkillTrace::GetLastProbeSummary();
            ImGui::TextWrapped("Last trace: %s", traceSummary.c_str());
            ImGui::TextWrapped("Last probe: %s", probeSummary.c_str());
            ImGui::Separator();
            ImGui::Text("Module base : 0x%llX", static_cast<unsigned long long>(s_dbgModuleBase));
            ImGui::Text("init()      : %s", s_initResult ? "OK" : "FAILED");
            ImGui::Text("FnAddr      : 0x%llX", static_cast<unsigned long long>(s_dbgFnAddr));
            ImGui::Text("CtxColl*    : 0x%llX", static_cast<unsigned long long>(s_dbgCtxAddr));
            ImGui::Text("ChCliCtx*   : 0x%llX", static_cast<unsigned long long>(s_dbgChCliCtx));
            ImGui::Text("LocalChar*  : 0x%llX", static_cast<unsigned long long>(s_dbgLocalChar));
            ImGui::Text("Position    : %.1f %.1f %.1f", s_dbgPos[0], s_dbgPos[1], s_dbgPos[2]);
            ImGui::Separator();
            ImGui::Text("0x10 status candidate : %u (0x%08X)", ReadLe32Ui(snap.rawCharSkillbar, 0x10), ReadLe32Ui(snap.rawCharSkillbar, 0x10));
            ImGui::Text("0x18 time candidate   : %u (0x%08X)", ReadLe32Ui(snap.rawCharSkillbar, 0x18), ReadLe32Ui(snap.rawCharSkillbar, 0x18));
            ImGui::Text("0x24 time candidate   : %u (0x%08X)", ReadLe32Ui(snap.rawCharSkillbar, 0x24), ReadLe32Ui(snap.rawCharSkillbar, 0x24));
            ImGui::Text("0x08 ptr candidate    : 0x%016llX", static_cast<unsigned long long>(ReadLe64Ui(snap.rawCharSkillbar, 0x08)));
            ImGui::Text("0x48 ptr candidate    : 0x%016llX", static_cast<unsigned long long>(ReadLe64Ui(snap.rawCharSkillbar, 0x48)));
            ImGui::Text("0x60 ptr candidate    : 0x%016llX", static_cast<unsigned long long>(ReadLe64Ui(snap.rawCharSkillbar, 0x60)));
            ImGui::Text("0x40 ref pointer      : 0x%016llX", static_cast<unsigned long long>(ReadLe64Ui(snap.rawCharSkillbar, 0x40)));
        }

        if (ImGui::CollapsingHeader("Skill ID Candidates")) {
            ImGui::TextWrapped("These slot candidates are not yet confirmed to be the real skill-slot layout. We are showing them separately so we can see whether one slot changes when a specific skill is used.");
            RenderSlotCandidateTable(snap);
        }

        if (ImGui::CollapsingHeader("Primary Skill Objects")) {
            const std::string ptr48Monotonic = BuildMonotonicCandidateText(s_samples, 48);
            const std::string ptr60Monotonic = BuildMonotonicCandidateText(s_samples, 60);
            RenderPrimaryPointerPanel("ptr48", ReadLe64Ui(snap.rawCharSkillbar, 0x48), snap.rawPtr48, sizeof(snap.rawPtr48), ptr48Monotonic);
            ImGui::Separator();
            RenderPrimaryPointerPanel("ptr60", ReadLe64Ui(snap.rawCharSkillbar, 0x60), snap.rawPtr60, sizeof(snap.rawPtr60), ptr60Monotonic);
            ImGui::Separator();
            RenderPointerObjectSummary("ptr40 (reference)", ReadLe64Ui(snap.rawCharSkillbar, 0x40), snap.rawPtr40, sizeof(snap.rawPtr40));
        }

        if (ImGui::CollapsingHeader("CMSG Use Skill")) {
            ImGui::TextWrapped("This is the most recent outgoing buffer region whose header matched CMSG_USE_SKILL (0x0017). It is stored as raw bytes first so we can correlate payload candidates before committing to a parser.");
            ImGui::InputTextMultiline("##UseSkillPacketHex",
                                      s_lastUseSkillHex.data(),
                                      s_lastUseSkillHex.size() + 1,
                                      ImVec2(-1, ImGui::GetTextLineHeight() * 5),
                                      ImGuiInputTextFlags_ReadOnly);
            ImGui::InputTextMultiline("##UseSkillPacketFields",
                                      s_lastUseSkillFields.data(),
                                      s_lastUseSkillFields.size() + 1,
                                      ImVec2(-1, ImGui::GetTextLineHeight() * 12),
                                      ImGuiInputTextFlags_ReadOnly);
        }

        if (ImGui::CollapsingHeader("UseSkill Mapping Tables")) {
            ImGui::TextUnformatted("Primary samples (tail_tag == 0x29):");
            ImGui::InputTextMultiline("##PrimaryUseSkillSamples",
                                      s_primaryUseSkillSamples.data(),
                                      s_primaryUseSkillSamples.size() + 1,
                                      ImVec2(-1, ImGui::GetTextLineHeight() * 6),
                                      ImGuiInputTextFlags_ReadOnly);
            ImGui::Separator();
            ImGui::TextUnformatted("Secondary samples (tail_tag == 0x25):");
            ImGui::InputTextMultiline("##SecondaryUseSkillSamples",
                                      s_secondaryUseSkillSamples.data(),
                                      s_secondaryUseSkillSamples.size() + 1,
                                      ImVec2(-1, ImGui::GetTextLineHeight() * 6),
                                      ImGuiInputTextFlags_ReadOnly);
            ImGui::Separator();
            ImGui::Text("CharSkillbar Raw Dump (0x188 bytes):");
            for (int row = 0; row < 0x188/16; row++) {
                char line[128];
                int  off = row * 16;
                snprintf(line, sizeof(line),
                    "%03X: %02X %02X %02X %02X  %02X %02X %02X %02X  "
                         "%02X %02X %02X %02X  %02X %02X %02X %02X",
                    off,
                    snap.rawCharSkillbar[off+0],  snap.rawCharSkillbar[off+1],
                    snap.rawCharSkillbar[off+2],  snap.rawCharSkillbar[off+3],
                    snap.rawCharSkillbar[off+4],  snap.rawCharSkillbar[off+5],
                    snap.rawCharSkillbar[off+6],  snap.rawCharSkillbar[off+7],
                    snap.rawCharSkillbar[off+8],  snap.rawCharSkillbar[off+9],
                    snap.rawCharSkillbar[off+10], snap.rawCharSkillbar[off+11],
                    snap.rawCharSkillbar[off+12], snap.rawCharSkillbar[off+13],
                    snap.rawCharSkillbar[off+14], snap.rawCharSkillbar[off+15]);
                ImGui::TextUnformatted(line);
            }
        }
    }
}
