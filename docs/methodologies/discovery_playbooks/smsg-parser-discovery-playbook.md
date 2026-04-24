# SMSG Packet Discovery Playbook (Final Workflow)

**Status:** Confirmed & Finalized

## Objective

This document provides the definitive, repeatable method for discovering and analyzing any Server-to-Client (SMSG) packet structure. The workflow leverages the live C++ hook within **KXPacketInspector** for initial discovery and takes advantage of a key architectural pattern—the collocation of schema and handler pointers—to rapidly perform static analysis.

## Core Architectural Concepts

A full understanding of the client's incoming message handling requires recognizing these key patterns:

*   **Dynamic Dispatch:** The client uses a `call rax` instruction to dynamically dispatch most messages to different handler functions at runtime.
*   **The Master Dispatch Table:** A large table exists in the `.rdata` section of the executable. This table contains `[Schema Pointer, Handler Pointer]` pairs that serve as the default dispatch information for most generic packets.
*   **Handler & Schema Collocation:** A critical discovery is that for a given entry in the master dispatch table, the pointer to the **schema** is located immediately before the pointer for its corresponding **handler function**. Finding one often immediately reveals the other.
*   **Fast Path:** High-frequency packets (like `SMSG_AGENT_UPDATE_BATCH`) can bypass the generic dispatch system entirely. Their logic is hardcoded directly into the main dispatcher (`Msg::DispatchStream`) and they will not appear in the logger's output. These require manual tracing.

---

## The Definitive Discovery Workflow

This workflow prioritizes the fastest path from live discovery to full static analysis.

### Stage 1: Live Discovery with KXPacketInspector

This is the primary and most efficient method for mapping the SMSG protocol.

1.  **Inject the Hook:** Compile and inject the **KXPacketInspector** DLL into the live game client.
2.  **Monitor Output:** Run a debug monitor (like DebugView from Sysinternals, as Administrator) to capture the `OutputDebugStringA` messages from the hook.
3.  **Play the Game:** Perform actions in-game to trigger various server messages.
4.  **Collect the Data:** The debug log will populate with `Opcode`, `Handler Address`, and `Schema Address` information for every generic packet. This is your primary discovery map.
    ```
    [Packet Discovery] Opcode: 0x003F -> Handler: +99DFC0 -> Schema: +253FF90
    ```

### Stage 2: Static Analysis

With the Handler and Schema addresses from the logger, you can perform a full analysis in Ghidra.

1.  **Go to the Handler in Ghidra:** Navigate to the Handler RVA you discovered (e.g., `Gw2-64.exe+99DFC0`).

2.  **Confirm the Schema via Cross-Reference (XREF):**
    *   As a validation step, use Ghidra's "Show References to..." feature on the handler function.
    *   This will lead you to the handler's entry in the **Master Dispatch Table** in the `.rdata` section.
    *   Confirm that the 8-byte pointer immediately **before** your handler's pointer in this table matches the Schema RVA from your log.
        ```assembly
        .rdata:01b5ecd0    addr   DAT_0253ff90      ; <-- Schema Pointer
        .rdata:01b5ecd8    addr   Gs2c_ProtoDispatcher ; <-- Handler Pointer
        ```

3.  **Decode and Document:**
    *   Use the `KX_SMSG_Static_Decoder_RVA.lua` script with the schema's RVA to get the packet's raw structure.
    *   Analyze the handler's decompiled code to understand the purpose of the fields from the schema.
    *   Create a new Markdown file for the packet, documenting its purpose, both addresses, and the fully named structure.

### Alternate & Manual Methods (For Fast Path Packets)

If a packet does not appear in the KXPacketInspector log, it is a "Fast Path" packet. These require manual tracing.

*   **The Hardware Breakpoint Trap:** The most reliable method is to perform an Array of Byte scan for a unique part of the packet's payload, set a hardware breakpoint on access, and trace the code that reads the data. This will lead you to the hardcoded logic inside `Msg::DispatchStream`.
*   **Manual Break-and-Check:** Set an unconditional breakpoint at the entry of `Msg::DispatchStream`. Repeatedly break and check the packet's opcode (found via the `[RDX+48]` pointer chain) until you trap the desired packet, then trace its execution with `F8`.

This finalized workflow represents the most efficient and accurate process for analyzing the entire SMSG protocol.