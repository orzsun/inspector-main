# Hooking Implementation Details

This document details the practical implementation of the packet hooking mechanism used by the KX Packet Inspector. It covers the target functions, data structures, and specific strategies used to capture network messages.

## Hooking Strategy

### Outgoing (CMSG)

*   **Method:** MinHook entry point hook on the `MsgConn::FlushPacketBuffer` function.
*   **Data:** Reads from a `MsgSendContext` structure to get the plaintext buffer before potential encryption.
*   **Status:** Stable and functional.

### Incoming (SMSG)

*   **Method:** SafetyHook mid-function hook (`MidHook`) placed at multiple sites inside the main message dispatcher function, `Msg::DispatchStream`.
*   **Target:** The hooks are placed immediately before the instructions that prepare for the final `call rax` dynamic dispatch. This ensures that all necessary context (registers and stack variables) has been set up by the dispatcher, regardless of which internal code path is taken.
*   **Data Extraction:** The hook uses the `SafetyHookContext` to read registers and memory, following a precise pointer chain to extract all necessary packet information. This method is highly reliable as it is based on the game's own logic.
*   **Status:** Functional and captures all generic-path packets. Fast Path packets are bypassed by this method and require separate, targeted hooks.

## The SMSG Data Extraction "Recipe"

This is the definitive, multi-step process used within the C++ hook to extract full packet details at the moment of dispatch.

1.  **Get `pMsgConn` from `RBX`:** The `RBX` register reliably holds a pointer to the `MsgConn` context object throughout the `Msg::DispatchStream` function.
2.  **Get `handlerInfoPtr`:** The pointer to the current `HandlerInfo` structure is located at `[RBX + 0x48]`.
3.  **Read from `HandlerInfo`:** With the `handlerInfoPtr`, the hook reads all critical metadata from fixed offsets:
    *   **Opcode:** `uint16_t` at `[handlerInfoPtr + 0x00]`
    *   **Schema Pointer (`msgDefPtr`):** `void*` at `[handlerInfoPtr + 0x08]`
    *   **Handler Function Pointer:** `void*` at `[handlerInfoPtr + 0x18]`
4.  **Get `messageSize`:** The hook follows the `msgDefPtr` (schema pointer) and reads the `uint32_t` size from `[msgDefPtr + 0x20]`.
5.  **Get `messageDataPtr`:** The pointer to the parsed packet payload is retrieved from a stack location relative to the base pointer: `[RBP - 0x18]`.

This complete chain provides the opcode, handler address, schema address, payload size, and a pointer to the payload itself.

## Target Functions

Function signatures are provided to help locate these functions after a game update.

### Outgoing Packets (`MsgConn::FlushPacketBuffer`)

*   **Purpose:** Prepares and sends the buffer of outgoing CMSG packets.
*   **Signature:** `40 ? 48 83 EC ? 48 8D ? ? ? 48 89 ? ? 48 89 ? ? 48 89 ? ? 4C 89 ? ? 48 8B ? ? ? ? ? 48 33 ? 48 89 ? ? 48 8B ? E8`

### Incoming Packets (`Msg::DispatchStream`)

*   **Internal Name:** `Msg::DispatchStream`
*   **Purpose:** Processes a buffer of one or more decrypted, framed messages and calls the appropriate handler for each. This is the primary target for SMSG hooking.
*   **Signature:** `48 89 5C 24 ? 4C 89 44 24 ? 55 56 57 41 54 41 55 41 56 41 57 48 8B EC 48 83 EC ? 8B 82`

### Obsolete Target (MsgRecv - Low Level)

*   **Signature:** `40 55 41 54 41 55 41 56 41 57 48 83 EC ? 48 8D 6C 24 ? 48 89 5D ? 48 89 75 ? 48 89 7D ? 48 8B 05 ? ? ? ? 48 33 C5 48 89 45 ? 44 0F B6 12`
*   **Reason for Deprecation:** Hooking this function was abandoned because it operates on the raw, encrypted message buffer. This required performing RC4 decryption manually and did not provide cleanly framed messages. The current approach of hooking the `Msg::DispatchStream` function is far more effective as it provides access to individual, plaintext messages after decryption and framing have already been handled by the game client.

## Key Data Structures & Offsets

These are the definitive offsets used by the KX Packet Inspector.

### `MsgConn` Context (Relative to `RBX` in `Msg::DispatchStream`)
*   `+0x48`: `void** ppHandlerInfo` (Pointer to a pointer to the current `HandlerInfo` structure)

### `HandlerInfo` Structure (Relative to `ppHandlerInfo`)
This structure is the central hub for dispatching a single message.
*   `+0x00`: `uint16_t messageId` (The packet's opcode)
*   `+0x08`: `void** ppMsgDef` (Pointer to a pointer to the `MessageDefinition` structure, which is the schema)
*   `+0x10`: `int32_t dispatchType` (Determines the call path, e.g., 0 for Generic, 1 for Fast Path)
*   `+0x18`: `void* handlerFuncPtr` (Pointer to the C++ handler function)

### `MessageDefinition` Structure (Schema - Relative to `ppMsgDef`)
*   `+0x20`: `uint32_t messageSize` (The size of the message payload)

## Toolchain

*   **Hooking:** MinHook (for entry-point hooks), SafetyHook (for mid-function hooks)
*   **Reverse Engineering:** Ghidra, IDA Pro
*   **Memory Analysis:** Cheat Engine, ReClass.NET
*   **Debugging:** Visual Studio Debugger