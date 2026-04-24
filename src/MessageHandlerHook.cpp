#include <windows.h> // Included first for platform definitions
#include "MessageHandlerHook.h"
#include "../libs/safetyhook/safetyhook.hpp"
#include "PacketProcessor.h"
#include "PacketData.h"
#include "GameStructs.h"
#include "AppState.h"

#include <iostream>  // For std::cout, std::cerr (initialization logging)
#include <iomanip>   // For std::hex
#include <vector>    // Used by dependencies
#include <debugapi.h> // For OutputDebugStringA (critical hook errors)
#include <cstdio>    // For sprintf_s (critical hook errors)
#include <exception> // For std::exception

// Global SafetyHook objects for managing the mid-function hooks.
SafetyHookMid g_handlerHook1{};
SafetyHookMid g_handlerHook2{};
SafetyHookMid g_handlerHook3{};
SafetyHookMid g_handlerHook4{};

// Stack offset relative to RBP to access the message data pointer (local_50).
// NOTE: This offset is specific to the compiled function's stack frame and may break with game updates.
constexpr ptrdiff_t STACK_OFFSET_MESSAGE_DATA_PTR = -0x18;

// Offsets within FUN_1412e9390 targeting the start of the 'MOV RDX, [RBP+local_50]' instructions.
// These pinpoint the locations for the mid-function hooks.
constexpr ptrdiff_t DISPATCHER_HOOK_OFFSET_SITE_1 = 0x219; // Before CALL at 1412e95ad
constexpr ptrdiff_t DISPATCHER_HOOK_OFFSET_SITE_2 = 0x228; // Before CALL at 1412e95bc
constexpr ptrdiff_t DISPATCHER_HOOK_OFFSET_SITE_3 = 0x3D4; // Before CALL at 1412e9768
constexpr ptrdiff_t DISPATCHER_HOOK_OFFSET_SITE_4 = 0x3E3; // Before CALL at 1412e9777

template <typename T>
bool TryReadMemory(const void* address, T& outValue) {
    __try {
        outValue = *reinterpret_cast<const T*>(address);
        return true;
    }
    __except (EXCEPTION_EXECUTE_HANDLER) {
        return false;
    }
}

/**
 * @brief Detour executed before a game message handler is called by the dispatcher.
 * @details Extracts message details using register context and known struct offsets,
 *          then delegates processing. Designed to work with MidHooks placed before
 *          handler call preparations in Msg::DispatchStream.
 * @param ctx SafetyHook context providing access to register state (RBX, RBP).
*/
void hookHandlerCallSite(SafetyHookContext& ctx)
{
    // Skip processing if capture is paused or the application is shutting down.
    if (kx::g_capturePaused.load(std::memory_order_acquire) ||
        kx::g_globalPauseMode.load(std::memory_order_acquire) ||
        kx::g_isShuttingDown.load(std::memory_order_acquire)) {
        return;
    }

    // pMsgConn is expected in RBX based on dispatcher function analysis.
    void* pMsgConn = reinterpret_cast<void*>(ctx.rbx);
    if (!pMsgConn) {
        return;
    }

    // messageDataPtr is derived from a stack local relative to RBP.
    void* messageDataPtr = nullptr;
    if (ctx.rbp == 0 ||
        !TryReadMemory(reinterpret_cast<const void*>(ctx.rbp + STACK_OFFSET_MESSAGE_DATA_PTR), messageDataPtr)) {
        return;
    }

    // Obtain pointer to the current message handler's information structure via MsgConn offset.
    void* handlerInfoPtr = nullptr;
    if (!TryReadMemory(
            static_cast<const void*>(static_cast<char*>(pMsgConn) + kx::GameStructs::MSGCONN_HANDLER_INFO_PTR_OFFSET),
            handlerInfoPtr) ||
        !handlerInfoPtr) {
        return;
    }

    // --- Read all key pieces of information from the Handler Info struct ---
    uint16_t messageId = 0;
    if (!TryReadMemory(
            static_cast<const void*>(static_cast<char*>(handlerInfoPtr) + kx::GameStructs::HANDLER_INFO_MSG_ID_OFFSET),
            messageId)) {
        return;
    }

    void* handlerFuncPtr = nullptr;
    if (!TryReadMemory(
            static_cast<const void*>(static_cast<char*>(handlerInfoPtr) + kx::GameStructs::HANDLER_INFO_HANDLER_FUNC_PTR_OFFSET),
            handlerFuncPtr)) {
        return;
    }

    void* msgDefPtr = nullptr;
    if (!TryReadMemory(
            static_cast<const void*>(static_cast<char*>(handlerInfoPtr) + kx::GameStructs::HANDLER_INFO_MSG_DEF_PTR_OFFSET),
            msgDefPtr)) {
        return;
    }

    uint32_t messageSize = 0;
    if (msgDefPtr &&
        !TryReadMemory(
            static_cast<const void*>(static_cast<char*>(msgDefPtr) + kx::GameStructs::MSG_DEF_SIZE_OFFSET),
            messageSize)) {
        return;
    }

    // Sanity check: if the game says the packet has a size but our pointer is null, something is wrong.
    if (messageDataPtr == nullptr && messageSize > 0) {
        return;
    }

    // --- NEW: Automated Discovery Logging ---
    // Log all three key pieces of information: Opcode, Handler, and Schema.
    if (handlerFuncPtr && msgDefPtr) {
        char buffer[256];
        uintptr_t gameBase = (uintptr_t)GetModuleHandle(L"Gw2-64.exe");
        uintptr_t handlerOffset = (uintptr_t)handlerFuncPtr - gameBase;
        uintptr_t schemaOffset = (uintptr_t)msgDefPtr - gameBase; // Calculate schema RVA

        // This new log format provides a complete mapping for static analysis.
        sprintf_s(buffer, sizeof(buffer),
            "[Packet Discovery] Opcode: 0x%04X -> Handler: +%p -> Schema: +%p\n",
            messageId,
            (void*)handlerOffset,
            (void*)schemaOffset
        );
        OutputDebugStringA(buffer);
    }

    // --- Original Packet Processing Call ---
    // Pass the captured data to your existing processing function.
    kx::PacketProcessing::ProcessDispatchedMessage(
        kx::PacketDirection::Received,
        messageId,
        static_cast<const uint8_t*>(messageDataPtr),
        messageSize,
        pMsgConn
    );
}

