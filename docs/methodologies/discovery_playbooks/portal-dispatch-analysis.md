# Analysis Playbook: The Portal Dispatcher

**Primary Functions:**
*   `Portal::DynamicHandler_Entry`
*   `Portal::DispatchMessage`

**Alias:** `Gs2c_PostParseDispatcher` (internal research name for the system as a whole)

## Summary

The "Portal" system is the **Application Layer** responsible for handling all Platform Server (`ps2c`) messages. This includes account-wide services, social features (chat, guilds, friends), and commercial interactions (trading post, gem store).

Crucially, these `ps2c` messages are **tunneled through the main Game Server (`gs2c`) connection**. The `Portal::DispatchMessage` function is the central hub that receives these tunneled messages from the low-level network layer and routes them to the appropriate application logic.

## Architectural Role: The Post-Networking Application Layer

1.  **The Networking Layer (`Msg::DispatchStream`):** This low-level function receives the raw `gs2c` byte stream. It decrypts, deframes, and prepares a clean message. For all Portal-related opcodes, it looks up the registered handler and finds that it points to a single function: `Portal::DynamicHandler_Entry`.

2.  **The Hand-off (`Portal::DynamicHandler_Entry`):** `Msg::DispatchStream` calls `Portal::DynamicHandler_Entry` via a dynamic `CALL RAX`. This is the hand-off from the Networking Layer to the Application Layer. This wrapper function's primary job is to immediately delegate the message to the main dispatcher, while also handling a few special-case opcodes (like `0x60` and `0x41`) that require extra logic after the main dispatch is complete.

3.  **The Application Layer (`Portal::DispatchMessage`):** This is the core of the Portal system. It receives the clean message from the wrapper and contains a massive `switch` statement that looks at the message's opcode. It then calls the final logic function (e.g., `PlManager`, `PlChannel`, `PlCliGemVault`) to actually perform the work.

This explains the old alias `Gs2c_PostParseDispatcher`:
*   **`Gs2c`**: Because the messages it handles arrive *on* the Game Server to Client stream.
*   **`PostParse`**: Because it runs *after* all low-level network parsing is completed by `Msg::DispatchStream`.
*   **`Dispatcher`**: Because its entire purpose is to dispatch messages to dozens of different application-level handlers.

## The Full Dispatch Chain

The complete execution flow for a Portal message is as follows:

1.  A `ps2c` message arrives, tunneled inside the main `gs2c` stream.
2.  `Msg::DispatchStream` processes the frame and identifies its opcode. It dynamically calls the registered handler.
3.  The handler, **`Portal::DynamicHandler_Entry`**, is executed.
4.  `Portal::DynamicHandler_Entry` immediately calls **`Portal::DispatchMessage`**.
5.  `Portal::DispatchMessage`'s internal `switch` statement finds the correct `case` for the opcode.
6.  The final application logic function (e.g., `PlCliUser`, `PlCliGemVault`) is called, which updates the game state.
7.  Execution returns to `Portal::DynamicHandler_Entry`, which may perform additional special-case logic before finally returning.

## The Gold Mine: The `switch` Statement

The body of `Portal::DispatchMessage` contains a `switch` statement with **over 50 `case` blocks**. This is a gold mine for packet discovery. Each `case` corresponds to a specific `ps2c` message, and the function called within that case is the final handler for that message.

### Next Steps for Packet Discovery

Analyzing this single function is the key to unlocking the entire Platform/Portal protocol. The workflow is simple and repeatable:

1.  Open [`../../protocols/portal/evidence/Portal_DispatchMessage.c`](../../protocols/portal/evidence/Portal_DispatchMessage.c).
2.  Examine the `switch(*param_3)` statement.
3.  For each `case [OPCODE]:`, identify the primary function being called.
4.  This gives you a definitive mapping: **`OPCODE -> Final Handler Function`**.
5.  By analyzing these final handlers (e.g., `PlManager`, `PlChannel`), the structure of every Portal packet can be determined.

## Supporting Evidence (Decompilations)

This architectural understanding is supported by the following decompiled functions:

*   **The Dispatch Chain:**
    *   [`../../architectural_evidence/incoming_smsg/Msg_DispatchStream.c`](../../architectural_evidence/incoming_smsg/Msg_DispatchStream.c)
    *   [`../../protocols/portal/evidence/Portal_DynamicHandler_Entry.c`](../../protocols/portal/evidence/Portal_DynamicHandler_Entry.c)
    *   [`../../protocols/portal/evidence/Portal_DispatchMessage.c`](../../protocols/portal/evidence/Portal_DispatchMessage.c)
*   **The Portal Connection System:**
    *   [`../../protocols/portal/evidence/PortalCli_Constructor.c`](../../protocols/portal/evidence/PortalCli_Constructor.c)
    *   [`../../protocols/portal/evidence/PortalCli_Auth.c`](../../protocols/portal/evidence/PortalCli_Auth.c)
    *   [`../../protocols/portal/evidence/GcPortal_Router.c`](../../protocols/portal/evidence/GcPortal_Router.c)