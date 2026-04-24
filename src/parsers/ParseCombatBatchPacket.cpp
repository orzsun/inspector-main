#include "ParseCombatBatchPacket.h"
#include "../PacketHeaders.h"
#include <sstream>
#include <iomanip>

namespace kx::Parsing {
    std::optional<std::string> ParseCombatBatchPacket(const kx::PacketInfo& packet) {
        // This single parser can handle both opcodes
        if (packet.rawHeaderId != 0x00DB && packet.rawHeaderId != 0x0032) {
            return std::nullopt;
        }

        std::stringstream ss;
        ss << "Container Packet (Opcode: 0x" << std::hex << packet.rawHeaderId << ")\n";
        ss << "Contains the following sub-packets:\n";

        // Simple loop to find opcodes (often start with 0x17 for skill use)
        // This logic can be refined, but it's a great start.
        size_t offset = 4; // Skip the container's own header
        while (offset + 2 <= packet.data.size()) {
            uint16_t sub_opcode;
            std::memcpy(&sub_opcode, packet.data.data() + offset, sizeof(uint16_t));

            // Heuristic: Check if the opcode is a known CMSG type to reduce noise.
            // A more robust way is to parse the container's structure, but this is fast.
            if (kx::g_cmsgNames.count(static_cast<kx::CMSG_HeaderId>(sub_opcode))) {
                ss << "  - " << kx::GetPacketName(kx::PacketDirection::Sent, sub_opcode)
                    << " (0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << sub_opcode << ")"
                    << " at offset 0x" << std::hex << offset << "\n";
            }
            // A common pattern is that sub-packets are separated by a size or sequence field.
            // Let's assume we skip a few bytes to find the next one. This needs tweaking based on live data.
            offset += 26; // Heuristic jump, adjust based on your logs
        }

        return ss.str();
    }
}