#pragma once

#include "../PacketData.h"
#include <optional>
#include <string>

namespace kx::Parsing {

    /**
     * @brief Parses the two variants of the AGENT_MOVEMENT_STATE_CHANGE packet (SMSG 0x001C).
     * @details This packet is responsible for synchronizing agent animation and movement states.
     *          It has a small variant for ending a state (e.g., stopping movement) and a large
     *          variant for applying a new state (e.g., starting to run) which includes detailed
     *          animation and physics parameters.
     * @param packet The PacketInfo object to parse.
     * @return A formatted string describing the packet's contents, or std::nullopt if it's not a valid 0x001C packet.
     */
    std::optional<std::string> ParseAgentMovementStatePacket(const kx::PacketInfo& packet);

} // namespace kx::Parsing