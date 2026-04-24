#define NOMINMAX
#include "SkillDb.h"

#include <windows.h>
#include <shellapi.h>
#include <winhttp.h>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iomanip>
#include <optional>
#include <sstream>
#include <string>

#pragma comment(lib, "winhttp.lib")

extern HINSTANCE dll_handle;

namespace kx::SkillDb {
namespace {

std::vector<SkillConfig> g_skillTable = {
    {71968, {"tag29_val06"}, "2", "Peacekeeper", SkillCdType::STANDARD, 0, 1, 0, 0, 1500, false},
    {71987, {"tag29_val07"}, "3", "Symbol of Ignition"},
    {9104, {"tag29_val08"}, "4", "Zealot's Flame", SkillCdType::CHARGE_PLUS_CD, 0, 2, 0, 500, 0, false},
    {9088, {"tag29_val09"}, "5", "Cleansing Flame"},
    {41714, {"tag29_val00"}, "6", "Mantra of Solace", SkillCdType::MULTI_CHARGE, 0, 3, 0, 0, 0, true},
    {9153, {"tag29_val01"}, "Z", "Stand Your Ground!", SkillCdType::STANDARD, 0, 1, 0, 0, 0, true},
    {9253, {"tag25_val9a"}, "X", "Hallowed Ground", SkillCdType::STANDARD, 0, 1, 0, 0, 0, true},
    {9128, {"tag25_valc5"}, "C", "Sanctuary", SkillCdType::STANDARD, 0, 1, 0, 0, 0, true},
    {43357, {"tag29_val04"}, "Q", "Mantra of Liberation", SkillCdType::MULTI_CHARGE, 0, 3, 0, 0, 0, false},
};

std::string g_apiStatusLine = "[API: not loaded]";
std::string g_dbStatusLine = "[DB: no profile loaded]";
std::string g_currentProfilePath;
bool g_initialized = false;

struct ApiCacheMeta {
    std::string cachedAt;
    int gameBuild = 0;
};

std::string GetDllDirectory() {
    char path[MAX_PATH] = {};
    if (!GetModuleFileNameA(dll_handle, path, MAX_PATH)) return ".";
    std::string p = path;
    size_t slash = p.find_last_of("\\/");
    return slash == std::string::npos ? "." : p.substr(0, slash);
}

std::string GetSkillDbDirectory() { return GetDllDirectory() + "\\skill-db"; }
std::string GetBuildProfilesDirectory() { return GetSkillDbDirectory() + "\\build_profiles"; }
std::string GetApiCachePath() { return GetSkillDbDirectory() + "\\api_cache.json"; }
std::string GetOverridesPath() { return GetSkillDbDirectory() + "\\overrides.json"; }
std::string GetDefaultProfilePath() { return GetBuildProfilesDirectory() + "\\firebrand_axe_torch.json"; }

void EnsureSkillDbDirectories() {
    CreateDirectoryA(GetSkillDbDirectory().c_str(), nullptr);
    CreateDirectoryA(GetBuildProfilesDirectory().c_str(), nullptr);
}

std::string BuildTimestampForLog() {
    SYSTEMTIME st = {};
    GetLocalTime(&st);
    char ts[32];
    snprintf(ts, sizeof(ts), "%02u:%02u:%02u.%03u", st.wHour, st.wMinute, st.wSecond, st.wMilliseconds);
    return ts;
}

std::string BuildIsoUtcTimestamp() {
    SYSTEMTIME st = {};
    GetSystemTime(&st);
    char ts[32];
    snprintf(ts, sizeof(ts), "%04u-%02u-%02uT%02u:%02u:%02uZ", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
    return ts;
}

std::string GetCurrentDateFileName() {
    SYSTEMTIME st = {};
    GetLocalTime(&st);
    char fileName[32];
    snprintf(fileName, sizeof(fileName), "%04u%02u%02u.log", st.wYear, st.wMonth, st.wDay);
    return fileName;
}

bool ReadTextFile(const std::string& path, std::string& out) {
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

bool WriteTextFile(const std::string& path, const std::string& text) {
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (!out) return false;
    out << text;
    return static_cast<bool>(out);
}

void AppendLogLine(const std::string& line) {
    const std::string dir = GetDllDirectory() + "\\cd-log";
    CreateDirectoryA(dir.c_str(), nullptr);
    std::ofstream out(dir + "\\" + GetCurrentDateFileName(), std::ios::app);
    if (out) out << line << "\n";
}

void LogApiLine(const std::string& text) {
    AppendLogLine("[" + BuildTimestampForLog() + "] [API     ] " + text);
}

std::wstring ToWide(const std::string& s) {
    if (s.empty()) return {};
    int n = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring out(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), out.data(), n);
    return out;
}

size_t SkipWs(const std::string& s, size_t p) {
    while (p < s.size() && std::isspace(static_cast<unsigned char>(s[p]))) ++p;
    return p;
}

size_t FindStringEnd(const std::string& s, size_t p) {
    bool esc = false;
    for (size_t i = p; i < s.size(); ++i) {
        if (esc) {
            esc = false;
            continue;
        }
        if (s[i] == '\\') {
            esc = true;
            continue;
        }
        if (s[i] == '"') return i;
    }
    return std::string::npos;
}

size_t FindMatch(const std::string& s, size_t p, char openCh, char closeCh) {
    int depth = 0;
    bool inStr = false;
    bool esc = false;
    for (size_t i = p; i < s.size(); ++i) {
        char c = s[i];
        if (inStr) {
            if (esc) esc = false;
            else if (c == '\\') esc = true;
            else if (c == '"') inStr = false;
            continue;
        }
        if (c == '"') {
            inStr = true;
            continue;
        }
        if (c == openCh) ++depth;
        else if (c == closeCh && --depth == 0) return i;
    }
    return std::string::npos;
}

std::optional<int> ExtractIntField(const std::string& s, const std::string& key) {
    size_t p = s.find("\"" + key + "\"");
    if (p == std::string::npos) return std::nullopt;
    p = s.find(':', p);
    if (p == std::string::npos) return std::nullopt;
    p = SkipWs(s, p + 1);
    size_t e = p;
    if (e < s.size() && (s[e] == '-' || std::isdigit(static_cast<unsigned char>(s[e])))) {
        ++e;
        while (e < s.size() && std::isdigit(static_cast<unsigned char>(s[e]))) ++e;
        return std::stoi(s.substr(p, e - p));
    }
    return std::nullopt;
}

std::optional<std::string> ExtractStringField(const std::string& s, const std::string& key) {
    size_t p = s.find("\"" + key + "\"");
    if (p == std::string::npos) return std::nullopt;
    p = s.find(':', p);
    if (p == std::string::npos) return std::nullopt;
    p = SkipWs(s, p + 1);
    if (p >= s.size() || s[p] != '"') return std::nullopt;
    size_t e = FindStringEnd(s, p + 1);
    if (e == std::string::npos) return std::nullopt;
    return s.substr(p + 1, e - p - 1);
}

std::optional<std::string> ExtractObjectForKey(const std::string& s, const std::string& key, char openCh, char closeCh) {
    size_t p = s.find("\"" + key + "\"");
    if (p == std::string::npos) return std::nullopt;
    p = s.find(':', p);
    if (p == std::string::npos) return std::nullopt;
    p = SkipWs(s, p + 1);
    if (p >= s.size() || s[p] != openCh) return std::nullopt;
    size_t e = FindMatch(s, p, openCh, closeCh);
    if (e == std::string::npos) return std::nullopt;
    return s.substr(p, e - p + 1);
}

std::vector<std::string> ExtractObjects(const std::string& arr) {
    std::vector<std::string> out;
    size_t p = 0;
    while ((p = arr.find('{', p)) != std::string::npos) {
        size_t e = FindMatch(arr, p, '{', '}');
        if (e == std::string::npos) break;
        out.push_back(arr.substr(p, e - p + 1));
        p = e + 1;
    }
    return out;
}

std::optional<int> ExtractRechargeFromFacts(const std::string& obj) {
    auto facts = ExtractObjectForKey(obj, "facts", '[', ']');
    if (!facts.has_value()) return std::nullopt;
    for (const auto& fact : ExtractObjects(*facts)) {
        auto type = ExtractStringField(fact, "type");
        if (type.has_value() && *type == "Recharge") {
            auto v = ExtractIntField(fact, "value");
            if (v.has_value()) return *v * 1000;
        }
    }
    return std::nullopt;
}

bool ParseIsoUtc(const std::string& iso, SYSTEMTIME& st) {
    if (iso.size() < 20) return false;
    st = {};
    st.wYear = static_cast<WORD>(std::stoi(iso.substr(0, 4)));
    st.wMonth = static_cast<WORD>(std::stoi(iso.substr(5, 2)));
    st.wDay = static_cast<WORD>(std::stoi(iso.substr(8, 2)));
    st.wHour = static_cast<WORD>(std::stoi(iso.substr(11, 2)));
    st.wMinute = static_cast<WORD>(std::stoi(iso.substr(14, 2)));
    st.wSecond = static_cast<WORD>(std::stoi(iso.substr(17, 2)));
    return true;
}

double AgeInDays(const std::string& isoUtc) {
    SYSTEMTIME then = {}, now = {};
    if (!ParseIsoUtc(isoUtc, then)) return 9999.0;
    FILETIME ftThen = {}, ftNow = {};
    GetSystemTime(&now);
    if (!SystemTimeToFileTime(&then, &ftThen) || !SystemTimeToFileTime(&now, &ftNow)) return 9999.0;
    ULARGE_INTEGER a{}, b{};
    a.LowPart = ftThen.dwLowDateTime;
    a.HighPart = ftThen.dwHighDateTime;
    b.LowPart = ftNow.dwLowDateTime;
    b.HighPart = ftNow.dwHighDateTime;
    const double diff100ns = static_cast<double>(b.QuadPart - a.QuadPart);
    return diff100ns / 10000000.0 / 86400.0;
}

std::string CdTypeToString(SkillCdType t) {
    switch (t) {
    case SkillCdType::STANDARD: return "STANDARD";
    case SkillCdType::MULTI_CHARGE: return "MULTI_CHARGE";
    case SkillCdType::CHARGE_PLUS_CD: return "CHARGE_PLUS_CD";
    default: return "UNKNOWN";
    }
}

void ApplyApiDataToSkill(SkillConfig& skill, const std::string& obj) {
    if (auto name = ExtractStringField(obj, "name"); name.has_value() && !name->empty()) skill.skillName = *name;
    int rechargeMs = 0;
    if (auto r = ExtractIntField(obj, "recharge"); r.has_value()) rechargeMs = *r * 1000;
    if (auto fr = ExtractRechargeFromFacts(obj); fr.has_value()) rechargeMs = *fr;
    if (rechargeMs > 0) skill.rechargeMs = rechargeMs;

    if (auto ammo = ExtractObjectForKey(obj, "ammo", '{', '}'); ammo.has_value()) {
        if (auto c = ExtractIntField(*ammo, "count"); c.has_value() && *c > 0) skill.maxCharges = *c;
        if (auto d = ExtractIntField(*ammo, "recharge_duration"); d.has_value() && *d > 0) skill.ammoRechargeMs = *d;
    }

    if (skill.apiId == 9104) {
        skill.cdType = SkillCdType::CHARGE_PLUS_CD;
        skill.maxCharges = std::max(skill.maxCharges, 2);
        if (skill.ammoRechargeMs <= 0) skill.ammoRechargeMs = skill.rechargeMs;
        if (skill.castCdMs <= 0) skill.castCdMs = 500;
    } else if (skill.apiId == 41714 || skill.apiId == 43357) {
        skill.cdType = SkillCdType::MULTI_CHARGE;
        skill.maxCharges = std::max(skill.maxCharges, 3);
        if (skill.ammoRechargeMs <= 0) skill.ammoRechargeMs = skill.rechargeMs;
    } else {
        skill.cdType = SkillCdType::STANDARD;
        skill.maxCharges = 1;
        skill.ammoRechargeMs = 0;
        skill.castCdMs = 0;
    }
}

std::string JsonEscape(const std::string& s) {
    std::string out;
    for (char ch : s) {
        switch (ch) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': break;
        default: out += ch; break;
        }
    }
    return out;
}

ApiCacheMeta LoadApiCacheMeta() {
    ApiCacheMeta meta{};
    std::string text;
    if (!ReadTextFile(GetApiCachePath(), text)) return meta;
    if (auto t = ExtractStringField(text, "cached_at"); t.has_value()) meta.cachedAt = *t;
    if (auto b = ExtractIntField(text, "game_build"); b.has_value()) meta.gameBuild = *b;
    return meta;
}

std::string HttpGet(const std::wstring& host, const std::wstring& path) {
    std::string body;
    HINTERNET hOpen = WinHttpOpen(L"KXPacketInspector/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hOpen) return body;
    HINTERNET hConn = WinHttpConnect(hOpen, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConn) {
        WinHttpCloseHandle(hOpen);
        return body;
    }
    HINTERNET hReq = WinHttpOpenRequest(hConn, L"GET", path.c_str(), nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, WINHTTP_FLAG_SECURE);
    if (!hReq) {
        WinHttpCloseHandle(hConn);
        WinHttpCloseHandle(hOpen);
        return body;
    }
    BOOL ok = WinHttpSendRequest(hReq, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(hReq, nullptr);
    while (ok) {
        DWORD avail = 0;
        DWORD read = 0;
        if (!WinHttpQueryDataAvailable(hReq, &avail) || !avail) break;
        std::string chunk(static_cast<size_t>(avail), '\0');
        if (!WinHttpReadData(hReq, chunk.data(), avail, &read) || !read) break;
        chunk.resize(static_cast<size_t>(read));
        body += chunk;
    }
    WinHttpCloseHandle(hReq);
    WinHttpCloseHandle(hConn);
    WinHttpCloseHandle(hOpen);
    return body;
}

int GetCurrentGameBuild() {
    const std::string json = HttpGet(L"api.guildwars2.com", L"/v2/build");
    auto build = ExtractIntField(json, "id");
    return build.value_or(0);
}

bool ShouldRefreshCache(const ApiCacheMeta& meta, bool forceRefresh) {
    if (forceRefresh) return true;
    if (meta.cachedAt.empty()) return true;
    if (AgeInDays(meta.cachedAt) > 7.0) return true;
    const int currentBuild = GetCurrentGameBuild();
    if (meta.gameBuild != 0 && currentBuild != 0 && meta.gameBuild != currentBuild) return true;
    return false;
}

void SaveApiCacheFile(int gameBuild) {
    std::ostringstream oss;
    oss << "{\n"
        << "  \"cached_at\": \"" << BuildIsoUtcTimestamp() << "\",\n"
        << "  \"game_build\": " << gameBuild << ",\n"
        << "  \"skills\": {\n";
    for (size_t i = 0; i < g_skillTable.size(); ++i) {
        const auto& s = g_skillTable[i];
        oss << "    \"" << s.apiId << "\": {\n"
            << "      \"name\": \"" << JsonEscape(s.skillName) << "\",\n"
            << "      \"recharge\": " << (s.rechargeMs / 1000) << ",\n";
        if (s.maxCharges > 1) {
            oss << "      \"ammo\": { \"count\": " << s.maxCharges << ", \"recharge_duration\": " << s.ammoRechargeMs << " },\n";
        } else {
            oss << "      \"ammo\": null,\n";
        }
        oss << "      \"slot\": \"" << JsonEscape(s.hotkeyName) << "\"\n"
            << "    }" << (i + 1 < g_skillTable.size() ? "," : "") << "\n";
    }
    oss << "  }\n}\n";
    WriteTextFile(GetApiCachePath(), oss.str());
}

void WriteDefaultOverridesFileIfMissing() {
    std::string existing;
    if (ReadTextFile(GetOverridesPath(), existing)) return;
    const char* text =
        "{\n"
        "  \"last_modified\": \"2026-04-16T11:25:00Z\",\n"
        "  \"overrides\": {\n"
        "    \"71968\": {\n"
        "      \"skill_name\": \"Peacekeeper\",\n"
        "      \"recharge_override_ms\": 6000,\n"
        "      \"cooldown_start_delay_ms\": 1500,\n"
        "      \"note\": \"API returns recharge=0, actual CD is 6s. Verified in-game 2026-04-16.\",\n"
        "      \"api_data_wrong\": true\n"
        "    },\n"
        "    \"9104\": {\n"
        "      \"skill_name\": \"Zealot's Flame\",\n"
        "      \"recharge_override_ms\": 12000,\n"
        "      \"cd_type_override\": \"CHARGE_PLUS_CD\",\n"
        "      \"cast_cd_ms\": 500,\n"
        "      \"note\": \"Radiant Fire trait: -20% recharge (15s -> 12s). Hidden 0.5s cast CD after use.\",\n"
        "      \"trait_modified\": true\n"
        "    },\n"
        "    \"9088\": {\n"
        "      \"skill_name\": \"Cleansing Flame\",\n"
        "      \"recharge_override_ms\": 12000,\n"
        "      \"note\": \"Radiant Fire trait: -20% recharge (15s -> 12s).\",\n"
        "      \"trait_modified\": true\n"
        "    }\n"
        "  },\n"
        "  \"special_mechanics\": {\n"
        "    \"weapon_swap\": {\n"
        "      \"cd_ms\": 10000,\n"
        "      \"archive_key\": \"UNKNOWN_TBD\",\n"
        "      \"note\": \"Weapon swap has 10s CD. Archive key not yet confirmed.\"\n"
        "    }\n"
        "  }\n"
        "}\n";
    WriteTextFile(GetOverridesPath(), text);
}

void ApplyOverridesFromFile() {
    std::string text;
    if (!ReadTextFile(GetOverridesPath(), text)) return;
    for (auto& skill : g_skillTable) {
        const std::string key = "\"" + std::to_string(skill.apiId) + "\"";
        size_t pos = text.find(key);
        if (pos == std::string::npos) continue;
        pos = text.find('{', pos);
        if (pos == std::string::npos) continue;
        const size_t end = FindMatch(text, pos, '{', '}');
        if (end == std::string::npos) continue;
        const std::string obj = text.substr(pos, end - pos + 1);
        if (auto name = ExtractStringField(obj, "skill_name"); name.has_value()) skill.skillName = *name;
        if (auto recharge = ExtractIntField(obj, "recharge_override_ms"); recharge.has_value()) {
            const int apiRecharge = skill.rechargeMs;
            skill.rechargeMs = *recharge;
            AppendLogLine("[" + BuildTimestampForLog() + "] [OVERRIDE] " + skill.skillName + ": api_recharge=" + std::to_string(apiRecharge) + " -> override=" + std::to_string(*recharge) + "ms");
        }
        if (auto delay = ExtractIntField(obj, "cooldown_start_delay_ms"); delay.has_value()) {
            skill.cooldownStartDelayMs = *delay;
            AppendLogLine("[" + BuildTimestampForLog() + "] [OVERRIDE] " + skill.skillName + ": cooldown_start_delay=" + std::to_string(*delay) + "ms");
        }
        if (auto cast = ExtractIntField(obj, "cast_cd_ms"); cast.has_value()) skill.castCdMs = *cast;
        if (auto type = ExtractStringField(obj, "cd_type_override"); type.has_value()) {
            if (*type == "STANDARD") skill.cdType = SkillCdType::STANDARD;
            else if (*type == "MULTI_CHARGE") skill.cdType = SkillCdType::MULTI_CHARGE;
            else if (*type == "CHARGE_PLUS_CD") skill.cdType = SkillCdType::CHARGE_PLUS_CD;
        }
    }
}

void SaveCurrentProfileFile(const std::string& path) {
    std::ostringstream oss;
    oss << "{\n"
        << "  \"profile_name\": \"Firebrand Axe/Torch - Support\",\n"
        << "  \"created_at\": \"" << BuildIsoUtcTimestamp() << "\",\n"
        << "  \"profession\": \"Guardian\",\n"
        << "  \"elite_spec\": \"Firebrand\",\n"
        << "  \"skills\": [\n";
    for (size_t i = 0; i < g_skillTable.size(); ++i) {
        const auto& s = g_skillTable[i];
        oss << "    { \"hotkey\": \"" << s.hotkeyName
            << "\", \"api_id\": " << s.apiId
            << ", \"archive_key\": \"" << (s.archiveKeys.empty() ? "" : s.archiveKeys.front())
            << "\", \"skill_name\": \"" << JsonEscape(s.skillName)
            << "\", \"cd_type\": \"" << CdTypeToString(s.cdType)
            << "\", \"recharge_ms\": " << s.rechargeMs
            << ", \"ammo_count\": " << s.maxCharges
            << ", \"ammo_recharge_ms\": " << s.ammoRechargeMs
            << ", \"cast_cd_ms\": " << s.castCdMs
            << ", \"cooldown_start_delay_ms\": " << s.cooldownStartDelayMs
            << ", \"acr_excluded\": " << (s.acrExcluded ? "true" : "false") << " }"
            << (i + 1 < g_skillTable.size() ? "," : "") << "\n";
    }
    oss << "  ]\n}\n";
    WriteTextFile(path, oss.str());
}

void LoadProfileFromFile(const std::string& path) {
    std::string text;
    if (!ReadTextFile(path, text)) return;
    for (const auto& obj : ExtractObjects(text)) {
        auto id = ExtractIntField(obj, "api_id");
        if (!id.has_value()) continue;
        for (auto& skill : g_skillTable) {
            if (skill.apiId != *id) continue;
            if (auto hotkey = ExtractStringField(obj, "hotkey"); hotkey.has_value()) skill.hotkeyName = *hotkey;
            if (auto archive = ExtractStringField(obj, "archive_key"); archive.has_value() && !archive->empty()) skill.archiveKeys = { *archive };
            if (auto name = ExtractStringField(obj, "skill_name"); name.has_value()) skill.skillName = *name;
            if (auto recharge = ExtractIntField(obj, "recharge_ms"); recharge.has_value()) skill.rechargeMs = *recharge;
            if (auto ammo = ExtractIntField(obj, "ammo_count"); ammo.has_value() && *ammo > 0) skill.maxCharges = *ammo;
            if (auto ammoRecharge = ExtractIntField(obj, "ammo_recharge_ms"); ammoRecharge.has_value()) skill.ammoRechargeMs = *ammoRecharge;
            if (auto castCd = ExtractIntField(obj, "cast_cd_ms"); castCd.has_value()) skill.castCdMs = *castCd;
            if (auto delay = ExtractIntField(obj, "cooldown_start_delay_ms"); delay.has_value()) skill.cooldownStartDelayMs = *delay;
            if (auto type = ExtractStringField(obj, "cd_type"); type.has_value()) {
                if (*type == "STANDARD") skill.cdType = SkillCdType::STANDARD;
                else if (*type == "MULTI_CHARGE") skill.cdType = SkillCdType::MULTI_CHARGE;
                else if (*type == "CHARGE_PLUS_CD") skill.cdType = SkillCdType::CHARGE_PLUS_CD;
            }
            break;
        }
    }
}

void LoadApiCacheFromFile(const std::string& path) {
    std::string text;
    if (!ReadTextFile(path, text)) return;
    for (auto& skill : g_skillTable) {
        const std::string key = "\"" + std::to_string(skill.apiId) + "\"";
        size_t pos = text.find(key);
        if (pos == std::string::npos) continue;
        pos = text.find('{', pos);
        if (pos == std::string::npos) continue;
        const size_t end = FindMatch(text, pos, '{', '}');
        if (end == std::string::npos) continue;
        const std::string obj = text.substr(pos, end - pos + 1);
        ApplyApiDataToSkill(skill, obj);
    }
}

bool LoadSkillDataFromApi(bool forceRefresh = false) {
    std::ostringstream path;
    path << "/v2/skills?ids=";
    for (size_t i = 0; i < g_skillTable.size(); ++i) {
        if (i) path << ",";
        path << g_skillTable[i].apiId;
    }

    const ApiCacheMeta meta = LoadApiCacheMeta();
    if (!forceRefresh && !ShouldRefreshCache(meta, false)) {
        LoadApiCacheFromFile(GetApiCachePath());
        g_apiStatusLine = "[API: cache loaded]";
        LogApiLine("loaded from api_cache.json");
        return true;
    }

    const std::string json = HttpGet(L"api.guildwars2.com", ToWide(path.str()));
    if (json.empty()) {
        g_apiStatusLine = "[API: load failed, using cache/fallback]";
        LogApiLine("load failed; using cache/fallback data");
        LoadApiCacheFromFile(GetApiCachePath());
        return false;
    }

    int loaded = 0;
    for (const auto& obj : ExtractObjects(json)) {
        auto id = ExtractIntField(obj, "id");
        if (!id.has_value()) continue;
        for (auto& skill : g_skillTable) {
            if (skill.apiId == *id) {
                ApplyApiDataToSkill(skill, obj);
                ++loaded;
                break;
            }
        }
    }

    std::ostringstream status;
    status << "[API: " << loaded << " skills loaded" << (loaded == static_cast<int>(g_skillTable.size()) ? " \xE2\x9C\x93" : "") << "]";
    g_apiStatusLine = status.str();
    LogApiLine("Loaded " + std::to_string(loaded) + " skills OK");
    for (const auto& skill : g_skillTable) {
        std::ostringstream line;
        line << skill.skillName << " (" << skill.hotkeyName << ") | api_id=" << skill.apiId
             << " | type=" << CdTypeToString(skill.cdType)
             << " | recharge_ms=" << skill.rechargeMs
             << " | max_charges=" << skill.maxCharges
             << " | ammo_recharge_ms=" << skill.ammoRechargeMs;
        LogApiLine(line.str());
    }
    SaveApiCacheFile(GetCurrentGameBuild());
    return loaded > 0;
}

} // namespace

bool Initialize() {
    if (g_initialized) return true;
    EnsureSkillDbDirectories();
    WriteDefaultOverridesFileIfMissing();
    g_apiStatusLine = "[API: loading current skill config...]";
    LoadSkillDataFromApi(false);
    ApplyOverridesFromFile();
    g_currentProfilePath = GetDefaultProfilePath();

    std::string profileText;
    if (ReadTextFile(g_currentProfilePath, profileText)) {
        LoadProfileFromFile(g_currentProfilePath);
        g_dbStatusLine = "[DB: loaded build_profiles/firebrand_axe_torch.json]";
    } else {
        SaveCurrentProfileFile(g_currentProfilePath);
        g_dbStatusLine = "[DB: created build_profiles/firebrand_axe_torch.json]";
    }

    g_initialized = true;
    return true;
}

const std::vector<SkillConfig>& GetSkillTable() {
    return g_skillTable;
}

std::string GetApiStatusLine() {
    return g_apiStatusLine;
}

std::string GetDbStatusLine() {
    return g_dbStatusLine;
}

bool ReloadBuildData() {
    ApplyOverridesFromFile();
    if (!g_currentProfilePath.empty()) {
        LoadProfileFromFile(g_currentProfilePath);
        g_dbStatusLine = "[DB: reloaded profile + overrides]";
    } else {
        g_dbStatusLine = "[DB: overrides reloaded]";
    }
    return true;
}

bool RefreshApiData() {
    const bool ok = LoadSkillDataFromApi(true);
    ApplyOverridesFromFile();
    if (!g_currentProfilePath.empty()) {
        SaveCurrentProfileFile(g_currentProfilePath);
    }
    g_dbStatusLine = ok ? "[DB: api cache refreshed]" : "[DB: api refresh failed; fallback active]";
    return ok;
}

bool SaveCurrentProfile() {
    EnsureSkillDbDirectories();
    if (g_currentProfilePath.empty()) g_currentProfilePath = GetDefaultProfilePath();
    SaveCurrentProfileFile(g_currentProfilePath);
    g_dbStatusLine = "[DB: profile saved]";
    return true;
}

bool OpenOverridesFile() {
    EnsureSkillDbDirectories();
    WriteDefaultOverridesFileIfMissing();
    const auto result = reinterpret_cast<intptr_t>(ShellExecuteA(nullptr, "open", "notepad.exe", GetOverridesPath().c_str(), nullptr, SW_SHOWNORMAL));
    return result > 32;
}

std::string ResolveSkillName(int apiId) {
    for (const auto& skill : g_skillTable) {
        if (skill.apiId == apiId && !skill.skillName.empty()) {
            return skill.skillName;
        }
    }

    std::ostringstream oss;
    oss << "Skill#" << apiId;
    return oss.str();
}

} // namespace kx::SkillDb
