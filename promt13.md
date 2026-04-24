搜索结果不相关，这个问题直接从技术设计角度回答更准确。

**本质上你需要两层存储：**

| 层 | 来源 | 内容 | 更新时机 |
|---|---|---|---|
| **API 缓存层** | GW2 官方 API | recharge / ammo / slot | 技能 patch 后刷新 |
| **本地覆写层** | 手动维护 | 特殊机制、override CD、标注类型 | 随时手动编辑 |

***

以下是整合了本地存储的完整 prompt：

***

**背景**

SkillCooldownTracker 当前每次启动都要调用 GW2 API，且无法处理 API 数据不准确或缺失的情况（如 Peacekeeper recharge=0）。本轮目标：建立一套本地技能数据库，实现 API 缓存 + 本地覆写 + 特殊机制标注，并兼容 build / 角色切换。

***

**任务一：目录结构**

```
x64/Release/skill-db/
    api_cache.json          ← GW2 API 返回的原始数据缓存
    overrides.json          ← 本地覆写层：修正错误 CD、标注特殊机制
    build_profiles/
        firebrand_axe_torch.json   ← 当前 build 的完整配置快照
        (其他 build...)
```

***

**任务二：`api_cache.json` 格式**

存储从 `/v2/skills` 拿到的原始数据，带时间戳，用于判断是否需要刷新：

```json
{
  "cached_at": "2026-04-16T11:20:00Z",
  "game_build": 154321,
  "skills": {
    "71968": {
      "name": "Peacekeeper",
      "recharge": 0,
      "ammo": null,
      "slot": "Weapon_2"
    },
    "9104": {
      "name": "Zealot's Flame",
      "recharge": 15,
      "ammo": { "count": 2, "recharge_duration": 15000 },
      "slot": "Weapon_4"
    },
    "9088": {
      "name": "Cleansing Flame",
      "recharge": 15,
      "ammo": null,
      "slot": "Weapon_5"
    },
    "41714": {
      "name": "Mantra of Solace",
      "recharge": 20,
      "ammo": { "count": 3, "recharge_duration": 20000 },
      "slot": "Heal"
    },
    "9153": {
      "name": "Stand Your Ground!",
      "recharge": 30,
      "ammo": null,
      "slot": "Utility"
    },
    "9253": {
      "name": "Hallowed Ground",
      "recharge": 45,
      "ammo": null,
      "slot": "Utility"
    },
    "9128": {
      "name": "Sanctuary",
      "recharge": 75,
      "ammo": null,
      "slot": "Utility"
    },
    "43357": {
      "name": "Mantra of Liberation",
      "recharge": 40,
      "ammo": { "count": 3, "recharge_duration": 40000 },
      "slot": "Elite"
    }
  }
}
```

刷新策略：
```cpp
bool ShouldRefreshCache(const ApiCache& cache) {
    // 1. 缓存超过 7 天强制刷新
    if (AgeInDays(cache.cached_at) > 7) return true;
    // 2. 游戏 build 号变化时刷新（从内存或 API /v2/build 读取）
    if (cache.game_build != GetCurrentGameBuild()) return true;
    // 3. 用户手动点击 "Force Refresh" 时刷新
    return false;
}
```

***

**任务三：`overrides.json` 格式**

本地覆写层，专门处理：
- API 返回错误值（如 recharge=0）
- 特殊机制（隐藏 CD、特性加成）
- 手动注释

```json
{
  "last_modified": "2026-04-16T11:25:00Z",
  "overrides": {
    "71968": {
      "skill_name": "Peacekeeper",
      "recharge_override_ms": 6000,
      "note": "API returns recharge=0, actual CD is 6s. Verified in-game 2026-04-16.",
      "api_data_wrong": true
    },
    "9104": {
      "skill_name": "Zealot's Flame",
      "recharge_override_ms": 12000,
      "cd_type_override": "CHARGE_PLUS_CD",
      "cast_cd_ms": 500,
      "note": "Radiant Fire trait: -20% recharge (15s -> 12s). Hidden 0.5s cast CD after use.",
      "trait_modified": true
    },
    "9088": {
      "skill_name": "Cleansing Flame",
      "recharge_override_ms": 12000,
      "note": "Radiant Fire trait: -20% recharge (15s -> 12s).",
      "trait_modified": true
    }
  },
  "special_mechanics": {
    "weapon_swap": {
      "cd_ms": 10000,
      "archive_key": "UNKNOWN_TBD",
      "note": "Weapon swap has 10s CD. Archive key not yet confirmed."
    }
  }
}
```

***

**任务四：`build_profiles/firebrand_axe_torch.json` 格式**

