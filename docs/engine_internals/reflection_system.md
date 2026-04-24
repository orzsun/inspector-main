# Engine Internals: The ArenaNet `hkReflect` Runtime System

**Status:** Confirmed (Evidence-Based Analysis)

This document outlines the core principles of a specific **runtime reflection system** used in the Guild Wars 2 game engine. The findings are the result of an extensive forensic analysis of the `Gw2-64.exe` binary.

This system is the blueprint for the game's runtime object model, defining the memory layout, inheritance, and member variables for hundreds of core engine and gameplay classes.

> **Related Tools:** See the reflection dumper scripts used to extract and analyze this data:
> - [Static Addresses Dumper](../../tools/ghidra/KX_ReflectionDumper_StaticAddresses.py)
> - [Dynamic Scan Dumper](../../tools/ghidra/KX_ReflectionDumper_DynamicScan.py)

## Core Discovery: Multiple Havok Systems

Initial analysis of the binary can be misleading, as the engine utilizes **multiple, distinct, Havok-based systems** for managing its data. It is now understood that there is a clear separation between the system used for file storage and the one used for runtime object representation.

1.  **File Serialization System:** Used for reading and writing game asset files. This system is responsible for handling the Havok Packfile (`.pf`) format used in the game's `.dat` archive. This is the system primarily documented by community tools like `gw2_reverse` and `gw2formats`.
2.  **Runtime Reflection System (`hkReflect`):** This is a separate, more modern system based on the `hkReflect` namespace. Its purpose appears to be runtime type-checking, object management, and powering high-level engine features like the internal developer UI.
3.  **Other Specialized Runtime Systems:** It is likely that even more, smaller, specialized reflection-like systems exist for specific engine features (e.g., for handling arrays of reflected types, gameplay systems, or other specific data structures). This is an area for future research.

**This document focuses exclusively on the second system: the `hkReflect` runtime engine.**

* **Evidence:** The executable is rich with strings referencing this specific system, such as `hkReflect::Type`, `hkReflect::FieldDecl`, `hkReflect::TypeReg`, `ReflectStrings`, and even `ReflectionInterface.dll`. This confirms that a complete, parallel reflection stack is present for runtime purposes, separate from file serialization.

## Key Architectural Findings

#### 1. System Initialization via a Global Linked List

The `hkReflect` system, and likely other core engine systems, are not initialized via direct, hardcoded function calls. Instead, they are part of a master linked list of systems that are initialized at startup.

* **Evidence:** A global pointer at `DAT_14293bba0` serves as the head of this list. A constructor function (`FUN_1415834d0`) adds new systems (like the global `ReflectStrings` object) to the front of this list. A separate function (`FUN_1415838f0`) implements a "move-to-front" heuristic, proving this list is actively managed at runtime for performance.
* **Conclusion:** To find and use a system like the reflection engine, the game's code first gets the head of this list from the global pointer and iterates through it to find the system it needs by name.

#### 2. The Blueprint: A Continuous Block of `ClassInitializer` Data

The definitions for all reflected classes are not scattered throughout the executable. The C++ linker aggregates them into a few large, continuous arrays in the `.rdata` section.

* **Evidence:** The most successful and complete dump of class data was achieved by treating the data from `0x142607840` to `0x14260BC60` as a single, contiguous array of class definition structures.
* **Conclusion:** This array is the primary source for the raw class blueprints. A script can iterate through this block to dump every known class in the game.

#### 3. The `MemberInitializer`: Proven by Destructor Code

The layout of the structure describing each class member is not a guess. It is proven by the game's own code for the `ReflectStrings::~ReflectStrings` destructor.

* **Evidence:** Helper functions used by the destructor iterate through an array of member data in **24-byte (0x18)** increments. Accessor functions within this code read two distinct 8-byte `longlong` values from offsets `+0x08` and `+0x10` within each 24-byte block.
* **Conclusion:** This is undeniable proof that the `MemberInitializer` (internally likely a `FieldDecl`) is a 24-byte structure with three 8-byte fields.

## The Final, Evidence-Based Data Structures

Based on this forensic analysis, these are the definitive high-level structures for the reflection blueprint.

#### `ClassInitializer` (Size: 40 bytes / 0x28)

This structure serves as the header for a class definition.

