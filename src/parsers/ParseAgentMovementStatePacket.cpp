#include "ParseAgentMovementStatePacket.h"
#include "../PacketHeaders.h"
#include <cstring>   // For memcpy
#include <sstream>   // For string building
#include <iomanip>   // For hex formatting

namespace kx::Parsing {

    std::optional<std::string> ParseAgentMovementStatePacket(const kx::PacketInfo& packet) {
        // 1. Ensure this is the correct packet we want to parse.
        if (packet.direction != kx::PacketDirection::Received ||
            packet.rawHeaderId != static_cast<uint16_t>(kx::SMSG_HeaderId::AGENT_MOVEMENT_STATE_CHANGE)) {
            return std::nullopt;
        }

        // 2. Ensure the packet has at least enough data for the subtype ID.
        if (packet.data.size() < 2) {
            return "Agent Movement State: (Malformed, too small for subtype)";
        }

        std::stringstream ss;
        uint16_t subtype;
        std::memcpy(&subtype, packet.data.data(), sizeof(uint16_t));

        // 3. Handle the "END State" variant (e.g., stopping movement).
        if (subtype == 0x03CC) {
            if (packet.data.size() < 7) {
                return "End Movement State: (Malformed, expected 7 bytes)";
            }

            uint32_t agentId;
            std::memcpy(&agentId, packet.data.data() + 2, sizeof(uint32_t));

            ss << "Agent Movement State: END\n"
                << "  Purpose: Agent stops a dynamic movement (e.g., running).\n"
                << "  Subtype: 0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << subtype << "\n"
                << "  Agent ID: 0x" << std::hex << std::uppercase << agentId << " (" << std::dec << agentId << ")";

            return ss.str();
        }
        // 4. Handle the "APPLY State" variant (e.g., starting movement).
        else if (subtype == 0x03C6) {
            if (packet.data.size() < 7) {
                return "Apply Movement State: (Malformed, expected >7 bytes)";
            }

            uint32_t agentId;
            std::memcpy(&agentId, packet.data.data() + 2, sizeof(uint32_t));

            ss << "Agent Movement State: APPLY\n"
                << "  Purpose: Agent starts a new dynamic movement (e.g., running).\n"
                << "  Subtype: 0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << subtype << "\n"
                << "  Agent ID: 0x" << std::hex << std::uppercase << agentId << " (" << std::dec << agentId << ")\n"
                << "  Animation & Physics Data Size: " << std::dec << (packet.data.size() - 7) << " bytes";

            return ss.str();
        }

        // 5. Fallback for any other subtype found under this opcode.
        ss << "Agent Movement State (Unknown Subtype):\n"
            << "  Subtype: 0x" << std::hex << std::uppercase << std::setw(4) << std::setfill('0') << subtype;
        return ss.str();
    }

} // namespace kx::Parsing