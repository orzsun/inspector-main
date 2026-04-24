#include "ParseAgentStateBulkPacket.h"

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

}

std::optional<std::string> ParseAgentStateBulkPacket(const kx::PacketInfo& packet) {
    if (packet.direction != kx::PacketDirection::Received ||
        packet.rawHeaderId != static_cast<uint16_t>(kx::SMSG_HeaderId::AGENT_STATE_BULK)) {
        return std::nullopt;
    }

    std::ostringstream oss;
    oss << "[0x0016] AgentStateBulk\n";
    oss << "length: " << packet.data.size() << " bytes\n";
    if (!packet.data.empty()) {
        oss << "length_mod_4: " << (packet.data.size() % 4) << "\n";
    }
    if (packet.data.size() >= 2) {
        oss << "u16 @ 0x00: " << ReadLe16(packet.data, 0)
            << " (0x" << std::hex << std::setw(4) << std::setfill('0') << ReadLe16(packet.data, 0) << std::dec << ")\n";
    }
    for (size_t offset = 2; offset + 4 <= packet.data.size(); offset += 4) {
        const uint32_t value = ReadLe32(packet.data, offset);
        oss << "u32 @ 0x" << std::hex << std::setw(2) << std::setfill('0') << offset
            << std::dec << ": " << value
            << " (0x" << std::hex << std::setw(8) << std::setfill('0') << value << std::dec << ")\n";
    }
    oss << "raw bytes:\n" << BuildRawHexRows(packet.data);
    return oss.str();
}

}