/**
 * @brief Installs a single SafetyHook MidHook at a specified site.
 * @param hookObject Reference to the global SafetyHookMid object to manage the hook.
 * @param siteAddress The absolute memory address to install the hook.
 * @param offset The relative offset used (for logging purposes).
 * @param siteNumber A number identifying the hook site (for logging).
 * @param errorMsg Reference to a string where error details can be written.
 * @return true if the hook was successfully installed, false otherwise.
 */
static bool InstallSingleMidHook(
    SafetyHookMid& hookObject,
    uintptr_t siteAddress,
    ptrdiff_t offset,
    int siteNumber,
    std::string& errorMsg)
{
    std::cout << "[MessageHandlerHook] Attempting MidHook at site " << siteNumber
        << " (Offset 0x" << std::hex << offset << "): 0x" << siteAddress << std::dec << std::endl;

    auto builder = safetyhook::MidHook::create(reinterpret_cast<void*>(siteAddress), hookHandlerCallSite);
    if (!builder) {
        errorMsg = "Failed to create MidHook builder for site " + std::to_string(siteNumber);
        return false;
    }

    hookObject = std::move(*builder);
    if (!hookObject) {
        // Builder might fail post-creation or during move construction
        errorMsg = "Failed to finalize MidHook object for site " + std::to_string(siteNumber);
        return false;
    }

    std::cout << "[MessageHandlerHook] Hook " << siteNumber << " installed." << std::endl;
    return true;
}

/**
 * @brief Initializes SafetyHook MidHooks at the four identified message handler preparation sites.
 * @details Orchestrates the installation using InstallSingleMidHook for each site.
 * @param dispatcherFuncAddress The runtime base address of the dispatcher function (FUN_1412e9390).
 * @return true on success (all hooks installed), false on failure.
 */
bool InitializeMessageHandlerHooks(uintptr_t dispatcherFuncAddress) {
    if (dispatcherFuncAddress == 0) {
        std::cerr << "[MessageHandlerHook] Error: Initialize called with null dispatcher address." << std::endl;
        return false;
    }

    // Calculate absolute addresses using the defined constants.
    const uintptr_t hookSite1 = dispatcherFuncAddress + DISPATCHER_HOOK_OFFSET_SITE_1;
    const uintptr_t hookSite2 = dispatcherFuncAddress + DISPATCHER_HOOK_OFFSET_SITE_2;
    const uintptr_t hookSite3 = dispatcherFuncAddress + DISPATCHER_HOOK_OFFSET_SITE_3;
    const uintptr_t hookSite4 = dispatcherFuncAddress + DISPATCHER_HOOK_OFFSET_SITE_4;

    std::string errorMsg; // Store the first error encountered
    bool success = true;

    // Install hooks sequentially, stopping on the first failure.
    if (success) { success = InstallSingleMidHook(g_handlerHook1, hookSite1, DISPATCHER_HOOK_OFFSET_SITE_1, 1, errorMsg); }
    if (success) { success = InstallSingleMidHook(g_handlerHook2, hookSite2, DISPATCHER_HOOK_OFFSET_SITE_2, 2, errorMsg); }
    if (success) { success = InstallSingleMidHook(g_handlerHook3, hookSite3, DISPATCHER_HOOK_OFFSET_SITE_3, 3, errorMsg); }
    if (success) { success = InstallSingleMidHook(g_handlerHook4, hookSite4, DISPATCHER_HOOK_OFFSET_SITE_4, 4, errorMsg); }

    // Handle failure and cleanup
    if (!success) {
        std::cerr << "[MessageHandlerHook] Error: " << errorMsg << ". Cleaning up potentially installed hooks..." << std::endl;
        CleanupMessageHandlerHooks(); // Attempt cleanup
        return false;
    }

    std::cout << "[MessageHandlerHook] All message handler MidHooks installed successfully." << std::endl;
    return true;
}

/**
 * @brief Cleans up and removes the installed SafetyHook MidHook(s).
 */
void CleanupMessageHandlerHooks() {
    // Destroy hook objects via RAII by assigning empty objects. Check validity first.
    if (g_handlerHook1) { g_handlerHook1 = {}; std::cout << "[MessageHandlerHook] Hook 1 cleaned up." << std::endl; }
    if (g_handlerHook2) { g_handlerHook2 = {}; std::cout << "[MessageHandlerHook] Hook 2 cleaned up." << std::endl; }
    if (g_handlerHook3) { g_handlerHook3 = {}; std::cout << "[MessageHandlerHook] Hook 3 cleaned up." << std::endl; }
    if (g_handlerHook4) { g_handlerHook4 = {}; std::cout << "[MessageHandlerHook] Hook 4 cleaned up." << std::endl; }
}
