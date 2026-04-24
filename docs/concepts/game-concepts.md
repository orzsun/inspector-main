# Core Game Concepts

This document outlines fundamental game mechanics and networking concepts that provide essential context for understanding the Guild Wars 2 network protocol.

## Server & Network Internals

*   **Server Tick Rate:** The game server operates on a tick rate of **25 ticks per second** (one tick every 40ms). All physics and game state updates are synchronized to this rate. This is a critical value for understanding skill timings and client-side simulation.

*   **Network Bubble:** The server does not send the client information about all agents on the map simultaneously. Instead, it only sends data for agents within a specific range of the player, known as the "network bubble." 
    *   This radius is typically **5,000 in-game units**.
    *   It can occasionally expand to **10,000 in-game units**.

*   **Lag Compensation (Server Absorb):** The server has a lag compensation mechanism that allows it to accept client-side actions (ticks) that are up to **400ms** old. This allows for tick-perfect skill casting from the client's perspective, as it mitigates the negative impact of network latency (ping).

*   **Rate Limiting:** The game servers employ rate-limiting mechanisms to prevent abuse. For example, sending too many requests in a short period (e.g., rapidly selling items on the Trading Post) can result in a temporary error from the server.

## Game State & Mechanics

*   **In-Combat Status:** The primary method for determining if a player is in combat is by checking their `health_regen` property. Natural health regeneration is disabled during combat. The server does not send a simple "in-combat" boolean flag.

*   **State Synchronization Delay:** There can be a noticeable delay (up to 0.5 seconds) for certain state changes to be reflected back to the client after an action is taken. An example is an Elementalist's `secondary_profession_state` updating after an attunement swap. This must be accounted for when building tools that rely on immediate state information.
