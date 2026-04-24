#pragma once

#include <cstddef> // For std::byte, std::size_t
#include <cstdint> // For std::uint8_t, std::int32_t
#include <type_traits> // For offsetof (used in static_assert)
#include <array>   // For std::array

namespace kx::GameStructs {

    /**
     * @brief Represents the memory layout of the context object for the outgoing message buffer.
     * @details This structure defines members based on reverse engineering the main CMSG
     *          buffering function, `MsgConn::FlushPacketBuffer`. It allows accessing
     *          packet data before it is encrypted and sent.
     * @warning This structure definition is based on analysis of specific versions
     *          of the game client. Game updates could alter this layout.
     */
    struct MsgSendContext {
        // --- Member offsets relative to the start of the structure ---

        std::byte padding_0x0[0xD0];

        /**
         * @brief 0xD0: Pointer to the current write position (end) within the packet data buffer.
         * @details The total size of all queued packet data is (currentBufferEndPtr - GetPacketBufferStart()).
         */
        std::uint8_t* currentBufferEndPtr;

        std::byte padding_0xD8[0x108 - 0xD8];

        /**
         * @brief 0x108: State variable influencing control flow within the send function.
         * @details State '3' is the normal state for sending a buffer of gameplay packets.
         */
        std::int32_t bufferState;

        // --- Constants Related to the Context ---

        /**
         * @brief The fixed memory offset from the base of the MsgSendContext instance
         *        to the start of the actual raw packet data buffer.
         */
        static constexpr std::size_t PACKET_BUFFER_OFFSET = 0x398;

        // --- Helper Methods ---

        /**
         * @brief Calculates the pointer to the start of the raw packet data buffer.
         * @return Pointer to the beginning of the packet data.
         */
        std::uint8_t* GetPacketBufferStart() const noexcept {
            return reinterpret_cast<std::uint8_t*>(
                const_cast<MsgSendContext*>(this)
                ) + PACKET_BUFFER_OFFSET;
        }

        /**
         * @brief Calculates the size of the packet data currently present in the buffer.
         * @return The size in bytes.
         */
        std::size_t GetCurrentDataSize() const noexcept {
            const std::uint8_t* bufferStart = GetPacketBufferStart();
            if (currentBufferEndPtr == nullptr || currentBufferEndPtr < bufferStart) {
                return 0;
            }
            return static_cast<std::size_t>(currentBufferEndPtr - bufferStart);
        }
    };

    // Static assertions to verify expected offsets at compile time.
    static_assert(offsetof(MsgSendContext, currentBufferEndPtr) == 0xD0, "Offset mismatch for MsgSendContext::currentBufferEndPtr");
    static_assert(offsetof(MsgSendContext, bufferState) == 0x108, "Offset mismatch for MsgSendContext::bufferState");



    // --- MsgConn Context Offsets (SMSG Dispatch) ---
    // These offsets are relative to the 'pMsgConn' pointer (RBX register) passed to
    // the main SMSG dispatcher function, `Msg::DispatchStream`.

    /**
     * @brief Offset to a pointer to the current message's `HandlerInfo` structure. (void**)
     * @details This is the most critical pointer for discovery. It points to the structure
     *          that contains the opcode, schema pointer, and handler function pointer.
     */
    inline constexpr std::ptrdiff_t MSGCONN_HANDLER_INFO_PTR_OFFSET = 0x48;

    /**
     * @brief Offset to the base pointer of the network ring buffer for raw received data. (byte**)
     */
    inline constexpr std::ptrdiff_t MSGCONN_BUFFER_BASE_PTR_OFFSET = 0x88;

    /**
     * @brief Offset to the current read index within the network ring buffer. (uint32_t)
     */
    inline constexpr std::ptrdiff_t MSGCONN_BUFFER_READ_IDX_OFFSET = 0x94;

    /**
     * @brief Offset to the current write index within the network ring buffer. (uint32_t)
     */
    inline constexpr std::ptrdiff_t MSGCONN_BUFFER_WRITE_IDX_OFFSET = 0xA0;

    /**
     * @brief Offset to the base pointer of the workspace buffer where processed
     *        (e.g., decrypted) packet data is held for dispatch. (void**)
     */
    inline constexpr std::ptrdiff_t MSGCONN_PROCESSED_BUFFER_BASE_OFFSET = 0xC8; // 200 decimal


    // --- HandlerInfo Structure Offsets ---
    // These offsets are relative to the 'handlerInfoPtr' obtained from `[pMsgConn + 0x48]`.
    // This structure acts as the central hub for dispatching a single message.

    /**
     * @brief Offset to the Message ID (Opcode). (uint16_t)
     */
    inline constexpr std::ptrdiff_t HANDLER_INFO_MSG_ID_OFFSET = 0x00;

    /**
     * @brief Offset to the Message Definition Pointer, which is the Schema. (void**)
     */
    inline constexpr std::ptrdiff_t HANDLER_INFO_MSG_DEF_PTR_OFFSET = 0x08;

    /**
     * @brief Offset to the Dispatch Type field, used to select a handler path. (int32_t)
     * @details e.g., 0 for Generic Path, 1 for Fast Path.
     */
    inline constexpr std::ptrdiff_t HANDLER_INFO_DISPATCH_TYPE_OFFSET = 0x10;

    /**
     * @brief Offset to the Handler Function Pointer. (void**)
     * @details This is the address of the C++ function that will be executed by `call rax`.
     */
    inline constexpr std::ptrdiff_t HANDLER_INFO_HANDLER_FUNC_PTR_OFFSET = 0x18;


    // --- MessageDefinition (Schema) Structure Offsets ---
    // These offsets are relative to the 'msgDefPtr' (Schema Pointer) obtained from the HandlerInfo struct.

    /**
     * @brief Offset within the Message Definition struct to the size of the message payload. (uint32_t)
     */
    inline constexpr std::ptrdiff_t MSG_DEF_SIZE_OFFSET = 0x20;
}