Build 快照，把 archive key、API ID、覆写合并后的最终数据一次性固化：

```json
{
  "profile_name": "Firebrand Axe/Torch — Support",
  "created_at": "2026-04-16T11:25:00Z",
  "profession": "Guardian",
  "elite_spec": "Firebrand",
  "weapon_set_a": {
    "weapon_main": "Axe",
    "weapon_off":  "Torch",
    "skills": [
      { "slot": 1, "hotkey": "1", "api_id": null,  "archive_key": null,           "skill_name": "Through the Heart", "cd_type": "AUTO_ATTACK", "recharge_ms": 0 },
      { "slot": 2, "hotkey": "2", "api_id": 71968, "archive_key": "tag29_val06",  "skill_name": "Peacekeeper",        "cd_type": "STANDARD",    "recharge_ms": 6000,  "note": "API wrong, override applied" },
      { "slot": 3, "hotkey": "3", "api_id": 71987, "archive_key": "tag29_val07",  "skill_name": "Symbol of Ignition", "cd_type": "STANDARD",    "recharge_ms": 15000 },
      { "slot": 4, "hotkey": "4", "api_id": 9104,  "archive_key": "tag29_val08",  "skill_name": "Zealot's Flame",     "cd_type": "CHARGE_PLUS_CD", "recharge_ms": 12000, "ammo_count": 2, "cast_cd_ms": 500 },
      { "slot": 5, "hotkey": "5", "api_id": 9088,  "archive_key": "tag29_val09",  "skill_name": "Cleansing Flame",    "cd_type": "STANDARD",    "recharge_ms": 12000 }
    ]
  },
  "weapon_set_b": {
    "skills": []
  },
  "utility_skills": [
    { "slot": "heal",    "hotkey": "6", "api_id": 41714, "archive_key": "tag29_val00", "skill_name": "Mantra of Solace",      "cd_type": "MULTI_CHARGE", "recharge_ms": 20000, "ammo_count": 3, "acr_excluded": true },
    { "slot": "util1",   "hotkey": "Z", "api_id": 9153,  "archive_key": "tag29_val01", "skill_name": "Stand Your Ground!",    "cd_type": "STANDARD",    "recharge_ms": 30000, "acr_excluded": true },
    { "slot": "util2",   "hotkey": "X", "api_id": 9253,  "archive_key": "tag25_val9a", "skill_name": "Hallowed Ground",       "cd_type": "STANDARD",    "recharge_ms": 45000, "acr_excluded": true },
    { "slot": "util3",   "hotkey": "C", "api_id": 9128,  "archive_key": "tag25_valc5", "skill_name": "Sanctuary",             "cd_type": "STANDARD",    "recharge_ms": 75000, "acr_excluded": true },
    { "slot": "elite",   "hotkey": "Q", "api_id": 43357, "archive_key": "tag29_val04", "skill_name": "Mantra of Liberation",  "cd_type": "MULTI_CHARGE", "recharge_ms": 40000, "ammo_count": 3 }
  ]
}
```

***

**任务五：加载优先级逻辑**

```cpp
SkillEntry LoadSkill(int api_id) {
    // 优先级：build_profile > overrides > api_cache
    // 1. 先从 build_profile 读（已合并好的终态）
    if (g_current_profile.HasSkill(api_id))
        return g_current_profile.GetSkill(api_id);

    // 2. 没有 profile 时，API 缓存 + 覆写合并
    SkillEntry e = g_api_cache.GetSkill(api_id);
    if (g_overrides.HasOverride(api_id))
        g_overrides.Apply(api_id, e);

    return e;
}
```

***

**任务六：UI 新增操作**

在 `Skill CD Monitor` 面板右上角增加：

```
[⟳ Reload Build]     ← 重新从文件加载当前 profile
[✎ Edit Overrides]   ← 用文本编辑器打开 overrides.json
[↻ Refresh API]      ← 强制重新调用 API 并更新 api_cache.json
[💾 Save Profile]    ← 把当前运行时状态保存为 build_profile 快照
```

***

**验收标准**

1. 程序启动时优先读取 `build_profiles/firebrand_axe_torch.json`，无网络也能正常运行
2. Peacekeeper 显示 CD = 6000ms（来自 overrides），日志打印 `[OVERRIDE] Peacekeeper: api_recharge=0 → override=6000ms`
3. 修改 `overrides.json` 后点击 `[⟳ Reload Build]`，无需重启立刻生效
4. `api_cache.json` 的 `game_build` 字段变化时，启动时自动触发 API 刷新
5. 切换角色 / build 时，通过 `[💾 Save Profile]` 保存新 profile，下次启动自动识别