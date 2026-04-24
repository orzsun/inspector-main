# Methodology: Function Naming from Assert Strings

## Objective

This document describes a powerful and reliable methodology for recovering original function names, namespaces, and source file locations by analyzing debug assertion strings left in the game's executable. This is a high-impact technique that dramatically improves the readability and accuracy of the reverse engineering project.

## Core Concept

During development, programmers often embed plain-text strings in assertion statements to help debug issues. These strings frequently contain the original source file path and sometimes even the function or class name. While the assertion logic is often compiled out of release builds, these strings are typically left behind in the data sections of the executable.

By finding these strings and tracing where they are used, we can effectively restore the original developer's naming and architectural conventions.

## The Workflow in Ghidra

### Phase 1: Locating the String Data Block

The majority of assert and error strings are located together in a large, contiguous block within the `.rdata` section of the executable.

1.  **Find a Known String:** Start with a known error message from one of the core functions (e.g., `"Msg::DispatchStream"` from `Msg::DispatchStream`).
2.  **Go to String in Memory:** In the Decompiler, right-click the string variable and select "Go to Address".
3.  **Explore the Region:** You will now be in the string data block. Scroll up and down from this location to see thousands of other useful strings, including many `D:\Perforce\...` file paths.

### Phase 2: Using a String to Name a Function

This is the manual process for a single function.

1.  **Identify a Target Function:** Start with a key function whose name is unknown (e.g., `FUN_140fd2c70`).
2.  **Look for Assert Calls:** Scan the decompiled code for calls to a common error-handling or logging function (e.g., `FUN_1409cd700`).
3.  **Analyze the String Argument:** Examine the string literal being passed to the error function. For `FUN_140fd2c70`, we find strings like `"Msg::MsgPack"` and file paths referencing `MsgConn.cpp`.
4.  **Deduce and Rename:**
    *   The string `"Msg::MsgPack"` is a strong candidate for the function's original name.
    *   The file path `...\Services\Msg\MsgConn.cpp` tells us the function belongs to the `Msg` service and is likely a method of the `MsgConn` class or a related `Msg` namespace.
    *   Based on this, `FUN_140fd2c70` was confidently renamed to **`Msg::MsgPack`**.

### Phase 3: Automated Discovery (Advanced)

Manually checking every function is impractical. A Ghidra script (Python/Jython) can automate this process and name thousands of functions in minutes.

*   **[Ghidra Script: `KX_NameFunctionsFromAsserts.py`](../../tools/ghidra/KX_NameFunctionsFromAsserts.py)**

The script's logic would be:

1.  **Find All Assert Strings:** Iterate through all defined strings in the `.rdata` section and use a regular expression to find any string that matches the pattern `D:\Perforce\...`.
2.  **Get Cross-References:** For each file path string found, get a list of all functions that reference it.
3.  **Parse and Name:**
    *   Parse the file path to extract key architectural tokens (e.g., `Game`, `Marker`, `Cli`, `MkrCliContext`).
    *   For each referencing function, generate a new, structured name based on these tokens (e.g., `Marker::Cli::MkrCliContext_SomeFunction`).
    *   Use the Ghidra API to apply this new name to the function.

## Conclusion

This methodology is one of the most effective ways to move beyond generic `FUN_...` labels and reconstruct a meaningful, organized view of the game's codebase. It complements automated analysis (like RTTI recovery) by providing developer-written context that is otherwise lost during compilation.
