#pragma once

#include <string>
#include <vector>

namespace kx::SkillTrace {

struct LiveSkillbarRow {
    std::string hotkeyName;
    int slotIndex = -1;
    int apiId = 0;
    int confidence = 0;
    std::string skillName;
    std::string source;
};

bool Initialize();
void Shutdown();

std::string GetStatusLine();
std::string GetLastTraceSummary();
std::string GetProbeStatusLine();
std::string GetLastProbeSummary();
bool StartSkillbarProbe(const char* scenarioName);
void CancelSkillbarProbe(const char* reason);
std::vector<LiveSkillbarRow> GetLiveSkillbarRows();

}
