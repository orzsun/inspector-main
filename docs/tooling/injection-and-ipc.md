# Tooling: Injection and Inter-Process Communication (IPC) Framework

This document outlines the architectural patterns and common techniques used to build external tools that interact with a live game client. This framework typically involves injecting code into the game process and establishing communication channels to enable advanced functionality and control.

## Purpose of Documentation

These techniques represent a powerful class of methods for interacting with closed-source applications, particularly in the context of game reverse engineering and tool development. Documenting this framework serves several key purposes:

*   **Knowledge Transfer:** Provides a clear, organized reference for understanding complex inter-process communication and code injection methodologies.
*   **Accelerated Development:** Offers a proven architectural blueprint, allowing developers to build new tools more efficiently without reinventing core infrastructure.
*   **Resilience & Adaptability:** Highlights strategies for creating tools that are more robust against game updates and changes.
*   **Educational Resource:** Serves as a practical guide for those learning advanced reverse engineering, dynamic analysis, and game hacking concepts.

## High-Level Architecture

A common setup for external game tools involves two primary components:

1.  **External Controller (e.g., GUI Application):** A separate application that runs outside the game process. It provides a user interface, handles configuration, and orchestrates the tool's behavior.
2.  **Injected Module (e.g., DLL):** A dynamic-link library (DLL) or other executable code that is loaded directly into the game's memory space. This module performs the direct interaction with the game's internal functions and data.

These two components require a robust system for inter-process communication.

```
+---------------------+       +-------------------+       +---------------------+
| External Controller | <---> | IPC Channel       | <---> | Injected Module     |
| (e.g., GUI App)     |       |                   |       | (e.g., DLL in Game) |
+---------------------+       +-------------------+       +---------------------+
                                                                  |
                                                                  | Interacts with
                                                                  v
                                                             +--------------------+
                                                             | Game Process       |
                                                             +--------------------+
```

## Why This Approach? (Advantages)

This architectural pattern offers significant benefits for game tooling:

*   **Powerful Control & Access:** The injected module runs directly within the game's process, granting full access to its memory, functions, and data structures. This allows for deep introspection and precise manipulation.
*   **Resilience to Updates:** Techniques like pattern scanning (detailed below) enable the tool to adapt to game updates that change memory addresses, reducing maintenance overhead compared to hardcoded offsets.
*   **Separation of Concerns:** Divides the tool into a user-friendly external interface and a low-level, in-game module. This improves maintainability and stability.
*   **Flexibility:** Supports a wide range of functionalities, from passive data logging (e.g., packet sniffing) to active gameplay modification (e.g., feature toggles).
*   **Stealth (Relative):** Some injection methods (like manual mapping) and dynamic function discovery can offer a degree of stealth against basic anti-cheat systems, as they avoid typical API calls.

## Key Components and Mechanisms

### 1. Code Injection

The process of loading an external module directly into a target process's memory space.

*   **Common Method: Manual Mapping:** This technique involves reading the target module's raw bytes, allocating memory in the target process, copying the module's headers and sections, performing necessary relocations and import resolutions, and then manually executing its entry point (e.g., `DllMain`).
*   **Advantage:** Manual mapping can offer increased stealth and resilience against basic anti-cheat detection compared to standard API-based injection methods (like `LoadLibrary`), as it bypasses default Windows loader routines.

### 2. Inter-Process Communication (IPC)

Enables communication between the external controller and the injected module.

*   **Mechanism: Named Pipes:** A flexible and common IPC method on Windows. One application creates a named pipe server, and the other connects as a client. Data can be sent and received over this pipe.
*   **Usage:** Typically used by the injected module to send intercepted game data (e.g., network packets, game state information) back to the external controller for display, logging, or modification.

### 3. Dynamic Function Discovery

Locating internal game functions when their addresses are not fixed (e.g., due to ASLR or frequent game updates).

*   **Mechanism: Pattern Scanning (Signature Scanning):** Involves searching the game's executable memory for unique byte sequences (signatures) that correspond to the prologue or specific instructions of target functions.
*   **Benefit:** Provides resilience against game updates, as a function's relative structure may remain stable even if its absolute memory address changes.

### 4. Function Hooking

Intercepting calls to original game functions to observe or modify their behavior.

*   **Mechanism:** Libraries (e.g., Detours, MinHook, SafetyHook) are used to rewrite the prologue of a target function, redirecting execution to a custom "hook" function. After the custom logic executes, the hook can then call the original function (or not, depending on the desired behavior).
*   **Applications:**
    *   **Data Interception:** Observing arguments passed to a function (e.g., network packet buffers, game events).
    *   **Behavior Modification:** Altering arguments, modifying return values, skipping original function execution, or injecting new code.
    *   **"Man-in-the-Middle" Proxy:** Intercepting data, sending it to an external controller for modification, receiving the modified data back, and then resuming the original function call with the altered data.

### 5. Remote Procedure Calls (RPC)

Provides a structured and often type-safe way for an external process to invoke functions directly within the injected module.

*   **Mechanism:** RPC frameworks (e.g., Microsoft RPC) define interfaces where functions can be exposed from one process to another. The framework handles the serialization, transport, and deserialization of function calls and their arguments.
*   **Usage:** Ideal for the external controller to remotely enable/disable features, change configuration settings, or trigger specific actions within the injected module.
*   **Benefit:** Simplifies complex control logic, allowing the external controller to interact with the injected module as if it were part of its own codebase.

## Conclusion

This framework provides a robust foundation for building external game tools. By combining code injection, flexible IPC, dynamic function discovery, and structured remote control, developers can create powerful applications for research, analysis, and custom gameplay experiences.
