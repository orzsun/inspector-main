#include "ParseSkillUpdatePacket.h"

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

std::optional<std::string> ParseSkillUpdatePacket(const kx::PacketInfo& packet) {
    if (packet.direction != kx::PacketDirection::Received ||
        packet.rawHeaderId != static_cast<uint16_t>(kx::SMSG_HeaderId::SKILL_UPDATE)) {
        return std::nullopt;
    }

    std::ostringstream oss;
    oss << "[0x0017] SkillUpdate\n";
    oss << "length: " << packet.data.size() << " bytes\n";
    if (!packet.data.empty()) {
        oss << "length_mod_2: " << (packet.data.size() % 2) << '\n';
        oss << "length_mod_4: " << (packet.data.size() % 4) << '\n';
    }

    for (size_t offset = 0; offset + 2 <= packet.data.size(); offset += 2) {
        const uint16_t value = ReadLe16(packet.data, offset);
        oss << "u16 @ 0x" << std::hex << std::setw(2) << std::setfill('0') << offset
            << std::dec << ": " << value
            << " (0x" << std::hex << std::setw(4) << std::setfill('0') << value << std::dec << ")\n";
    }

    oss << "aligned u32:\n";
    for (size_t offset = 0; offset + 4 <= packet.data.size(); offset += 4) {
        const uint32_t value = ReadLe32(packet.data, offset);
        oss << "  u32 @ 0x" << std::hex << std::setw(2) << std::setfill('0') << offset
            << std::dec << ": " << value
            << " (0x" << std::hex << std::setw(8) << std::setfill('0') << value << std::dec << ")\n";
    }

    oss << "raw bytes:\n" << BuildRawHexRows(packet.data);
    return oss.str();
}

}
