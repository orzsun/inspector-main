#include <iostream>
#include <cstdio> // Required for fclose
#include <cstdint>
#include <optional>
#include <string>
#include <windows.h>
#include "Console.h"
#include "Hooks.h"
#include "AppState.h"   // Include for g_isInspectorWindowOpen, g_isShuttingDown
#include "SkillMemoryReader.h"
#include "SkillTraceCoordinator.h"
#include "SkillCooldownTracker.h"

HINSTANCE dll_handle;

namespace {
std::optional<kx::SkillCooldown::PtrWriteHitKind> MapBridgeWatchKind(uint32_t kind) {
    switch (kind) {
    case 1:
        return kx::SkillCooldown::PtrWriteHitKind::SLOT;
    case 2:
        return kx::SkillCooldown::PtrWriteHitKind::PTR40;
    case 3:
        return kx::SkillCooldown::PtrWriteHitKind::PTR48;
    case 4:
        return kx::SkillCooldown::PtrWriteHitKind::PTR60;
    case 5:
        return kx::SkillCooldown::PtrWriteHitKind::N48;
    default:
        return std::nullopt;
    }
}

bool IsCurrentProcessForegroundWindow() {
    const HWND foreground = GetForegroundWindow();
    if (!foreground) {
        return false;
    }

    DWORD foregroundPid = 0;
    GetWindowThreadProcessId(foreground, &foregroundPid);
    return foregroundPid != 0 && foregroundPid == GetCurrentProcessId();
}

bool IsUnloadHotkeyPressed() {
    struct HotkeyState {
        int vk;
        bool wasDown;
    };
    static HotkeyState hotkeys[] = {
        { VK_DELETE, false },
        { VK_END, false },
        { VK_DECIMAL, false }, // Numpad Del on many layouts
    };

    if (kx::g_unloadRequested.load(std::memory_order_acquire)) {
        return true;
    }

    if (!IsCurrentProcessForegroundWindow()) {
        for (auto& hotkey : hotkeys) {
            hotkey.wasDown = false;
        }
        return false;
    }

    for (auto& hotkey : hotkeys) {
        const SHORT keyState = GetAsyncKeyState(hotkey.vk);
        const bool isDown = (keyState & 0x8000) != 0;
        const bool pressed = (keyState & 0x0001) != 0 || (isDown && !hotkey.wasDown);
        hotkey.wasDown = isDown;
        if (pressed) {
            kx::RequestUnload("hotkey poll");
            return true;
        }
    }
    return false;
}

void PollSkillHotkeys() {
    static const struct {
        int vk;
        int hotkeyId;
    } kSkillKeys[] = {
        { '1', static_cast<int>(kx::SkillHotkeyId::Weapon1) },
        { '2', static_cast<int>(kx::SkillHotkeyId::Weapon2) },
        { '3', static_cast<int>(kx::SkillHotkeyId::Weapon3) },
        { '4', static_cast<int>(kx::SkillHotkeyId::Weapon4) },
        { '5', static_cast<int>(kx::SkillHotkeyId::Weapon5) },
        { '6', static_cast<int>(kx::SkillHotkeyId::Healing) },
        { 'Z', static_cast<int>(kx::SkillHotkeyId::Utility1) },
        { 'X', static_cast<int>(kx::SkillHotkeyId::Utility2) },
        { 'C', static_cast<int>(kx::SkillHotkeyId::Utility3) },
        { 'Q', static_cast<int>(kx::SkillHotkeyId::Elite) },
        { 'E', static_cast<int>(kx::SkillHotkeyId::SpecialAction) },
        { VK_F1, static_cast<int>(kx::SkillHotkeyId::Profession1) },
        { VK_F2, static_cast<int>(kx::SkillHotkeyId::Profession2) },
        { VK_F3, static_cast<int>(kx::SkillHotkeyId::Profession3) },
        { VK_F4, static_cast<int>(kx::SkillHotkeyId::Profession4) },
        { VK_F5, static_cast<int>(kx::SkillHotkeyId::Profession5) },
        { VK_F6, static_cast<int>(kx::SkillHotkeyId::Profession6) },
        { VK_F7, static_cast<int>(kx::SkillHotkeyId::Profession7) },
    };

    static bool wasDown[std::size(kSkillKeys)] = {};

    if (!IsCurrentProcessForegroundWindow()) {
        for (size_t i = 0; i < std::size(kSkillKeys); ++i) {
            wasDown[i] = false;
        }
        return;
    }

    for (size_t i = 0; i < std::size(kSkillKeys); ++i) {
        const bool isDown = (GetAsyncKeyState(kSkillKeys[i].vk) & 0x8000) != 0;
        if (isDown && !wasDown[i]) {
            kx::g_lastSkillHotkey.store(kSkillKeys[i].hotkeyId, std::memory_order_release);
            kx::g_lastSkillHotkeyTickMs.store(GetTickCount64(), std::memory_order_release);
        }
        wasDown[i] = isDown;
    }
}
}

