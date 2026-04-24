#include "ParseAgentAttributeUpdatePacket.h"

#include "../PacketHeaders.h"

#include <iomanip>
#include <sstream>

namespace kx::Parsing {
namespace {

uint16_t ReadLe16(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<uint16_t>(data[offset]) |
           (static_cast<uint16_t>(data[offset + 1]) << 8);
}

uint32_t ReadLe32(const std::vector<uint8_t>& data, size_t offset) {
    return static_cast<uint32_t>(data[offset]) |
           (static_cast<uint32_t>(data[offset + 1]) << 8) |
           (static_cast<uint32_t>(data[offset + 2]) << 16) |
           (static_cast<uint32_t>(data[offset + 3]) << 24);
}

std::string BuildRawHexRows(const std::vector<uint8_t>& data) {
    std::ostringstream oss;
    for (size_t row = 0; row < data.size(); row += 16) {
        oss << "  ";
        oss << std::hex << std::setw(4) << std::setfill('0') << row << ": ";
        const size_t rowSize = std::min<size_t>(16, data.size() - row);
        for (size_t i = 0; i < rowSize; ++i) {
            oss << std::setw(2) << static_cast<unsigned>(data[row + i]);
            if (i + 1 != rowSize) {
                oss << ' ';
            }
        }
        oss << std::dec << '\n';
    }
    return oss.str();
}

std::string BuildSequentialFieldRows(const std::vector<uint8_t>& data, const char* title) {
    std::ostringstream oss;
    oss << title << '\n';
    if (data.size() >= 2) {
        oss << "  u16 @ 0x00: " << ReadLe16(data, 0)
            << " (0x" << std::hex << std::setw(4) << std::setfill('0') << ReadLe16(data, 0) << std::dec << ")"
            << "  // agent_id candidate\n";
    }
    for (size_t offset = 2; offset + 4 <= data.size(); offset += 4) {
        const uint32_t value = ReadLe32(data, offset);
        const unsigned fieldIndex = static_cast<unsigned>((offset - 2) / 4);
        oss << "  u32 @ 0x" << std::hex << std::setw(2) << std::setfill('0') << offset
            << std::dec << ": " << value
            << " (0x" << std::hex << std::setw(8) << std::setfill('0') << value << std::dec << ")";
        if (fieldIndex == 0) {
            oss << "  // field_A";
        } else if (fieldIndex == 1) {
            oss << "  // field_B";
        }
        oss << '\n';
    }
    return oss.str();
}

}

std::optional<std::string> ParseAgentAttributeUpdatePacket(const kx::PacketInfo& packet) {
    if (packet.direction != kx::PacketDirection::Received ||
        packet.rawHeaderId != static_cast<uint16_t>(kx::SMSG_HeaderId::AGENT_ATTRIBUTE_UPDATE)) {
        return std::nullopt;
    }

    std::ostringstream oss;
    oss << "[0x0028] AgentAttributeUpdate\n";
    oss << "length: " << packet.data.size() << " bytes\n";
    oss << "raw bytes:\n" << BuildRawHexRows(packet.data);
    oss << BuildSequentialFieldRows(packet.data, "sequential fields:");
    return oss.str();
}

}
