#include "AppState.h"
#include <map>
#include <atomic>
#include <iostream>
#include <windows.h>
#include "PacketHeaders.h"
#include "PacketData.h" // Include PacketData for PacketDirection enum used below

namespace kx {
    const char* GetSkillHotkeyName(int hotkeyId) {
        switch (static_cast<SkillHotkeyId>(hotkeyId)) {
        case SkillHotkeyId::Weapon1: return "Weapon1(1)";
        case SkillHotkeyId::Weapon2: return "Weapon2(2)";
        case SkillHotkeyId::Weapon3: return "Weapon3(3)";
        case SkillHotkeyId::Weapon4: return "Weapon4(4)";
        case SkillHotkeyId::Weapon5: return "Weapon5(5)";
        case SkillHotkeyId::Healing: return "Healing(6)";
        case SkillHotkeyId::Utility1: return "Utility1(Z)";
        case SkillHotkeyId::Utility2: return "Utility2(X)";
        case SkillHotkeyId::Utility3: return "Utility3(C)";
        case SkillHotkeyId::Elite: return "Elite(Q)";
        case SkillHotkeyId::SpecialAction: return "SpecialAction(E)";
        case SkillHotkeyId::Profession1: return "Profession1(F1)";
        case SkillHotkeyId::Profession2: return "Profession2(F2)";
        case SkillHotkeyId::Profession3: return "Profession3(F3)";
        case SkillHotkeyId::Profession4: return "Profession4(F4)";
        case SkillHotkeyId::Profession5: return "Profession5(F5)";
        case SkillHotkeyId::Profession6: return "Profession6(F6)";
        case SkillHotkeyId::Profession7: return "Profession7(F7)";
        case SkillHotkeyId::None:
        default:
            return "None";
        }
    }

	// --- Status Information ---
	HookStatus g_presentHookStatus = HookStatus::Unknown;
	HookStatus g_msgSendHookStatus = HookStatus::Unknown;
	HookStatus g_msgRecvHookStatus = HookStatus::Unknown;
	uintptr_t g_msgSendAddress = 0;
	uintptr_t g_msgRecvAddress = 0;

	// --- UI State ---
	std::atomic<bool> g_isInspectorWindowOpen = true;
	std::atomic<bool> g_showInspectorWindow = true;
    std::atomic<bool> g_capturePaused = false;
	std::atomic<bool> g_globalPauseMode = false;
	std::atomic<bool> g_unloadRequested = false;

    bool IsUnloadHotkeyVk(unsigned int vk) {
        return vk == VK_DELETE || vk == VK_END || vk == VK_DECIMAL;
    }

    void RequestUnload(const char* reason) {
        if (!g_unloadRequested.exchange(true, std::memory_order_acq_rel)) {
            g_isInspectorWindowOpen.store(false, std::memory_order_release);
            std::cout << "[Main] Unload requested";
            if (reason && reason[0] != '\0') {
                std::cout << " reason=" << reason;
            }
            std::cout << std::endl;
        }
    }

	// --- Filtering State ---
	// Header Filtering
	FilterMode g_packetFilterMode = FilterMode::ShowAll;

	// Initialize filter maps (typically populated by UI setup)
	std::map<std::pair<PacketDirection, uint16_t>, bool> g_packetHeaderFilterSelection;
	std::map<InternalPacketType, bool> g_specialPacketFilterSelection;

	// Direction Filtering
	DirectionFilterMode g_packetDirectionFilterMode = DirectionFilterMode::ShowAll; // Default to showing all directions


	// --- Shutdown Synchronization ---
	std::atomic<bool> g_isShuttingDown = false;

	// --- Skill Correlation State ---
	std::atomic<int> g_lastSkillHotkey = -1;
	std::atomic<unsigned long long> g_lastSkillHotkeyTickMs = 0;
	std::atomic<uint16_t> g_lastOutgoingHeader = 0;
	std::atomic<int> g_lastOutgoingSize = 0;
	std::atomic<int> g_lastOutgoingBufferState = 0;
	std::atomic<unsigned long long> g_lastOutgoingTickMs = 0;
	std::mutex g_lastSkillUsePacketMutex;
	SkillUsePacketCapture g_lastSkillUsePacket = {};

} // namespace kx