extern "C" __declspec(dllexport)
void __stdcall KxBridgeNotePtrWriteHitSimple(
    uint64_t hit_addr,
    uint64_t new_value,
    uint64_t rip,
    uint32_t kind,
    uint32_t slot,
    uint32_t phase) {
    const auto mappedKind = MapBridgeWatchKind(kind);
    if (!mappedKind.has_value()) {
        return;
    }

    kx::SkillCooldown::NotePtrWriteHit(
        mappedKind.value(),
        static_cast<std::uintptr_t>(hit_addr),
        new_value,
        static_cast<std::uintptr_t>(rip),
        slot,
        phase);
}

// Eject thread to free the DLL
DWORD WINAPI EjectThread(LPVOID lpParameter) {
#ifdef _DEBUG
    // In Debug builds, close standard streams and free the console
    if (stdout) fclose(stdout);
    if (stderr) fclose(stderr);
    if (stdin) fclose(stdin);

    if (!FreeConsole()) {
        OutputDebugStringA("kx-packet-inspector: FreeConsole() failed.\n");
    }
#endif // _DEBUG
    FreeLibraryAndExitThread(dll_handle, 0);
    return 0;
}

void InitializeFilters() {
    // Clear existing (in case of re-init?)
    kx::g_packetHeaderFilterSelection.clear();
    kx::g_specialPacketFilterSelection.clear();

    // Populate CMSG headers
    for (const auto& headerInfo : kx::GetKnownCMSGHeaders()) {
        kx::g_packetHeaderFilterSelection[std::make_pair(kx::PacketDirection::Sent, headerInfo.first)] = false; // Default unchecked
    }

    // Populate SMSG headers
    for (const auto& headerInfo : kx::GetKnownSMSGHeaders()) {
        kx::g_packetHeaderFilterSelection[std::make_pair(kx::PacketDirection::Received, headerInfo.first)] = false; // Default unchecked
    }

    // Populate Special types
    for (const auto& typeInfo : kx::GetSpecialPacketTypesForFilter()) {
        kx::g_specialPacketFilterSelection[typeInfo.first] = false; // Default unchecked
    }
    std::cout << "[Main] Filter selections initialized." << std::endl;
}

// Main function that runs in a separate thread
DWORD WINAPI MainThread(LPVOID lpParameter) {
#ifdef _DEBUG
    kx::SetupConsole(); // Only setup console in Debug builds
#endif // _DEBUG

    // *** Initialize Filters Early ***
    InitializeFilters();

    if (!kx::InitializeHooks()) {
        std::cerr << "Failed to initialize hooks." << std::endl;
        return 1;
    }

    if (!g_skillReader.init()) {
        std::cerr << "SkillMemoryReader init failed. GTH-backed skill memory features may remain unavailable." << std::endl;
    }

    kx::SkillCooldown::Initialize();

    // Main loop to keep the hook alive
    while (kx::g_isInspectorWindowOpen.load(std::memory_order_acquire) &&
           !kx::g_unloadRequested.load(std::memory_order_acquire) &&
           !IsUnloadHotkeyPressed()) {
        PollSkillHotkeys();
        Sleep(100); // Sleep to avoid busy-waiting
    }

    // Signal hooks to stop processing before actual cleanup
    kx::g_isShuttingDown.store(true, std::memory_order_release);

    // Give hooks a moment to recognize the flag before cleanup starts
	// This helps prevent calls into ImGui after it's destroyed.
    Sleep(250);

    // Cleanup hooks and ImGui
    kx::SkillCooldown::Shutdown();
    kx::CleanupHooks();
    (void)kx::Hooking::D3DRenderHook::WaitForCallbacksToDrain(2000);

    // Eject the DLL and exit the thread
    CreateThread(0, 0, EjectThread, 0, 0, 0);

    return 0;
}

// DLL entry point
BOOL APIENTRY DllMain(HMODULE hModule,
    DWORD  ul_reason_for_call,
    LPVOID lpReserved
)
{
    switch (ul_reason_for_call)
    {
    case DLL_PROCESS_ATTACH:
        dll_handle = hModule;
        DisableThreadLibraryCalls(hModule);
        CreateThread(NULL, 0, MainThread, NULL, 0, NULL);
        break;
    case DLL_PROCESS_DETACH:
        // Optional: Handle cleanup if necessary
        break;
    }
    return TRUE;
}
