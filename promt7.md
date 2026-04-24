<img src="https://r2cdn.perplexity.ai/pplx-full-logo-primary-dark%402x.png" style="height:64px;margin-right:32px"/>

# 给我新的promt

基于这份最新发现，给 Agent 的 prompt：

***

**背景更新**

自动化 trace 框架已经跑通，但有三个明确的质量问题需要修复。本轮任务专注于这三个修复，不扩展新功能。

***

**修复一（最高优先级）：统一 tail 字段解析**

当前自动化代码对 `CMSG_USE_SKILL (0x0017)` packet 的 tail 解析结果与手动 UI 的 `UseSkill Breakdown` 不一致。Archive key 出现了类似 `tag_A1_val_88`、`tag_00_val_00`、`tag_04_val_00` 这样错误的值。

**要求：**

将自动化 trace 代码中的 tail 解析逻辑替换为与手动 UI 完全相同的实现。具体字段结构如下：

```
byte @ 0x0D: tail_prefix   (目前固定为 0x02，保留但不用作分类)
byte @ 0x0E: tail_tag      (主分类：0x29 = primary, 0x25 = secondary)
byte @ 0x0F: tail_val      (具体槽位值)
```

Archive key 格式改为：`tag{tail_tag:02X}_val{tail_val:02X}`

例如：`tag29_val00`（Healing）、`tag29_val01`（Utility1/Z）、`tag29_val04`（Elite/Q）

修复后用已知按键各采一次，验证 archive key 与下表一致：


| Hotkey | 期望 Archive Key |
| :-- | :-- |
| `Healing(6)` | `tag29_val00` |
| `Utility1(Z)` | `tag29_val01` |
| `Elite(Q)` | `tag29_val04` |
| `Profession1(F1)` | `tag29_val0C` |
| `Utility2(X)` | `tag25_val83` |


***

**修复二：替换固定 `+500ms` cooldown 锁定**

当前 `cooldown_stable` 在 packet 确认后固定 `+500ms` 采样，导致部分技能（尤其是 `Healing`）采样过早，与 baseline_pre 几乎无差异。

**要求：**

改为基于「变化检测」的动态锁定逻辑：

```
packet 确认后，开始以 50ms 间隔轮询 CharSkillbar + ptr48/ptr60
    ↓
检测到第一次结构变化（与 baseline_pre 的 diff 非空）
    ↓
再连续 2 次轮询 diff 稳定（变化趋于一致）
    ↓
此时记录为 cooldown_stable
    ↓
继续轮询，等待 ready_returned
```

设置最大等待上限为 `2000ms`，超时则记录 `cooldown_stable_timeout` 并继续流程。

***

**修复三：收紧 ready 检测条件**

当前 ready 检测只判断 ptr48/ptr60 地址是否回归，容易误触发。

**要求：**

ready 判定改为同时满足以下两个条件：

1. `ptr48` 和 `ptr60` 的指针地址与 `baseline_pre` 相同
2. 指针所指对象内，所有值 `< 0x10000` 的 u32 字段（技能 ID 候选）与 `baseline_pre` 完全相等

并且：**连续 2 次轮询（间隔 100ms）都满足以上条件**，才最终确认为 `ready_confirmed`。

将状态从原来的 `Completed` 改名为：

- `ready_confirmed`：严格条件满足
- `ready_candidate`：地址回归但字段未完全匹配（原来的 Completed）

***

**验证步骤**

三个修复完成后，对以下技能各采一次完整 trace，确认输出符合预期：

1. `Healing(6)` — CD 长，用于验证修复二效果
2. `Utility1(Z)` — 已知 primary branch，用于验证修复一
3. `Utility2(X)` — 已知 secondary branch，用于验证修复一的 secondary 情况

每份 trace 应包含：`baseline_pre` → `cooldown_stable`（动态锁定）→ `ready_confirmed` 或 `ready_candidate`，以及对应的 diff 文件。

