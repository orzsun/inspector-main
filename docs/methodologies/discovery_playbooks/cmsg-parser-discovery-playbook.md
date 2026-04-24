# CMSG Packet Discovery Playbook

**Status:** Confirmed & Validated Master Workflow

## Objective

This document provides the definitive, repeatable method for discovering and validating any Client-to-Server (CMSG) packet structure. This workflow is a complete, end-to-end process that is resilient to game updates, starting with finding necessary metadata and proceeding to live traffic analysis.

## Core Principle: The Hybrid Model

The client uses a sophisticated hybrid model for building outgoing packets, combining high-level logic with a generic serialization engine. Understanding this is key to successful analysis.

1.  **High-Level "Data Aggregators":** For each major player action (moving, jumping, using a skill), there is a dedicated high-level function (e.g., in the `Gw2::Engine::Agent::AgCommand` family). This function's job is to gather all necessary game state and package it into a temporary data structure.
2.  **Generic Schema-Driven Serialization:** The aggregator function then calls a generic builder, which in turn uses a master schema table and the `Msg::MsgPack` engine to serialize the temporary data structure into the final packet bytes.
3.  **"Fast Path" Optimization:** For high-frequency packets, the aggregator may only provide a *minimal subset* of the data defined in the full schema. The serialization engine intelligently skips the unprovided optional fields, resulting in a small, efficient packet on the wire.

**Live traffic is the only source of truth** for determining the final on-wire structure of a packet.

---

## Part 1: Prerequisite - Finding Core Structures & Metadata

These addresses can change with game updates. The first step is always to find their new locations.

### Step 1.1: Find the Schema Table Address (Dynamic Analysis)

1.  **Find `MsgConn` Pointer:** The global pointer to the game connection object is at the static address `DAT_142628290`. In a memory debugger (like Cheat Engine), find the 8-byte pointer value stored at "Gw2-64.exe"+2628290. This is the live address of the `MsgConn` object.
2.  **Find `SchemaTableInfo` Pointer:** The pointer to the structure containing table information is at offset `+0x18` from the `MsgConn` object. Read the 8-byte pointer at `[MsgConn Address + 0x18]`.
3.  **Find Table Base and Size:** From the `SchemaTableInfo` address, read the following:
    *   **Table Base:** The 8-byte pointer at offset `+0x50`.
    *   **Table Size:** The 4-byte integer at offset `+0x5c`.

### Step 1.2: Dump the Opcode-to-Schema Table

With the live base address and size, use the provided LUA script (`../../tools/cheat-engine/CE_CMSG_SchemaDumper.lua`) to print the complete `Opcode -> Schema Address` mapping.

---

## Part 2: The Universal CMSG Discovery Workflow

### Step 2.1: Find a Low-Level Write Primitive

