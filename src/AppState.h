#pragma once

#include "PacketHeaders.h" // For PacketHeader in filter map
#include <map>
#include <atomic>
#include <cstdint> // For uintptr_t
#include <array>
#include <mutex>
#include <chrono>
#include <string>
#include <sstream>
#include <iomanip>

namespace kx {
    enum class SkillHotkeyId : int {
        None = -1,
        Weapon1 = 1,
        Weapon2 = 2,
        Weapon3 = 3,
        Weapon4 = 4,
        Weapon5 = 5,
        Healing = 6,
        Utility1 = 7,
        Utility2 = 8,
        Utility3 = 9,
        Elite = 10,
        SpecialAction = 11,
        Profession1 = 12,
        Profession2 = 13,
        Profession3 = 14,
        Profession4 = 15,
        Profession5 = 16,
        Profession6 = 17,
        Profession7 = 18,
    };

    const char* GetSkillHotkeyName(int hotkeyId);

    // --- Status Information ---
    enum class HookStatus {
        Unknown,
        OK,
        Failed
    };

    extern HookStatus g_presentHookStatus;
    extern HookStatus g_msgSendHookStatus;
    extern HookStatus g_msgRecvHookStatus;
    extern uintptr_t g_msgSendAddress;
    extern uintptr_t g_msgRecvAddress;

    // --- UI State ---
    extern std::atomic<bool> g_isInspectorWindowOpen; // Controls main loop / unload trigger
    extern std::atomic<bool> g_showInspectorWindow;   // Controls GUI visibility (toggle via hotkey)
    extern std::atomic<bool> g_capturePaused;         // Controls packet capture logging
    extern std::atomic<bool> g_globalPauseMode;       // Pauses non-UI runtime processing
    extern std::atomic<bool> g_unloadRequested;       // Set by hotkeys/UI to request DLL unload

    bool IsUnloadHotkeyVk(unsigned int vk);
    void RequestUnload(const char* reason);

    // --- Filtering State ---
	// Header Filtering Mode (Include/Exclude/All) - applies to items checked below
    enum class FilterMode {
        ShowAll,
        IncludeOnly, // Show only checked items
        Exclude      // Hide checked items
    };
    extern FilterMode g_packetFilterMode;

    // Map Key: Pair of <Direction, HeaderID> -> Value: bool selected
    extern std::map<std::pair<PacketDirection, uint16_t>, bool> g_packetHeaderFilterSelection;

    // Map Key: Special Type Enum -> Value: bool selected
    extern std::map<InternalPacketType, bool> g_specialPacketFilterSelection;


    // Direction Filtering Mode (Sent/Recv/All) - applies globally
    enum class DirectionFilterMode {
        ShowAll,
        ShowSentOnly,
        ShowReceivedOnly
    };
    extern DirectionFilterMode g_packetDirectionFilterMode;

    // --- Shutdown Synchronization ---
    extern std::atomic<bool> g_isShuttingDown; // Flag to signal shutdown to hooks

    // --- Skill Correlation State ---
    extern std::atomic<int> g_lastSkillHotkey;
    extern std::atomic<unsigned long long> g_lastSkillHotkeyTickMs;
    extern std::atomic<uint16_t> g_lastOutgoingHeader;
    extern std::atomic<int> g_lastOutgoingSize;
    extern std::atomic<int> g_lastOutgoingBufferState;
    extern std::atomic<unsigned long long> g_lastOutgoingTickMs;

    struct SkillUsePacketCapture {
        bool valid = false;
        uint16_t header = 0;
        int totalBufferSize = 0;
        int packetOffset = -1;
        int packetSize = 0;
        unsigned long long tickMs = 0;
        std::array<uint8_t, 64> bytes = {};
    };

    struct SkillUseTailFields {
        bool valid = false;
        uint8_t prefix = 0;
        uint8_t tag = 0;
        uint8_t value = 0;
    };

    inline SkillUseTailFields ParseSkillUseTailFields(const SkillUsePacketCapture& capture) {
        SkillUseTailFields fields = {};
        if (!capture.valid || capture.packetSize <= 15) {
            return fields;
        }

        fields.valid = true;
        fields.prefix = capture.bytes[13];
        fields.tag = capture.bytes[14];
        fields.value = capture.bytes[15];
        return fields;
    }

    inline std::string BuildSkillUseArchiveKeyLegacy(const SkillUsePacketCapture& capture) {
        const SkillUseTailFields fields = ParseSkillUseTailFields(capture);
        std::ostringstream oss;
        oss << "tag" << std::hex << std::nouppercase << std::setw(2) << std::setfill('0')
            << static_cast<unsigned>(fields.tag)
            << "_val" << std::setw(2) << static_cast<unsigned>(fields.value);
        return oss.str();
    }

    extern std::mutex g_lastSkillUsePacketMutex;
    extern SkillUsePacketCapture g_lastSkillUsePacket;

} // namespace kx
