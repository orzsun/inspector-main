<img src="https://r2cdn.perplexity.ai/pplx-full-logo-primary-dark%402x.png" style="height:64px;margin-right:32px"/>

# 不应该只针对Zealot's Flame吧,应该是所有类似的技能,这个可以判断么? 还要整合前两个promt到一起.

根据 GW2 维基的技能类型，GW2 技能本质上只有三种 CD 模式，我来写一个完整整合版的 prompt：

***

**背景与已知信息**

项目已具备：

- `CMSG_USE_SKILL (0x0017)` 捕获与 archive key 解析（稳定）
- 自动化 trace 框架已运行，`cooldown_path_detected` / `castable` 状态机已落地
- 已确认的 archive key 映射：

| Archive Key | 热键 | 技能名 |
| :-- | :-- | :-- |
| `tag29_val00` | 6 | Mantra of Solace |
| `tag29_val01` | Z | Mantra of Potency |
| `tag25_val83` | X | Mantra of Flame |
| `tag_C_valXX` | C | Mantra of Liberation（待确认） |
| `tag29_val04` | Q | Mantra of Truth |
| `tag08_val00` | 4 | Zealot's Flame |
| `tag09_val00` | 5 | Cleansing Flame |
| `tag02_valXX` | 2 | Symbol of Vengeance（待确认） |
| `tag03_valXX` | 3 | Blazing Edge（待确认） |


***

**核心设计：三种通用技能类型**

不针对特定技能硬编码逻辑，而是定义三种通用类型，所有技能按类型自动套用对应状态机：

```cpp
enum class SkillCdType {
    // 类型一：标准 CD
    // 使用后进入冷却，归零后可用
    // 例：Symbol of Vengeance(2), Blazing Edge(3), Cleansing Flame(5)
    STANDARD,

    // 类型二：多充能（Ammo/Charges）
    // 有最大层数，用一次消耗一层，每层独立计时恢复
    // 例：Mantra of Potency(Z), Mantra of Flame(X), Mantra of Truth(Q)
    MULTI_CHARGE,

    // 类型三：充能积累 + 独立释放 CD
    // 层数由外部机制自动积累（非使用后恢复）
    // 同时有一个独立的使用后 CD
    // 例：Zealot's Flame(4)，层数靠特性自动积累，使用后有转换 CD
    CHARGE_PLUS_CD,
};
```


***

**任务一：实现通用 `SkillCooldownTracker` 类**

```cpp
struct SkillConfig {
    std::string  archive_key;
    std::string  hotkey;
    std::string  skill_name;
    SkillCdType  cd_type;
    int          base_cd_ms;       // 标准 CD 时长 / 每层恢复时长
    int          max_charges;      // MULTI_CHARGE / CHARGE_PLUS_CD 最大层数
    int          cast_cd_ms;       // 仅 CHARGE_PLUS_CD：使用后的独立 CD
    bool         acr_excluded;     // true = 保命技能，仅记录不参与 ACR
};

// 当前 build 配置表
static const std::vector<SkillConfig> SKILL_TABLE = {
    // 武器技能
    { "tag02_valXX", "2", "Symbol of Vengeance", STANDARD,        8000,  1, 0,     false },
    { "tag03_valXX", "3", "Blazing Edge",         STANDARD,       12000,  1, 0,     false },
    { "tag08_val00", "4", "Zealot's Flame",        CHARGE_PLUS_CD, 15000, 5, 1000,  false },
    { "tag09_val00", "5", "Cleansing Flame",       STANDARD,       25000,  1, 0,     false },
    // 通用技能（Mantras）
    { "tag29_val00", "6", "Mantra of Solace",      MULTI_CHARGE,   20000, 3, 0,     true  },
    { "tag29_val01", "Z", "Mantra of Potency",     MULTI_CHARGE,   20000, 3, 0,     false },
    { "tag25_val83", "X", "Mantra of Flame",       MULTI_CHARGE,   20000, 3, 0,     false },
    { "tag_C_valXX", "C", "Mantra of Liberation",  MULTI_CHARGE,   25000, 3, 0,     false },
    { "tag29_val04", "Q", "Mantra of Truth",        MULTI_CHARGE,   30000, 3, 0,     false },
};
```

各类型状态机逻辑：

```
STANDARD:
  OnUsed  → cd_remaining = base_cd_ms
  Tick    → cd_remaining -= dt; 归零 → READY

MULTI_CHARGE:
  OnUsed  → charges -= 1; 启动一个恢复计时器（base_cd_ms 后 charges += 1）
  可用条件 → charges > 0
  全满条件 → charges == max_charges 且无计时器

CHARGE_PLUS_CD:
  外部积累 → 每隔 base_cd_ms 自动 charges += 1（上限 max_charges）
  OnUsed   → charges -= 1; cast_cd_remaining = cast_cd_ms
  可用条件 → charges > 0 AND cast_cd_remaining == 0
```


***

