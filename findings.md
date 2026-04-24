# GW2 Reffect Git 调查报告

> 来源仓库：https://github.com/Zerthox/gw2-reffect  
> 调查时间：2026-04-19  
> 目标：从开源代码历史中提取 GW2 技能冷却数据结构的逻辑模型

---

## 一、关键 Commit 时间线

| 日期 | Hash | 说明 |
|---|---|---|
| 2024-06-28 | `418b6df` | Switch to reffect internal — 内存读取正式移入私有 DLL |
| 2024-12-10 | `8c61d25` | Add skillbar — **首次引入 Skillbar/Ability/Slot 数据模型** |
| 2024-12-14 | `b08758f` | Add weapon & legend swap — 新增 WeaponSwap/LegendSwap 独立对象 |
| 2024-12-25 | `add26fa` | Add weapon swap icons |
| 2024-12-30 | `62efc08` | Adjust skillbar — last_update/recharge_rate 下移到每个 Ability |
| 2025-09-18 | `b2bdc45` | Implement pet swap — 新增 PetSwap SkillId，移除 has_bundle |
| 2025-10-24 | `6d60049` | Implement pet resources — 新增 pet CombatantResources |

**重要结论**：内存读取实现从未提交，一直在私有二进制 DLL 中。  
开源部分只包含数据模型（struct 定义）和消费层（trigger/render）。

---

## 二、核心数据结构（最终版 `7a0c8ba`）

### Skillbar

```rust
pub struct Skillbar {
    pub skills: SkillSlots,   // [Option<Ability>; 18]
}
// 注：last_update / recharge_rate 已在 62efc08 下移到每个 Ability
// 注：has_bundle 已在 b2bdc45 移除（改为通过 SkillId::BundleDrop 判断）
```

### Ability（每个 slot 的数据，共 9 个字段）

```rust
pub struct Ability {
    pub id: SkillId,                  // 技能标识符
    pub ammo: u32,                    // 弹药数（无弹药技能 = 1）
    pub last_update: u32,             // 上次更新时间戳（毫秒，timeGetTime）
    pub recharge_rate: f32,           // 冷却速率（正常 = 1.0，加速 > 1.0）
    pub recharge: u32,                // 总冷却时间（毫秒，固定值）
    pub recharge_remaining: u32,      // 冷却剩余快照（毫秒，需配合时间戳计算）
    pub ammo_recharge: u32,           // 弹药充能总时间（毫秒）
    pub ammo_recharge_remaining: u32, // 弹药充能剩余快照（毫秒）
    pub state: BitFlags<AbilityState>,// 状态位标志（u8）
}
// 总大小估算：9字段 × 4字节 = 36字节（0x24）
// 注意：state 是 BitFlags<u8>，实际占 1 字节，有 padding
```

### Recharge（WeaponSwap / LegendSwap 专用）

```rust
pub struct Recharge {
    pub last_update: u32,  // 上次更新时间戳
    pub recharge: u32,     // 总冷却时间（毫秒）
}
// 剩余 = (last_update + recharge) - now
// 注意：计算方式与 Ability 不同！
```

### SkillId 枚举

```rust
pub enum SkillId {
    Unknown,      // 默认/未知
    WeaponSwap,   // 武器交换（不在 skills 数组里）
    BundleDrop,   // 丢弃 Bundle（has_bundle=true 时替换 WeaponSwap）
    PetSwap,      // 宠物交换（Ranger 专用）
    Id(u32),      // 普通技能 ID
}
```

### AbilityState 位标志

```rust
pub enum AbilityState {
    AutoAttack    = 1 << 0,  // 0x01 自动攻击
    Pending       = 1 << 1,  // 0x02 施放中/排队中（绿色）
    Pressed       = 1 << 2,  // 0x04 正在按住（蓝色）
    ActivePrimary = 1 << 3,  // 0x08 主要激活
    ActiveSecondary = 1 << 4,// 0x10 次要激活
    NoResources   = 1 << 5,  // 0x20 资源不足（红色）
    NoRange       = 1 << 6,  // 0x40 不在范围内（黄色）
}
```

### Slot 枚举（18 个，含 WeaponSwap）

```rust
pub enum Slot {
    WeaponSwap,     // index 0  — 独立 Recharge 对象，不在 skills[] 里
    Weapon1,        // index 0  → skills[0]
    Weapon2,        // index 1  → skills[1]
    Weapon3,        // index 2  → skills[2]
    Weapon4,        // index 3  → skills[3]
    Weapon5,        // index 4  → skills[4]
    Heal,           // index 5  → skills[5]
    Utility1,       // index 6  → skills[6]
    Utility2,       // index 7  → skills[7]
    Utility3,       // index 8  → skills[8]
    Elite,          // index 9  → skills[9]
    Profession1,    // index 10 → skills[10]
    Profession2,    // index 11 → skills[11]
    Profession3,    // index 12 → skills[12]
    Profession4,    // index 13 → skills[13]
    Profession5,    // index 14 → skills[14]
    SpecialAction,  // index 15 → skills[15]
    Mount,          // index 16 → skills[16]
}
// 早期版本（8c61d25 ~ b2bdc45）无 WeaponSwap，skills 数组大小为 17
// 当前版本（b2bdc45 之后）含 WeaponSwap，skills 数组大小为 18
```

