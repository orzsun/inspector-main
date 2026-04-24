# The Game Server Protocol (`gs2c`/`c2gs`)

**Status:** Confirmed (In-Depth Client-Side Analysis)

This document provides a detailed analysis of the primary gameplay connection, which is the current focus of the KX Packet Inspector project. This protocol handles all real-time, in-world gameplay interactions, including movement, combat, and agent synchronization. It employs a highly dynamic, multi-stage dispatch and serialization system.

---

### Incoming (SMSG) Packet Processing

The pipeline for handling messages from the server is a multi-stage, dynamic process designed for performance and flexibility.

1.  **Framing & Initial Processing (`MsgConn_Dispatch`):** All incoming `gs2c` raw byte streams first enter [`MsgConn_Dispatch`](../../architectural_evidence/incoming_smsg/MsgConn_Dispatch.c). This function is responsible for reading from the network ring buffer, identifying message frames, and performing initial processing (decryption, decompression) to yield plaintext messages. This also includes the initial connection handshake handled by functions like [`MsgRaw_ClientRecvEncrypt.c`](../../architectural_evidence/incoming_smsg/MsgRaw_ClientRecvEncrypt.c). Once framed, individual messages are then passed to [`Msg::DispatchStream`](../../architectural_evidence/incoming_smsg/Msg_DispatchStream.c) for further processing.

*   **High-Level Dispatchers:** Several high-level systems feed into or are triggered by this pipeline:
    *   [`Gw2::Engine::Controls::CtlInstance`](../../engine_internals/evidence/core_dispatch/Gw2_Engine_Controls_CtlInstance.c): A high-level dispatcher for control-related events or commands within the game engine.
    *   [`GcSrv::Dispatch`](../../architectural_evidence/incoming_smsg/GcSrv_Dispatch.c): A top-level dispatcher for commands originating from the game server's perspective, often leading to client-side actions.
    *   [`GcGameCmd::Handler`](../../architectural_evidence/incoming_smsg/GcGameCmd_Handler.c): A central handler for "Game Commands" that manages the lifecycle of the `GcGameCmd` system.

2.  **Dispatch Type Evaluation:** For each message, `Msg::DispatchStream` retrieves its `Handler Info` structure. A key field in this structure is the "Dispatch Type" (`HandlerInfo+0x10`), which dictates the processing path:
    *   **Dispatch Type `0` (Generic Path):** For most packets. Leads to schema-driven parsing.
    *   **Dispatch Type `1` (Fast Path):** For high-frequency packets like `SMSG_AGENT_UPDATE_BATCH` (0x0001). This path uses hardcoded logic for performance.

3.  **Parsing & Data Extraction:**
    *   **Generic Path (`MsgUnpack::ParseWithSchema`):** For Type `0` packets, `Msg::DispatchStream` calls the schema virtual machine, [`MsgUnpack::ParseWithSchema`](../../architectural_evidence/incoming_smsg/MsgUnpack_ParseWithSchema.c), to transform raw bytes into a structured data tuple.
    *   **Fast Path (Hardcoded Parsing):** For Type `1` packets, `Msg::DispatchStream` contains hardcoded logic to manually parse the payload directly from the network buffer.

4.  **Handler Execution:**
    *   **Generic Handler Dispatch:** `Msg::DispatchStream` calls the handler function pointer (from `HandlerInfo+0x18`), passing it the parsed data tuple. (e.g., [`Marker::Cli::ProcessAgentMarkerUpdate`](./smsg/evidence/Marker_Cli_ProcessAgentMarkerUpdate.c)).
    *   **Fast Path Notification (Decoupled Event):** After the hardcoded parsing and state updates are completed *inside* `Msg::DispatchStream`, a notification stub (e.g., [`Event::PreHandler_Stub_0x88`](./smsg/evidence/Event_PreHandler_Stub_0x88.c)) is called. This stub does not process packet data itself; its role is to queue a generic, post-facto event using the [`Event::Factory_QueueEvent`](../../engine_internals/evidence/event_system/Event_Factory_QueueEvent.c) system.

### Master Dispatch Table Registration

Static analysis has revealed the existence of a master dispatch table in the `.rdata` section of the executable. This table contains pairs of `[Schema Pointer, Handler Pointer]` that serve as the default dispatch information for most generic SMSG opcodes.

This table is not used directly by its address. Instead, it is registered with the `MsgChannel` system during client initialization by a dedicated function, `FUN_001de000`. This function passes blocks of this table to a registration routine (`FUN_00fd5eb0`), which builds the final, ready-to-use dispatch map in memory. This confirms that the table in the executable is a static "staging area" for the live dispatch system.

While analyzing this registration function is not necessary for the day-to-day discovery of individual packets (for which using the C++ logger and finding XREFs is faster), it provides the definitive architectural proof of how the generic SMSG dispatch system is constructed.

### Outgoing (CMSG) Packet Processing

The pipeline for sending messages to the server is based on a hybrid model that combines high-level data aggregation with a generic, schema-driven serialization engine.

1.  **High-Level "Data Aggregators":** For each major player action (moving, jumping, using a skill), there is a dedicated high-level function (e.g., in the `Gw2::Engine::Agent::AgCommand` family). This function's job is to gather all necessary game state and package it into a temporary data structure.

2.  **Serialization (`CMSG::BuildAndSendPacket`):** The aggregator function then calls a generic builder, which in turn uses a master schema table and the [`Msg::MsgPack`](../../architectural_evidence/outgoing_cmsg/Msg_MsgPack.c) engine to serialize the temporary data structure into the final packet bytes. The schema is retrieved via a call to [`MsgConn::BuildPacketFromSchema`](../../architectural_evidence/outgoing_cmsg/MsgConn_BuildPacketFromSchema.c).

3.  **Sending Mechanism:** The serialized packet bytes are placed into an outgoing buffer to be sent to the server. There are two primary mechanisms for this:
    *   **Buffered Stream (Primary Gameplay Pathway):** For the vast majority of gameplay-related actions (including discrete events like jumps and skill use, as well as continuous data like movement), the serialized data is written directly into the main `MsgSendContext` buffer. This buffer is then flushed in batches by a call to [`MsgConn::FlushPacketBuffer`](../../architectural_evidence/outgoing_cmsg/MsgConn_FlushPacketBuffer.c).
    *   **Direct Queue (`MsgConn::QueuePacket`):** This pathway is typically reserved for simpler, out-of-band system commands or legacy packets that require more immediate sending. A prime example is `CMSG_0x0006_PING_RESPONSE`, whose builder directly calls [`MsgConn::QueuePacket`](../../architectural_evidence/outgoing_cmsg/MsgConn_QueuePacket.c), bypassing the main buffer.