**任务二：统一日志系统**

日志写入 `x64/Release/cd-log/YYYYMMDD.log`，每行格式：

```
[时间戳] [事件类型] 技能名(热键) | 状态详情

事件类型：
  [USED]     - 收到 CMSG，技能被使用
  [CD_START] - 进入冷却（对 STANDARD / CHARGE_PLUS_CD）
  [CHARGE-]  - 充能层数减少（MULTI_CHARGE / CHARGE_PLUS_CD）
  [CHARGE+]  - 充能层数恢复（MULTI_CHARGE / CHARGE_PLUS_CD 自动积累）
  [READY]    - 技能变为可用（至少 1 层可用 / CD 归零）
  [FULL]     - 技能完全回满（所有层数恢复 / CD 彻底结束）
```

示例输出：

```
[10:15:30.000] [USED]     Symbol of Vengeance(2)   | cd_ends: 10:15:38.000
[10:15:38.000] [READY]    Symbol of Vengeance(2)   | cd_complete
[10:15:41.220] [USED]     Zealot's Flame(4)         | charges: 3->2 | cast_cd_ends: 10:15:42.220
[10:15:42.220] [READY]    Zealot's Flame(4)         | castable: true (charges=2)
[10:15:56.220] [CHARGE+]  Zealot's Flame(4)         | charges: 2->3 | auto_accumulated
[10:16:00.100] [USED]     Mantra of Potency(Z)      | charges: 3->2 | recharge_ends: 10:16:20.100
[10:16:20.100] [CHARGE+]  Mantra of Potency(Z)      | charges: 2->3 | full: true
```


***

**任务三：UI 面板新增 CD 状态看板**

在现有工具 UI 里增加一个 `Skill CD Monitor` 面板，实时显示：

```
┌─────────────────────────────────────────────────────┐
│ Skill CD Monitor                                    │
├──────────────────────┬────────────┬─────────────────┤
│ 技能                 │ 状态       │ 剩余            │
├──────────────────────┼────────────┼─────────────────┤
│ Symbol of Vengeance  │ ✅ READY   │ -               │
│ Blazing Edge         │ ⏳ CD      │ 4.2s            │
│ Zealot's Flame       │ ✅ READY   │ charges: 3/5    │
│ Cleansing Flame      │ ⏳ CD      │ 18.7s           │
│ Mantra of Potency    │ ✅ READY   │ charges: 2/3    │
│ Mantra of Flame      │ ⏳ CD      │ 12.1s (1 rech.) │
│ Mantra of Liberation │ ✅ READY   │ charges: 3/3    │
│ Mantra of Truth      │ ✅ READY   │ charges: 3/3    │
│ [EXCLUDED] Mantra of │ ⚠️ SKIP   │ acr_excluded    │
│ Solace               │            │                 │
└──────────────────────┴────────────┴─────────────────┘
```


***

**任务四：顺带确认三个未知 archive key**

本轮测试时分别按一次 `2`、`3`、`C`，在日志里记录实际 archive key，填回 `SKILL_TABLE`。

***

**验收标准**

1. 按任意已配置技能，`cd-log` 立刻出现对应 `[USED]` + `
<span style="display:none">[^1][^10][^11][^12][^13][^14][^15][^2][^3][^4][^5][^6][^7][^8][^9]</span>

<div align="center">⁂</div>

[^1]: https://gw2.huijiwiki.com/wiki/技能

[^2]: https://gw2.17173.com/content/2021-02-10/20210210154703123.shtml

[^3]: https://gw2.huijiwiki.com/wiki/充能

[^4]: https://gw2.wishingstarmoye.com/doc/beginner

[^5]: https://www.reddit.com/r/LancerRPG/comments/1leacxd/hey_hoping_someone_can_explain_how_recharging/

[^6]: https://gw2.huijiwiki.com/wiki/Project:Trans/NamedText

[^7]: https://gw2.huijiwiki.com/wiki/充能技能（技能类型）

[^8]: https://www.reddit.com/r/Guildwars2/comments/y9s8mw/gw2_is_way_too_hard_and_fastpaced_to_be_called/

[^9]: https://www.reddit.com/r/BulletEchoGame/comments/147ni9a/can_someone_explain_the_charges_of_hero_abilities/

[^10]: https://www.reddit.com/r/Guildwars2/comments/27apfc/new_to_gw2_what_are_the_heaviest_skillbased/

[^11]: https://www.bilibili.com/video/BV1764y1W7Mf/

[^12]: https://www.reddit.com/r/Guildwars2/comments/tchtf7/doing_dps_is_not_hard_a_response_to_many/

[^13]: https://gw2.huijiwiki.com/wiki/最终充能（技能类型）

[^14]: https://www.reddit.com/r/Guildwars2/comments/uiksu4/i_quit_wow_for_gw2_here_is_whats_missing_for_me/

[^15]: https://gw2.huijiwiki.com/wiki/技能/充能打击

