两轮测试信息量很足，现在可以写一个完全基于实测结论的精确 prompt：

***

**背景**

项目已具备 `CMSG_USE_SKILL (0x0017)` 捕获与稳定的 archive key 解析。经过两轮实测，已完成以下确认：

**已稳定收实的槽位映射：**

| 热键 | 技能名 | API ID | Archive Key | 状态 |
|------|--------|--------|-------------|------|
| 2 | Peacekeeper | 71968 | `tag29_val06` | ✅ 确认 |
| 3 | Symbol of Ignition | 71987 | `tag29_val07` | ✅ 确认 |
| 4 | Zealot's Flame | 9104 | `tag29_val08` | ✅ 确认 |
| 5 | Cleansing Flame | 9088 | `tag29_val09` | ✅ 确认 |
| 6 | Mantra of Solace | 41714 | `tag29_val00` | ✅ 确认 |
| Z | "Stand Your Ground!" | 9153 | `tag29_val01` | ✅ 确认 |
| Q | Mantra of Liberation | 43357 | `tag29_val04` | ✅ 确认 |
| X | Hallowed Ground | 9253 | `tag25_val4c`（冲突） | ⚠️ 需消歧 |
| C | Sanctuary | 9128 | `tag25_val4c`（冲突） | ⚠️ 需消歧 |

**已确认的包结构规律：**
- 每次技能触发前，会先到一个 `tag2a_*` **前导包**，紧随其后才是真正的 `tag29_*` 动作包
- **Tracker 只应归档 `tag29_*` 层**，`tag2a_*` 一律忽略
- X 和 C 两个技能共享 `tag25_val4c`，单靠 archive key 无法唯一区分

***

**任务一：包过滤规则**

在 CMSG 处理入口处，加入以下过滤逻辑：

```cpp
bool ShouldTrackPacket(const std::string& archive_key) {
    // 只处理 tag29_* 层，忽略 tag2a_* 前导包
    if (archive_key.substr(0, 6) == "tag2a_") return false;
    return true;
}
```

***

**任务二：X / C 消歧策略**

由于 Hallowed Ground (X) 和 Sanctuary (C) 共享 `tag25_val4c`，需要结合「最后按下的热键」来判定是哪个技能。

```cpp
// 全局追踪最后一次按下的热键
std::string g_last_hotkey_pressed = "";  // 由键盘 hook 更新

// 消歧函数
const SkillEntry* ResolveConflict(
    const std::string& archive_key,
    const std::string& last_hotkey)
{
    if (archive_key != "tag25_val4c") return nullptr;  // 不是冲突 key，走正常路径
    
    if (last_hotkey == "X") return FindSkill(9253);   // Hallowed Ground
    if (last_hotkey == "C") return FindSkill(9128);   // Sanctuary
    
    // 热键未知时，记录警告但不归档，避免误判
    LOG_WARN("[AMBIGUOUS] tag25_val4c received, last_hotkey unknown, skipping");
    return nullptr;
}
```

键盘 hook 需要在用户按下 X / C 时立即更新 `g_last_hotkey_pressed`，时间窗口在 `tag25_val4c` 包到达之前（通常 < 50ms）。

***

**任务三：完整技能配置表**

```cpp
static std::vector<SkillEntry> SKILL_TABLE = {
    // archive_key 已全部确认
    { 71968,  "tag29_val06",  "2", "Peacekeeper",          false },
    { 71987,  "tag29_val07",  "3", "Symbol of Ignition",   false },
    { 9104,   "tag29_val08",  "4", "Zealot's Flame",        false },
    { 9088,   "tag29_val09",  "5", "Cleansing Flame",       false },
    { 41714,  "tag29_val00",  "6", "Mantra of Solace",      true  },  // acr_excluded
    { 9153,   "tag29_val01",  "Z", "Stand Your Ground!",    true  },  // acr_excluded
    { 9253,   "tag25_val4c",  "X", "Hallowed Ground",       true  },  // acr_excluded, 消歧
    { 9128,   "tag25_val4c",  "C", "Sanctuary",             true  },  // acr_excluded, 消歧
    { 43357,  "tag29_val04",  "Q", "Mantra of Liberation",  false },
};

// 冲突 key 集合，命中时走 ResolveConflict
const std::unordered_set<std::string> AMBIGUOUS_KEYS = {
    "tag25_val4c",
};
```

***

**任务四：CMSG 主处理逻辑（完整路径）**

```cpp
void OnCmsgSkillUsed(const ParsedPacket& pkt) {
    const auto& key = pkt.archive_key;
    
    // 1. 过滤前导包
    if (!ShouldTrackPacket(key)) return;
    
    // 2. 查找技能条目
    const SkillEntry* skill = nullptr;
    
    if (AMBIGUOUS_KEYS.count(key)) {
        // 消歧路径：结合最后热键判定
        skill = ResolveConflict(key, g_last_hotkey_pressed);
        if (!skill) return;
    } else {
        // 正常路径：archive_key 唯一
        skill = FindSkillByKey(key);
        if (!skill) return;
    }
    
    // 3. 交给 CD 状态机处理
    g_cd_tracker.OnSkillUsed(*skill);
    
    // 4. 写日志
    LogSkillUsed(*skill);
}
```

***

**任务五：三种通用 CD 状态机（同前，CD 值从 API 拉取）**

启动时调用：
```
GET https://api.guildwars2.com/v2/skills?ids=71968,71987,9104,9088,41714,9153,9253,9128,43357
```

从返回 JSON 的 `recharge` / `ammo.count` / `ammo.recharge_duration` 自动填入 `SKILL_TABLE`，并按以下规则分类：

```
STANDARD       → ammo 字段不存在
MULTI_CHARGE   → ammo 字段存在，无独立 recharge
CHARGE_PLUS_CD → ammo 字段存在，且同时有独立 recharge（如 Zealot's Flame）
```

***

**任务六：日志格式（含消歧来源标注）**

```
[10:15:30.100] [USED]  Symbol of Ignition   (3) tag29_val07 | STANDARD       | cd_ends: 10:15:38.1
[10:15:38.100] [READY] Symbol of Ignition   (3)             | cd_complete
[10:15:41.220] [USED]  Zealot's Flame       (4) tag29_val08 | CHARGE_PLUS_CD | charges:2->1
[10:15:41.720] [READY] Zealot's Flame       (4)             | castable=true  | charges=1
[10:16:00.100] [USED]  Hallowed Ground      (X) tag25_val4c | STANDARD       | resolved_by: hotkey=X | cd_ends: 10:16:25.1
[10:16:00.200] [WARN]  tag25_val4c received, last_hotkey unknown → skipped
```

***

**验收标准**

1. `tag2a_*` 包在入口处被完全过滤，不出现在日志里
2. X / C 消歧正确：快速交替按 X 和 C 各 3 次，日志里两个技能的 CD 独立追踪，互不干扰
3. 其余 7 个技能（2/3/4/5/6/Z/Q）CD 追踪时间误差 < 200ms
4. 消歧失败时（`last_hotkey` 为空），出现 `[WARN]` 而不是崩溃或误归档
5. 启动日志打印 `[API] 9 skills loaded`，并列出每个技能的类型和 CD 值