---

## 三、冷却时间计算逻辑

### 普通技能（Ability）

```rust
// 经过的时间（考虑速率加成）
passed = (now - last_update) * recharge_rate

// 实时剩余冷却
remaining = max(0, recharge_remaining - passed)

// 冷却结束的绝对时间戳（用于持久化或比较）
end = last_update + (recharge_remaining / recharge_rate)

// 冷却进度（0.0 = 冷却完毕，1.0 = 刚按下）
progress = remaining / recharge
```

### WeaponSwap / LegendSwap（Recharge）

```rust
// 直接用绝对终点时间，计算方式不同！
remaining = max(0, (last_update + recharge) - now)
end = last_update + recharge
```

### 弹药技能（ammo）

```rust
// 弹药充能剩余
ammo_remaining = max(0, ammo_recharge_remaining - passed)

// 弹药充能进度
ammo_progress = ammo_remaining / ammo_recharge

// 判断技能就绪（有弹药且不在主冷却）
is_ready = (remaining == 0) && (ammo > 0)
```

---

## 四、内存读取路径（从 Error 变体顺序推断）

`8c61d25` 首次引入的 Error 变体顺序揭示了读取路径：

```
Error::Context      →  游戏内 Context 对象
Error::World        →  Context → World 子对象
Error::Content      →  World → Content 子对象
Error::Skill        →  Content → Skill 数据库（用于技能元数据）
Error::User         →  找到本地 User/Agent
Error::Character    →  User → 本地角色对象
Error::CharacterState → 检查角色状态（不在载入等）
Error::Buffs        →  Character → Buff 列表
Error::Skillbar     →  Character → Skillbar   ← 目标
Error::Health       →  Character → 血量
Error::Endurance    →  Character → 耐力
```

对应你的调查路径：
```
ContextCollection → ChCliCtx → LocalChar → CharSkillbar → skills[]
```

### 在 IDA / 内存中搜索的字符串

```
CoContext / ChCliCtx / AClientContext
CoWorld   / AWorld   / ChWorld
CoChar    / ChChar   / AChar / LocalPlayer
CoSkillbar / ChSkillBar / ASkillBar / SkillBar
```

---

## 五、已知 vs 未知

| 内容 | 状态 | 来源 |
|---|---|---|
| `Ability` 全部字段定义 | ✅ 确认 | `reffect_core/src/context/skill/ability.rs` |
| `Recharge` struct | ✅ 确认 | `reffect_api/src/skillbar.rs` @ `b08758f` |
| `AbilityState` 位标志 | ✅ 确认 | `reffect_core/src/context/skill/ability.rs` |
| Slot 枚举顺序（18个） | ✅ 确认 | `reffect_core/src/context/skill/skillbar.rs` |
| `SkillId` 枚举 | ✅ 确认 | `reffect_core/src/context/skill/mod.rs` |
| 冷却时间计算方式 | ✅ 确认 | `active.rs` `from_ability()` |
| 内存读取路径层次 | ✅ 推断 | Error 变体顺序 |
| `skills[]` 数组基地址 | ❌ 未知 | 私有 DLL，需内存调查 |
| `weapon_swap` / `legend_swap` 偏移 | ❌ 未知 | 私有 DLL |
| `ptr48` / `ptr60` 具体指向 | ❌ 未知 | 需继续 dump 调查 |
| `Skillbar` 对象内字段布局 | ❌ 未知 | 游戏 C++ 对象，需对照 |

---

## 六、验证方法

对一个**处于冷却中**的技能做内存 dump，按以下规律识别字段：

```
偏移 +0x00: id              — 已知的 skill ID 数值（u32，可查 wiki）
偏移 +0x04: ammo            — 1~5 的小整数（无弹药技能 = 1，冷却中 = 0）
偏移 +0x08: last_update     — 接近 timeGetTime() 的毫秒值（约 10位数）
偏移 +0x0C: recharge_rate   — 接近 1.0 的 float
偏移 +0x10: recharge        — 总冷却毫秒（固定，如 30000 = 30秒）
偏移 +0x14: recharge_remaining — 冷却中快速减少的值
偏移 +0x18: ammo_recharge   — 弹药充能总时间（无弹药技能 = 0）
偏移 +0x1C: ammo_recharge_remaining — 同上剩余
偏移 +0x20: state           — 0x00~0x7F 的位标志（u8 + padding）
```

每个 Ability 估算占 **32~36 字节**，相邻 slot 地址差应为固定值。

---

*本文档由 gw2-reffect 开源仓库代码历史分析生成，不含任何逆向工程内容。*