| Offset  | Data Type            | Name                 | Description                                                |
| :------ | :------------------- | :------------------- | :--------------------------------------------------------- |
| `+0x00` | `const char*`        | `classNamePtr`       | A pointer to a null-terminated string with the class name. |
| `+0x08` | `const char*`        | `parentClassNamePtr` | A pointer to the parent's class name string.               |
| `+0x10` | `int`                | `version`            | The version number for this class definition.              |
| `+0x14` | `int`                | `flags`              | Flags or the number of implemented interfaces.             |
| `+0x18` | `MemberInitializer*` | `membersPtr`         | A pointer to the start of the member definition array.     |
| `+0x20` | `int`                | `numMembers`         | The number of members in the array.                        |

#### `MemberInitializer` / `FieldDecl` (Size: 24 bytes / 0x18)

This structure describes a single variable within a class.

| Offset  | Data Type     | Name               | Description                                                             |
| :------ | :------------ | :----------------- | :---------------------------------------------------------------------- |
| `+0x00` | `longlong`    | `signatureOrType`  | A "tagged" value. Can be a small integer (a type ID) or a pointer.      |
| `+0x08` | `const char*` | `namePtr`          | A pointer to a null-terminated string with the member's name.           |
| `+0x10` | `longlong`    | `typeDataOrOffset` | A "tagged" value containing type data, flags, and/or the memory offset. This is a 64-bit bitfield where the lower 16 bits represent the field's memory offset. |

## Advanced Insights and Future Work

The ongoing analysis of the `hkReflect` system has yielded further critical insights into its architecture and revealed clear paths for continued investigation.

### Implicit vs. Explicit Reflection

It has been discovered that not all fundamental engine classes are explicitly registered within the reflection system dumps. Core building blocks like `hkBaseObject` and `hkReferencedObject` are implicitly known by the engine's "Type Decoder" virtual machine. This means the reflection system primarily documents classes that need dynamic lookup, while foundational types are hardcoded into the system's parsing logic itself.

### Static vs. Dynamic Registration

The current class dumps primarily represent classes that are statically registered at engine startup. However, the engine employs a modular design where specialized classes, such as `hkpSerializedAgentNnEntry` used in networking, are likely registered dynamically by their respective modules only when needed during gameplay. Future analysis will involve dynamic observation to identify additional `TypeReg::add()` calls during runtime.

#### Inferring C++ Types from Copy/Move Dispatchers

Further analysis of the `Reflect_GenericCopyDispatcher` and `Reflect_MemmoveDispatcher` functions has revealed a direct mechanism for inferring C++ types. These functions contain `switch` statements that dispatch operations based on the *size* of the data being copied or moved. This allows for the deduction of primitive C++ types:

*   **1 byte:** `char`, `bool`, `int8_t`, `uint8_t`
*   **2 bytes:** `short`, `int16_t`, `uint16_t`
*   **4 bytes:** `int`, `float`, `int32_t`, `uint32_t`
*   **8 bytes:** `long long`, `double`, `void*`, or any pointer type (`T*`)

For sizes between 9 and 15 bytes, the dispatchers handle these as combinations of 8-byte and smaller chunks, indicating custom structures or fixed-size arrays. Larger data blocks are handled efficiently using SIMD instructions (e.g., `MOVDQU`, `VMOVDQU`, `VMOVNTDQ`). This size-based dispatch is a key component of how the engine interprets the `MemberInitializer` `signatureOrType` field to perform type-aware operations.

## Project Complete: The Blueprint Is Decoded

The investigation has concluded successfully. The "Reader" code—a small virtual machine responsible for parsing the reflection data—was located and analyzed.

The key functions, `Reflect_TypeDecoder` and `Reflect_FieldInitDispatcher`, revealed the exact logic for interpreting the `MemberInitializer` structure. This analysis confirmed that the `typeDataOrOffset` field is a bitfield containing the member's memory offset and various type-specific flags.

With this knowledge, the [Dynamic Scan Dumper](../../tools/ghidra/KX_ReflectionDumper_DynamicScan.py) script has been finalized. It now accurately dumps the engine's class layouts into a clean, human-readable C++ header file, fulfilling the project's primary goal. The most critical decompiled code snippets that serve as evidence of this system's functionality have been preserved in the [`evidence/reflection_system`](./evidence/reflection_system) directory.

This document represents the complete, evidence-based foundation of the engine's core reflection system.