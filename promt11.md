现在所有 API ID 都确认了 ： [wiki.guildwars2](https://wiki.guildwars2.com/wiki/Peacekeeper)

| 槽位 | 技能名 | **API ID** |
|------|--------|-----------|
| 2 | Peacekeeper | **71968** |
| 3 | Symbol of Ignition | **71987** |
| 4 | Zealot's Flame | **9104** |
| 5 | Cleansing Flame | **9088** |
| 6 | Mantra of Solace | **41714** |
| Z | "Stand Your Ground!" | **9153** |
| X | Hallowed Ground | **9253** |
| C | Sanctuary | **9128** |
| Q | Mantra of Liberation | **43357** |

以下是带完整 API ID 的最终 prompt：

***

**背景**

项目已具备 `CMSG_USE_SKILL (0x0017)` 捕获与稳定的 archive key 解析。本轮新增 `SkillCooldownTracker` 模块，实现技能 CD 状态检测与日志记录。**所有 CD 数值必须在启动时从 GW2 官方 API 动态获取，不得硬编码。**

***

**任务一：启动时从 GW2 API 拉取技能数据**

以下是当前 build 所有技能的完整 API ID 列表（已从 wiki 逐一确认）：

```cpp
const std::vector<int> SKILL_API_IDS = {
    71968,  // Peacekeeper          (2)
    71987,  // Symbol of Ignition   (3)
    9104,   // Zealot's Flame       (4)  CHARGE_PLUS_CD
    9088,   // Cleansing Flame      (5)
    41714,  // Mantra of Solace     (6)  MULTI_CHARGE, acr_excluded
    9153,   // "Stand Your Ground!" (Z)  acr_excluded
    9253,   // Hallowed Ground      (X)  acr_excluded
    9128,   // Sanctuary            (C)  acr_excluded
    43357,  // Mantra of Liberation (Q)  MULTI_CHARGE
};
```

启动时调用：
```
GET https://api.guildwars2.com/v2/skills?ids=71968,71987,9104,9088,41714,9153,9253,9128,43357
```

从返回 JSON 提取每个技能的：
- `recharge`：基础 CD 秒数
- `ammo`（如存在）：`count`（最大 charges）和 `recharge_duration`（每层恢复毫秒数）
- `facts`：type=`Recharge` 的条目作为校验

***

**任务二：热键到 archive key 的映射表**

结合已确认的 archive key，建立完整映射：

```cpp
struct SkillEntry {
    int         api_id;
    std::string archive_key;   // CMSG 包里的 tail 标识
    std::string hotkey;
    std::string skill_name;
    bool        acr_excluded;  // true = 保命技能，只记录，不参与 ACR
    // CD 数据从 API 填入，不硬编码
    int         recharge_ms;         // API 拉取后写入
    int         ammo_count;          // 0 = STANDARD
    int         ammo_recharge_ms;    // 0 = STANDARD
};

static std::vector<SkillEntry> SKILL_TABLE = {
    { 71968,  "tag02_valXX", "2", "Peacekeeper",          false },
    { 71987,  "tag03_valXX", "3", "Symbol of Ignition",   false },
    { 9104,   "tag08_val00", "4", "Zealot's Flame",        false },
    { 9088,   "tag09_val00", "5", "Cleansing Flame",       false },
    { 41714,  "tag29_val00", "6", "Mantra of Solace",      true  },
    { 9153,   "tag29_val01", "Z", "Stand Your Ground!",    true  },
    { 9253,   "tag25_val83", "X", "Hallowed Ground",       true  },
    { 9128,   "tag_C_valXX", "C", "Sanctuary",             true  },
    { 43357,  "tag29_val04", "Q", "Mantra of Liberation",  false },
};
```

注意：`tag02_valXX`、`tag03_valXX`、`tag_C_valXX` 三个 archive key 尚未确认，本轮测试中顺带按一次对应按键来确认。

***

**任务三：三种通用 CD 类型状态机**

从 API 返回数据自动分类，不针对特定技能名写逻辑：

```cpp
enum class SkillCdType {
    STANDARD,        // ammo_count == 0：标准单次 CD
    MULTI_CHARGE,    // ammo_count > 0 且无独立 recharge：消耗最后一层才进完整 CD
    CHARGE_PLUS_CD,  // ammo_count > 0 且同时有 recharge：充能自动积累 + 使用后独立 CD
};

SkillCdType InferType(const SkillEntry& e) {
    if (e.ammo_count == 0)                        return STANDARD;
    if (e.recharge_ms > 0 && e.ammo_count > 0
        && e.ammo_recharge_ms > 0)                return CHARGE_PLUS_CD;
    return MULTI_CHARGE;
}
```

各类型状态机：

```
STANDARD:
  OnUsed  → cd_ends_at = now + recharge_ms
  isReady → now >= cd_ends_at

MULTI_CHARGE（Mantra 类）:
  初始    → charges = ammo_count（全满）
  OnUsed → charges -= 1
           若 charges > 0：启动单层恢复计时（ammo_recharge_ms 后 charges += 1）
           若 charges == 0：进入完整 CD（recharge_ms 后重置为 ammo_count）
  isReady → charges > 0
  isFull  → charges == ammo_count

CHARGE_PLUS_CD（Zealot's Flame 等）:
  被动    → 每隔 ammo_recharge_ms 自动 charges += 1（上限 ammo_count）
  OnUsed → charges -= 1; cast_cd_ends_at = now + 500ms（使用后隐藏小 CD）
  isReady → charges > 0 AND now >= cast_cd_ends_at
```

***

**任务四：统一日志格式**

写入 `x64/Release/cd-log/YYYYMMDD.log`：

```
[10:15:30.100] [USED]     Peacekeeper          (2)  | STANDARD       | cd_ends: 10:15:36.100
[10:15:36.100] [READY]    Peacekeeper          (2)  | cd_complete
[10:15:41.220] [USED]     Zealot's Flame       (4)  | CHARGE_PLUS_CD | charges:2->1 | mini_cd: 500ms
[10:15:41.720] [READY]    Zealot's Flame       (4)  | castable=true  | charges=1
[10:15:56.000] [CHARGE+]  Zealot's Flame       (4)  | charges:1->2   | auto_accumulated
[10:16:00.100] [USED]     Mantra of Liberation (Q)  | MULTI_CHARGE   | charges:3->2 | recharge_ends: 10:16:X
[10:16:00.200] [USED]     Mantra of Liberation (Q)  | MULTI_CHARGE   | charges:2->1
[10:16:00.300] [USED]     Mantra of Liberation (Q)  | MULTI_CHARGE   | charges:1->0 | FULL_CD
[10:16:XX.XXX] [READY]    Mantra of Liberation (Q)  | full_reset     | charges=3
[10:15:55.000] [USED]     Stand Your Ground!   (Z)  | STANDARD       | acr_excluded | cd_ends: 10:16:25.000
[10:16:25.000] [READY]    Stand Your Ground!   (Z)  | acr_excluded   | cd_complete
```

***

**任务五：UI 面板**

新增 `Skill CD Monitor` 实时面板：

```
┌─────────────────────────────────────────────────────────┐
│ Skill CD Monitor              [API: 9 skills loaded ✓] │
├──────────────────────┬─────────────────┬────────────────┤
│ 技能                 │ 状态            │ 详情           │
├──────────────────────┼─────────────────┼────────────────┤
│ Peacekeeper     (2)  │ ✅ READY        │ -              │
│ Symbol of Ign.  (3)  │ ⏳ CD           │ 4.2s           │
│ Zealot's Flame  (4)  │ ✅ READY        │ charges: 2/2   │
│ Cleansing Flame (5)  │ ⏳ CD           │ 8.7s           │
│ Mantra of Lib.  (Q)  │ ✅ READY        │ charges: 2/3   │
├──────────────────────┼─────────────────┼────────────────┤
│ Mantra of Sol.  (6)  │ ⚠️ EXCLUDED    │ acr_excluded   │
│ Stand Yr Ground (Z)  │ ⚠️ EXCLUDED    │ acr_excluded   │
│ Hallowed Ground (X)  │ ⚠️ EXCLUDED    │ acr_excluded   │
│ Sanctuary       (C)  │ ⚠️ EXCLUDED    │ acr_excluded   │
└──────────────────────┴─────────────────┴────────────────┘
```

***

**验收标准**

1. 启动时成功调用 API，日志打印 `[API] Loaded 9 skills OK`，并显示每个技能的名字、类型、CD 值
2. 每个技能自动分类为正确的 `SkillCdType`（`STANDARD` / `MULTI_CHARGE` / `CHARGE_PLUS_CD`）
3. 按任意技能，`cd-log` 立刻出现对应 `[USED]` 条目
4. CD 到期后出现 `[READY]` 条目，时间误差 < 200ms
5. Mantra 连按三次后正确进入 `FULL_CD`，全满后出现 `full_reset`
6. Zealot's Flame 使用后 500ms 内不可用，500ms 后 `castable=true`
7. 按 `2`、`3`、`C` 各一次，日志里的 archive key 被确认并填回 `SKILL_TABLE`