1.  In your debugger, use a logging feature (e.g., Cheat Engine's "Find out what writes to this address") on the main `MsgSendContext` buffer. This buffer is located at offset `+0x398` from the base `MsgSendContext` object address.
    *(Tip: Find the `MsgSendContext` address by breaking on `MsgConn::FlushPacketBuffer` once and reading the `RCX` register.)*
2.  Perform various in-game actions (move, jump, use skills).
3.  Analyze the log. You will find that a small number of instructions are responsible for most of the writes. Identify a common, generic "write primitive" function, such as one that performs a 2-byte write (e.g., `mov [rcx], ax`). **Record the address of this instruction.** It will be our initial hook point.

### Step 2.2: Isolate the Builder with a Conditional Breakpoint

This is the most critical step. We will pause the game *only* when the specific opcode we're interested in is being written.

1.  **Set a Breakpoint:** In your debugger, go to the address of the low-level write primitive you just found. Set a normal breakpoint.
2.  **Set the Condition:** Right-click the breakpoint and add a condition. The condition must check the value of the register being written. For a 2-byte opcode written from the `AX` register, the formula is:
    ```lua
    (RAX & 0xFFFF) == 0x0017
    ```
    *(Replace `0x0017` with the hex value of the opcode you are hunting. Note that the register name `RAX` must be uppercase and the hex value must start with `0x`.)*
3.  **Trigger and Break:** Perform the in-game action. The debugger will now pause *only* when your target opcode is being written.

### Step 2.3: Find the High-Level Aggregator in the Call Stack

When the conditional breakpoint hits, examine the debugger's **Call Stack**.

*   **Interpreting the Call Stack:** You are paused deep inside the serialization engine. You must walk up the stack, past the low-level serialization functions, until you find a function that is clearly part of the higher-level game logic (e.g., `Gw2::Engine::Agent::AgCommand`). This function is the **true data aggregator** that initiates the packet creation process. Its address will typically be in a different memory range than the core serialization functions.

    *Example Call Stack:*
    ```
    ...FD2DAE      <-- You are here (Low-level write)
    ...FD2A1A      <-- Serialization Engine
    ...1021B1F     <-- Serialization Engine
    ...10225DE     <-- Generic Builder
    ...1016919     <-- **THIS IS THE HIGH-LEVEL AGGREGATOR**
    ```

### Step 2.4: Statically Analyze the Full Call Chain

Navigate to the aggregator function's address in Ghidra and analyze its code, following the chain of calls down to the serialization engine. This will reveal the complete construction process.

### Step 2.5: Document the Verified Structure

Create or update the markdown file for the packet.

*   Describe the full construction chain, from the high-level aggregator down to the serialization call.
*   Provide the confirmed on-wire packet structure, validated against live captures.
*   Link to the key decompiled functions as evidence.

---

## Future Improvements: Automating Builder Discovery

The manual workflow described in this document is reliable but time-consuming, requiring a separate debugging session for each opcode. Based on the discovery of the unified `CMSG::BuildAndSendPacket` function, a more advanced, automated approach is possible.

### TODO: Implement a "Builder Discovery" Hook

The next evolution of the KX Packet Inspector tool should be to automate the process of mapping opcodes to their high-level aggregator functions.

**Proposed Method:**

1.  **Target Function:** Instead of hooking a low-level write primitive, the tool should hook the central serialization function, **`CMSG::BuildAndSendPacket`**.
2.  **Hook Logic (Detour):** The detour function for this hook would perform the following actions:
    *   **Extract Opcode:** Read the first two bytes from the packet data buffer passed into the function to get the `opcode`.
    *   **Get Caller Address:** Use a compiler intrinsic (like `_ReturnAddress()` in MSVC) to get the memory address of the function that *called* `CMSG::BuildAndSendPacket`. This address points directly to the generic builder (`FUN_141018E00`). To find the *aggregator*, the tool would need to walk the call stack one or two levels higher.
    *   **Log the Mapping:** Write the `[Opcode -> Aggregator Address]` pair to a dedicated log file.

**Pseudo-Code Example:**
```cpp
// Detour for the CMSG::BuildAndSendPacket function
void Detour_CMSG_BuildAndSendPacket(byte* pMsgConnContext, uint size, ushort* pPacketData) {
    
    // 1. Get the Opcode from the packet data.
    uint16_t opcode = *pPacketData;

    // 2. Get the address of the high-level aggregator from the call stack.
    // This requires a stack walking utility.
    void* aggregatorAddress = GetCallerAddressFromStack(2); // Example: walk up 2 frames

    // 3. Log the discovered relationship.
    LogToFile("[Builder Discovery] Opcode: 0x%04X -> Aggregator: %p\n", opcode, aggregatorAddress);

    // 4. Call the original function to maintain game functionality.
    original_CMSG_BuildAndSendPacket(pMsgConnContext, size, pPacketData);
}
```

**Benefit:**

By implementing this, the discovery process would change from a manual, one-by-one hunt into a bulk data collection task. A user could simply play the game for a few minutes, performing various actions, and the tool would automatically generate a near-complete map of which high-level functions are responsible for building which CMSG packets. This would accelerate the documentation of the remaining CMSG protocol exponentially.

