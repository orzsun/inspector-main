#pragma once

/**
 * @file PacketProcessor.h
 * @brief Defines the interface for processing captured packet data.
 * @details This layer separates packet analysis, decryption, and logging
 *          from the raw function hooking mechanism.
 */

#include <cstddef>        // For std::size_t
#include <cstdint>        // For std::uint8_t
#include <optional>       // For std::optional
#include "GameStructs.h"   // For kx::GameStructs::MsgSendContext, RC4State
#include "PacketData.h"    // For kx::PacketDirection (potentially useful here later)

namespace kx::PacketProcessing {

    /**
     * @brief Processes data captured from an outgoing packet event (MsgSend).
     * @param context Pointer to the game's MsgSendContext structure containing
     *                buffer state and pointers relevant to the outgoing packet.
     *                Expected to be non-null by the caller (hook).
     */
    void ProcessOutgoingPacket(const GameStructs::MsgSendContext* context);

    /**
    * @brief Processes an individual, decrypted, framed game message.
    * @details This function is intended to be called by the MsgDispatch hook
    *          for each message identified within the processed network stream.
    *
    * @param direction The direction of the message (should be Received).
    * @param messageId The 2-byte header/opcode of the message.
    * @param messageData Pointer to the start of the message's data payload (after the header).
    * @param messageSize The size of the message's data payload.
    * @param pMsgConn Optional: Pointer to the MsgConn context, if available/needed.
    */
    void ProcessDispatchedMessage(
        kx::PacketDirection direction, // Should always be Received here
        uint16_t messageId,
        const uint8_t* messageData,
        size_t messageSize,
        void* pMsgConn = nullptr // Pass context if needed later
    );

} // namespace kx::PacketProcessing