# Analysis Playbook: The Login Protocol

**Primary Components:**
*   `GcApi`: The high-level connection state machine.
*   `GcAuthCmd`: The namespace for authentication packet builders.

## Summary

This document details the architecture of the **Login Server protocol (`ls2c`/`c2ls`)**. This is the initial, short-lived connection the Guild Wars 2 client makes upon startup to handle account authentication, character selection, and the hand-off to the persistent Game and Platform servers.

The system is managed by a high-level API (`GcApi`) which orchestrates a series of authentication commands (`GcAuthCmd`) that are sent to the server.

## Architectural Role: The First Contact

The Login Protocol is the first of the three major network systems the client interacts with. Its lifecycle is as follows:

1.  **Connection:** The client connects to the Login Server.
2.  **Authentication:** The client and server exchange a series of packets, managed by `GcApi` and built by the various `GcAuthCmd` functions. This validates the user's credentials and session.
3.  **Character Info:** The server sends character list data.
4.  **Hand-off:** Upon successful character selection, the Login Server sends a final command that contains the network coordinates for the specific Game Server and Platform Server that the client should connect to for the gameplay session.
5.  **Disconnect:** The client disconnects from the Login Server.

## Key Components

### 1. `GcApi` (The State Machine)

*   **Evidence:** `../../protocols/login/evidence/GcApi_StateMachine.c`
*   **Role:** This is the master controller for the login connection's lifecycle. It operates as a state machine, checking the current connection status and calling the appropriate `GcAuthCmd` functions to send the next packet in the sequence. It is responsible for orchestrating the entire authentication process from start to finish.

### 2. `GcAuthCmd` (The Packet Builders)

*   **Evidence:** `../../protocols/login/evidence/GcAuthCmd_PacketBuilders.c`
*   **Role:** This is a collection of dozens of small, single-purpose functions. Each function is responsible for building and sending one specific Client-to-Login-Server (`c2ls`) packet. By analyzing these functions, the opcodes and payloads for the entire authentication sequence can be reverse-engineered.

## Discovered `c2ls` Packets

Analysis of the `GcAuthCmd` functions has revealed the builders for numerous authentication-related packets. The function names often correspond to the opcode being sent in hexadecimal.

| Opcode | Builder Function | Notes |
| :--- | :--- | :--- |
| `0x0001` | `GcAuthCmd(void)` | Sends `timeGetTime()`, likely an initial handshake. |
| `0x0002` | `GcAuthCmd(undefined8 *, ...)` | Handles encryption key exchange. |
| `0x000B` | `GcAuthCmd(undefined4)` | |
| `0x000C` | `GcAuthCmd(undefined4, uint, ...)` | |
| `0x000E` | `GcAuthCmd(undefined4, undefined4, ...)` | |
| `0x000F` | `GcAuthCmd(undefined1)` | |
| `0x0010` | `GcAuthCmd(undefined4, undefined4, ...)` | |
| `0x0012` | `CMSG::BuildAndSendPacket(...)` | The call is inside a `GcAuthCmd` wrapper. |
| `0x0014` | `GcAuthCmd(undefined4, undefined8)` | |
| `0x0016` | `GcAuthCmd(undefined4, undefined8)` | |
| `0x0018` | `GcAuthCmd(undefined4, undefined8, ...)` | |
| `0x0019` | `GcAuthCmd(undefined4)` | |
| `0x001E` | `GcAuthCmd(void)` | |
| `0x0022` | `GcAuthCmd(undefined4)` | |
| `0x0023` | `GcAuthCmd(void)` | |
| `0x0024` | `GcAuthCmd(undefined4, undefined4, ...)` | |
| `0x0025` | `GcAuthCmd(undefined4, undefined8, ...)` | |
| `0x0026` | `GcAuthCmd(undefined4, undefined4, ...)` | |
| `0x0027` | `GcAuthCmd(undefined4, undefined4 *)` | |
| `0x0028` | `GcAuthCmd(undefined4, undefined4, ...)` | |
| `0x0036` | `CMSG::BuildAndSendPacket(...)` | The call is inside a `GcAuthCmd` wrapper. |

## Implications for the Project

This discovery fully maps out a third, distinct network protocol. While the KX Packet Inspector does not currently hook this transient connection, these findings provide a complete architectural picture and a clear roadmap for any future work aimed at analyzing the full login sequence.