#pragma once

#include <cstdint> // For uint16_t, uint32_t

namespace kx::Packets {

    // Basic structure for movement packet payload (X, Y, Z coordinates)
    // Based on analysis suggesting these are read from offset size - 16.
    struct MovementPayload {
        float x;
        float y;
        float z;
    };

    #pragma pack(push, 1)

    // SMSG_PLAYER_STATE_UPDATE (0x0002) - 11 bytes
    struct SMSG_PlayerStateUpdatePayload {
        uint16_t hdr;         // observed 0x02E7
        uint8_t  mode_or_ix;  // small mode/index
        uint16_t tick_lo;     // low word of a cadence counter or time-based index
        uint16_t millis_or_k; // observed 0x03E8 (1000)
        uint16_t world_or_id; // observed 0x022A
        uint16_t flags;       // observed 0x0000
    };

    // SMSG_TIME_SYNC (0x003F) - Variant A: Cadence/Tick (10 bytes)
    struct SMSG_TimeSyncTickPayload {
        uint16_t type;        // e.g., 0x050F
        uint32_t time_lo;     // monotonic/cadenced low part
        uint16_t time_hi;     // high part/segment
        uint16_t flags_or_id; // small ID or flags (e.g., 0x0039/0x0038/0x0000)
    };

    // SMSG_TIME_SYNC (0x003F) - Variant B: Seed/Epoch (10 bytes)
    struct SMSG_TimeSyncSeedPayload {
        uint16_t type;        // 0x050D
        uint16_t seed;        // 0x8A4A
        uint16_t millis;      // 0x03E8 (1000)
        uint16_t world_or_id; // 0x022A
        uint16_t flags;       // 0x0000
    };

    // CMSG_AGENT_LINK (0x0036) - 6 bytes (excluding opcode)
    struct CMSG_AgentLinkPayload {
        uint16_t agentId;
        uint16_t parentId;
        uint16_t flags;
    };

    // CMSG_SELECT_AGENT (0x00E5) - 8 bytes
    struct CMSG_SelectAgentPayload {
        uint16_t opcode;
        uint16_t agentId;
        uint16_t unknown;
        uint16_t agentId_repeat;
    };

    #pragma pack(pop)

    // Add definitions for other packet payloads here as they are identified.

} // namespace kx::Packets