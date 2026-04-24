这个观察非常重要。从 Wiki 数据来看，GW2 里槽位 1-5 的替换机制在各职业间差异巨大。 需要把整个模型从「武器组切换」升级为「技能上下文切换」。 [wiki.guildwars2](https://wiki.guildwars2.com/wiki/Profession_mechanic)

***

**各职业 F键替换槽位 1-5 的方式总结**

| 职业 | F键行为 | 替换 1-5？ | 特点 |
|------|---------|-----------|------|
| **元素使** | F1-F4 切换属性（火/水/风/土） | ✅ 完全替换 | 4 套 × 武器类型，不能换武器 |
| **机械师/工程师** | F1-F4 切换工具包 | ✅ 完全替换 | 工具包本身就是技能 |
| **死灵/收割者** | F1 进/出 Shroud | ✅ 完全替换 | 有生命力限制 |
| **Firebrand** | F1-F3 进书卷 | ✅ 完全替换 | 有圣页限制 |
| **幻术师** | F1-F4 是碎片技能 | ❌ 不替换 | F键是独立技能 |
| **战士** | F1 Burst 技能 | ❌ 不替换 | F键是独立技能 |
| **守护者（核心）** | F1-F3 美德激活 | ❌ 不替换 | F键是独立技能 |
| **破法者** | F1-F3 步法美德 | ❌ 不替换 | F键是独立技能 |
| **游侠** | F2 野兽技能 | ❌ 不替换 | F键是独立技能 |

***

以下是升级后的通用 prompt：

***

**背景**

SkillCooldownTracker 当前把武器切换和 F1-F4 都建模为"武器组切换"，但实际上 GW2 各职业的 F 键机制差异极大——有些职业的 F1-F4 完全替换槽位 1-5（元素使、工程师、死灵、Firebrand），有些职业的 F 键只是独立技能（战士、幻术师、破法者）。本轮目标：把「武器组切换」概念升级为「技能上下文」通用模型，让 tracker 能正确处理所有职业。

***

**任务一：核心概念重新建模**

删除 `WeaponSet` 概念，替换为通用的 `SkillContext`：

```cpp
// 技能上下文：代表一套完整的槽位 1-5 映射
struct SkillContext {
    std::string id;           // 唯一标识："weapon_set_a", "fire_attunement", "shroud" 等
    std::string display_name; // UI 显示："Set A (Axe/Torch)", "Fire", "Death Shroud"
    SkillEntry  slots [youtube](https://www.youtube.com/watch?v=EXWvLxIR23g);     // 槽位 1-5 的技能
    bool        replaces_weapon_bar; // true = 完全替换 1-5；false = F 键是独立技能
};

// 上下文切换事件来源
enum class ContextSwitchTrigger {
    WEAPON_SWAP,       // 普通换武器（tag0c_*）
    PROFESSION_F1,     // F1（书卷/属性/Shroud）
    PROFESSION_F2,     // F2
    PROFESSION_F3,     // F3
    PROFESSION_F4,     // F4
};

// 全局状态
std::vector<SkillContext> g_skill_contexts;   // 当前职业所有可用上下文
int                       g_active_context = 0;
```

***

**任务二：职业上下文配置（build_profiles 里声明）**

在 build profile JSON 里新增 `context_mode` 字段，显式声明该职业的 F 键行为：

```json
{
  "profile_name": "Firebrand Axe/Torch",
  "profession": "Guardian",
  "elite_spec": "Firebrand",

  "context_mode": "REPLACE_ON_F",
  // REPLACE_ON_F    = F1-F3 完全替换槽位 1-5（Firebrand、元素使、工程师、死灵）
  // INDEPENDENT_F   = F键是独立技能，不影响槽位 1-5（战士、幻术师、破法者）
  // WEAPON_SWAP_ONLY = 只有换武器才切换槽位 1-5

  "contexts": [
    {
      "id": "weapon_set_a",
      "display_name": "Set A (Axe/Torch)",
      "trigger": "WEAPON_SWAP",
      "replaces_weapon_bar": true,
      "slots": [ ...当前 5 个技能... ]
    },
    {
      "id": "tome_f1",
      "display_name": "Tome of Justice (F1)",
      "trigger": "PROFESSION_F1",
      "replaces_weapon_bar": true,
      "slots": [
        { "slot": 1, "skill_name": "Tome of Justice 1", "cd_type": "PAGE_BASED" },
        { "slot": 2, "skill_name": "Tome of Justice 2", "cd_type": "PAGE_BASED" },
        ...
      ]
    },
    {
      "id": "tome_f2",
      "display_name": "Tome of Resolve (F2)",
      "trigger": "PROFESSION_F2",
      "replaces_weapon_bar": true,
      "slots": [ ... ]
    },
    {
      "id": "tome_f3",
      "display_name": "Tome of Courage (F3)",
      "trigger": "PROFESSION_F3",
      "replaces_weapon_bar": true,
      "slots": [ ... ]
    }
  ]
}
```

对比：**元素使的 profile** 会有 4 个 attunement context + 2 个武器组 = 最多 8 种上下文组合；**战士的 profile** 的 F1 直接列在 utility 层，`replaces_weapon_bar: false`。

***

**任务三：上下文切换处理器**

```cpp
void OnContextSwitch(ContextSwitchTrigger trigger, const ParsedPacket& pkt) {
    // 1. 找到对应的目标上下文
    SkillContext* target = FindContextByTrigger(trigger);
    if (!target) {
        LOG_DEBUG("[CTX_UNKNOWN] trigger=%d key=%s — no matching context",
                  trigger, pkt.archive_key.c_str());
        return;
    }

    // 2. 如果目标上下文不替换武器栏，当做普通技能处理
    if (!target->replaces_weapon_bar) {
        OnSkillUsed(pkt);
        return;
    }

    // 3. 切换上下文
    auto& prev = g_skill_contexts[g_active_context];
    g_active_context = IndexOf(target);

    LOG_INFO("[CTX_SWITCH] %s → %s | trigger=%s | key=%s",
             prev.display_name.c_str(),
             target->display_name.c_str(),
             TriggerName(trigger),
             pkt.archive_key.c_str());

    // 4. 更新 UI
    UpdateSkillBarDisplay(*target);
}
```

***

**任务四：tag0c_* 路由升级**

```cpp
void OnCmsgReceived(const ParsedPacket& pkt) {
    const auto& key = pkt.archive_key;

    if (key.substr(0, 6) == "tag2a_") return; // 前导包，忽略

    // tag0c_* → 上下文切换通道（武器切换 / F1-F4）
    if (key.substr(0, 6) == "tag0c_") {
        auto trigger = ResolveTrigger(key);  // 根据 key 判断是哪个 F 键或武器切换
        OnContextSwitch(trigger, pkt);
        return;
    }

    // tag29_* / tag25_* → 普通技能动作
    if (key.substr(0, 6) == "tag29_" || key.substr(0, 6) == "tag25_") {
        OnSkillUsed(pkt);
        return;
    }

    LOG_DEBUG("[UNKNOWN_CHANNEL] key=%s", key.c_str());
}
```

`ResolveTrigger` 的映射需要本轮实测确认 `tag0c_val00` / `tag0c_val17` 哪个对应哪个 F 键。

***

**任务五：UI 升级**

```
┌─────────────────────────────────────────────────────────────┐
│ Skill CD Monitor         [Firebrand] [API: 9/9 ✓]          │
│ Context: Set A (Axe/Torch)    ← 当前上下文，点击可手动切换  │
├──────────────────────┬──────────────────┬───────────────────┤
│ Peacekeeper     (2)  │ ✅ READY         │ -                 │
│ Symbol of Ign.  (3)  │ ⏳ CD            │ 4.2s              │
│ Zealot's Flame  (4)  │ ✅ READY         │ charges: 2/2      │
│ Cleansing Flame (5)  │ ⏳ CD            │ 8.7s              │
├──────────────────────┼──────────────────┼───────────────────┤
│ ⚔ Weapon Swap        │ ⏳ CD            │ 7.3s              │
│ 📖 F1 Tome           │ ✅ READY         │ -                 │
│ 📖 F2 Tome           │ ⏳ CD            │ 12.1s             │
│ 📖 F3 Tome           │ ✅ READY         │ -                 │
├──────────────────────┼──────────────────┼───────────────────┤
│ Mantra of Lib.  (Q)  │ ✅ READY         │ charges: 2/3      │
└─────────────────────────────────────────────────────────────┘
```

***

**任务六：本轮实测目标**

确认 `tag0c_*` 各 key 的具体语义：

```
测试步骤：
1. 按 F1（进 Tome of Justice）→ 记录 tag0c_* key
2. 按 F1（退出 Tome）         → 记录 tag0c_* key
3. 按武器切换                  → 记录 tag0c_* key
4. 按 F2 / F3                  → 记录 tag0c_* key

预期：
- 每个 F 键和武器切换各自对应不同的 tag0c_* key
- 或者：进/退同一 context 共用一个 key（toggle 模式）
```

***

**验收标准**

1. `context_mode: REPLACE_ON_F` 时，F1-F3 触发 `[CTX_SWITCH]`，不出现 `[USED]`
2. `context_mode: INDEPENDENT_F` 时，F 键走 `[USED]` 普通技能路径
3. 切换 context 后，UI 的槽位 1-5 立刻更新为对应 context 的技能名
4. 两套 context 的 CD 独立计时，互不干扰
5. 本轮实测后，`tag0c_*` 各 key 的语义被记录到 `overrides.json` 的 `special_mechanics` 里