#pragma once

#include <string>
#include <vector>

namespace kx::SkillDb {

enum class SkillCdType {
    STANDARD,
    MULTI_CHARGE,
    CHARGE_PLUS_CD,
};

struct SkillConfig {
    int apiId = 0;
    std::vector<std::string> archiveKeys;
    std::string hotkeyName;
    std::string skillName;
    SkillCdType cdType = SkillCdType::STANDARD;
    int rechargeMs = 0;
    int maxCharges = 1;
    int ammoRechargeMs = 0;
    int castCdMs = 0;
    int cooldownStartDelayMs = 0;
    bool acrExcluded = false;
};

bool Initialize();
const std::vector<SkillConfig>& GetSkillTable();

std::string GetApiStatusLine();
std::string GetDbStatusLine();

bool ReloadBuildData();
bool RefreshApiData();
bool SaveCurrentProfile();
bool OpenOverridesFile();
std::string ResolveSkillName(int apiId);

} // namespace kx::SkillDb
