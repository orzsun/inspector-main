#include "MsgSendHook.h"
#include "PacketProcessor.h" // Include the new processor header
#include "AppState.h"        // For g_capturePaused, g_isShuttingDown
#include "GameStructs.h"     // For MsgSendContext definition
#include "HookManager.h"
#include "PacketHeaders.h"

#include <iostream> // For temporary error logging (replace with Log.h later)
#include <cstring>
#include <algorithm>

// Function pointer to the original game function. Set by MinHook.
MsgSendFunc originalMsgSend = nullptr;
// Address of the target function, used for cleanup.
static uintptr_t hookedMsgSendAddress = 0;

namespace {
void CaptureUseSkillPacket(const uint8_t* buffer, std::size_t bufferSize) {
    if (!buffer || bufferSize < sizeof(uint16_t)) {
        return;
    }

    constexpr uint16_t kUseSkillHeader = static_cast<uint16_t>(kx::CMSG_HeaderId::USE_SKILL);
    constexpr std::size_t kMaxCaptureBytes = 64;
    int foundOffset = -1;
    for (std::size_t i = 0; i + sizeof(uint16_t) <= bufferSize; ++i) {
        uint16_t header = 0;
        std::memcpy(&header, buffer + i, sizeof(header));
        if (header == kUseSkillHeader) {
            foundOffset = static_cast<int>(i);
            break;
        }
    }

    if (foundOffset < 0) {
        return;
    }

    kx::SkillUsePacketCapture capture = {};
    capture.valid = true;
    capture.header = kUseSkillHeader;
    capture.totalBufferSize = static_cast<int>(bufferSize);
    capture.packetOffset = foundOffset;
    capture.packetSize = static_cast<int>(std::min<std::size_t>(kMaxCaptureBytes, bufferSize - static_cast<std::size_t>(foundOffset)));
    capture.tickMs = GetTickCount64();
    std::memcpy(capture.bytes.data(), buffer + foundOffset, static_cast<std::size_t>(capture.packetSize));

    std::lock_guard<std::mutex> lock(kx::g_lastSkillUsePacketMutex);
    kx::g_lastSkillUsePacket = capture;
}
}

// Detour function for the game's internal message sending logic.
// This function now primarily captures the context and delegates processing.
void __fastcall hookMsgSend(void* param_1) {

    // Check if packet capture is active before processing.
    // This check happens *before* calling the original function.
    if (!kx::g_capturePaused.load(std::memory_order_acquire) &&
        !kx::g_globalPauseMode.load(std::memory_order_acquire) &&
        !kx::g_isShuttingDown.load(std::memory_order_acquire)) {
        if (param_1 != nullptr) {
            // Cast the context pointer.
            auto* context = reinterpret_cast<kx::GameStructs::MsgSendContext*>(param_1);
            std::size_t bufferSize = 0;
            int bufferState = -1;
            uint8_t* packetStart = nullptr;

            __try {
                bufferSize = context->GetCurrentDataSize();
                bufferState = context->bufferState;
                packetStart = context->GetPacketBufferStart();
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                OutputDebugStringA("[hookMsgSend] SEH while reading MsgSendContext.\n");
                bufferSize = 0;
                bufferState = -1;
                packetStart = nullptr;
            }

            uint16_t header = 0;
            if (bufferSize >= sizeof(header) && packetStart) {
                std::memcpy(&header, packetStart, sizeof(header));
            }
            kx::g_lastOutgoingHeader.store(header, std::memory_order_release);
            kx::g_lastOutgoingSize.store(static_cast<int>(bufferSize), std::memory_order_release);
            kx::g_lastOutgoingBufferState.store(bufferState, std::memory_order_release);
            kx::g_lastOutgoingTickMs.store(GetTickCount64(), std::memory_order_release);
            CaptureUseSkillPacket(packetStart, bufferSize);

            __try {
                // Delegate the actual processing and logging.
                kx::PacketProcessing::ProcessOutgoingPacket(context);
            }
            __except (EXCEPTION_EXECUTE_HANDLER) {
                OutputDebugStringA("[hookMsgSend] SEH during outgoing processing call.\n");
            }
        }
        else {
            // Log::Warn("[hookMsgSend] Called with null context pointer.");
            OutputDebugStringA("[hookMsgSend] Warning: Called with null context pointer.\n");
        }
    }

    // CRITICAL: Always call the original function, regardless of capture state or errors.
    if (originalMsgSend) {
        originalMsgSend(param_1);
    }
    else {
        // Log::Critical("[hookMsgSend] Original MsgSend function pointer is NULL!");
        OutputDebugStringA("[hookMsgSend] CRITICAL ERROR: Original MsgSend function pointer is NULL!\n");
        // Avoid crashing, but logging indicates a severe setup issue.
    }
}

// Initializes the MinHook detour for the message sending function.
bool InitializeMsgSendHook(uintptr_t targetFunctionAddress) {
    if (targetFunctionAddress == 0) {
        // Log::Error("InitializeMsgSendHook called with null address.");
        std::cerr << "[Error] InitializeMsgSendHook called with null address." << std::endl;
        return false;
    }

    // Create the hook.
    hookedMsgSendAddress = targetFunctionAddress; // Keep track for potential specific cleanup
    if (!kx::Hooking::HookManager::CreateHook(reinterpret_cast<LPVOID>(targetFunctionAddress), &hookMsgSend, reinterpret_cast<LPVOID*>(&originalMsgSend))) {
        std::cerr << "[MsgSendHook] Hook creation failed via HookManager." << std::endl;
        hookedMsgSendAddress = 0;
        return false;
    }

    if (!kx::Hooking::HookManager::EnableHook(reinterpret_cast<LPVOID>(targetFunctionAddress))) {
        std::cerr << "[MsgSendHook] Hook enabling failed via HookManager." << std::endl;
        hookedMsgSendAddress = 0;
        return false;
    }

    return true;
}

// The explicit disable-remove sequence is employed to avoid potential disconnects or packet loss,
// ensuring the hook is cleanly removed before the shutdown process completes.
void CleanupMsgSendHook() {
    if (hookedMsgSendAddress != 0) {
        // Disable the hook to immediately halt message interception.
        if (MH_DisableHook(reinterpret_cast<LPVOID>(hookedMsgSendAddress)) != MH_OK) {
            std::cerr << "[MsgSendHook] Failed to disable hook." << std::endl;
        }

        // Remove the hook to finalize cleanup and restore original function behavior.
        if (MH_RemoveHook(reinterpret_cast<LPVOID>(hookedMsgSendAddress)) != MH_OK) {
            std::cerr << "[MsgSendHook] Failed to remove hook." << std::endl;
        }

        // Reset local state variables.
        hookedMsgSendAddress = 0;
        originalMsgSend = nullptr;
        std::cout << "[MsgSendHook] Cleaned up." << std::endl;
    }
}
