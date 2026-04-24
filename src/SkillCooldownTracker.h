#pragma once

#include "PacketData.h"

#include <cstdint>
#include <string>
#include <vector>

namespace kx::SkillCooldown {

enum class SkillCdType {
    STANDARD,
    MULTI_CHARGE,
    CHARGE_PLUS_CD,
};

enum class PtrWriteHitKind {
    SLOT = 1,
    PTR40 = 2,
    PTR48 = 3,
    PTR60 = 4,
    N48 = 5,
};

struct DebugAssistState {
    bool autoMode = true;
    bool enabled = false;
    bool breakOnMatch = false;
    bool armed = false;
    std::string targetHotkey;
    std::string statusText;
    std::string lastSnapshotText;
};

struct EstimatedCooldownDisplay {
    bool available = false;
    bool estimated = true;
    std::string hotkey;
    std::string skillName;
    std::string stateText;
    int durationMs = 0;
    int remainingMs = 0;
    float progress01 = 0.0f;
    uint64_t ptr40Value = 0;
    uint64_t ptr60Value = 0;
};

bool Initialize();
void Shutdown();

std::string GetStatusLine();
std::string GetApiStatusLine();
std::string GetDbStatusLine();

bool ReloadBuildData();
bool RefreshApiData();
bool SaveCurrentProfile();
bool OpenOverridesFile();
bool IsBridgeOnlyMode();
void SetBridgeOnlyMode(bool enabled);

DebugAssistState GetDebugAssistState();
EstimatedCooldownDisplay GetEstimatedCooldownDisplay();
void SetDebugAssistEnabled(bool enabled);
void SetDebugAssistBreakOnMatch(bool enabled);
void SetDebugAssistTargetHotkey(const std::string& hotkey);
void SetDebugAssistAutoMode(bool enabled);
void ArmDebugAssistOnce();
void ClearDebugAssistSnapshot();
void NoteReceivedPacket(const PacketInfo& packet);
void NotePtrWriteHit(PtrWriteHitKind kind,
                     std::uintptr_t hitAddr,
                     uint64_t newValue,
                     std::uintptr_t rip,
                     uint32_t slot,
                     uint32_t phase);

}
