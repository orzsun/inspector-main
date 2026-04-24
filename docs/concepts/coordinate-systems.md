# Coordinate Systems & In-Game Units

Guild Wars 2 utilizes multiple coordinate systems and a specific unit scale, which are critical to understand when working with positional data from network packets.

> **For detailed information on all GW2 unit systems, conversions, and evidence-based analysis, see: [GW2 Unit Systems - Evidence-Based Reference](https://github.com/kxtools/kx-vision/blob/main/docs/engine_internals/unit-systems.md)**

## Dual Coordinate Systems

The game employs two primary coordinate systems:

1.  **Proprietary Engine / Renderer System (Left-Handed, Z-up):** This is the coordinate system used for rendering and general game logic. It is a **left-handed** system where:
    *   **X-Axis:** Right
    *   **Y-Axis:** Forward
    *   **Z-Axis:** Up

2.  **Havok Physics System (Right-Handed, Y-up):** The underlying physics engine, Havok, uses a different, **right-handed** system where:
    *   **X-Axis:** Right
    *   **Y-Axis:** Up
    *   **Z-Axis:** Backward (negative Z is forward)

This discrepancy is a common source of confusion. Most third-party tools and analysis, including this project, will often normalize to one system. It is crucial to be aware of which system is being used when interpreting positional data.

## Unit Conversion Factor

There is a fixed conversion factor between the high-level game units (used for things like skill ranges in tooltips) and the low-level units used by the Havok physics engine.

*   **Conversion Factor:** **32**
*   **Example:** A skill with an in-game range of **960** units corresponds to **30** units in the physics system (960 / 32 = 30).

This factor is essential for correctly interpreting and applying positional data from packets, especially when correlating it with in-game effects.
