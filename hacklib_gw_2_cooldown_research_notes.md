# hacklib_gw2 项目中与 cooldown 相关的研究笔记

## 目标

从 `hacklib_gw2` 项目中提取所有**与技能冷却（cooldown / recharge）相关的线索**，判断：

1. 项目里是否已经存在可直接读取的 cooldown 数据路径
2. 如果没有，哪些模块可以作为进一步逆向/实现 cooldown 跟踪的入口
3. 下一步 agent 应该优先搜索哪些符号、结构、调用链

---

## 结论摘要

`hacklib_gw2` **不是一个现成的技能栏冷却解析器**。

当前已确认：

- 项目更偏向**对象状态采集 / 可视化 / hook 接口**
- 已有较完整的 **buff 持续时间建模**
- 已有 **combat log / damage log hook**
- 已有角色资源值、职业状态等上下文数据
- **没有发现**直接暴露的 `skillbar` / `slot` / `cooldown` / `recharge` / `ammo` 接口

因此，这个项目对 cooldown 研究的价值主要在于：

- 提供一种**时间状态建模方式**（buff 剩余时间）
- 提供一种**事件驱动入口**（combat log hook）
- 提供角色资源、职业、姿态等**技能上下文状态**

但它**不能直接回答**“某个技能槽位当前还剩多少冷却”。

---

## 已确认的 cooldown 相关线索

### 1. Buff 剩余时间系统是当前最接近 cooldown 的现成实现

在 `CharacterData` 中存在：

- `buffDataList`
- `buffTimeList`

说明项目已经维护了一套“效果剩余时间”系统。

### 关键接口

- `Character::GetBuffTimeLeft()`
- `CharacterData::AddBuff()`

### 逻辑摘要

`GetBuffTimeLeft()` 的逻辑：

- 从 `buffTimeList[type]` 中取结束时间 `end`
- 读取当前时间戳 `now`
- 返回 `end - now`

`AddBuff()` 的逻辑：

- `dur == -1` 直接跳过
- `end = applyTime + duration`
- 若已有该 effect：
  - `PROGRESSIVE`：取更晚的结束时间
  - 否则：叠加持续时间
- 回写 `buffTimeList[effectType]`

### 启发

这说明作者的思路是：

> 时间类战斗状态 = 维护结束时间戳，再用当前时间差计算剩余时长

这和 cooldown 的实现思路高度相似。

### 对 agent 的意义

如果后续要自己做 cooldown tracking，优先复用这套模型：

- 找到技能触发时间 / 冷却结束时间来源
- 存成 `end_timestamp`
- 用当前时间戳计算剩余冷却

---

## 2. Combat log hook 是事件驱动 cooldown 的入口

项目中暴露了：

- `HOOK_DAMAGE_LOG`
- `HOOK_COMBAT_LOG`

hook 回调可获得的信息包括：

- `src`
- `tgt`
- `hit`
- `CombatLogType`
- `EffectType`

### 启发

虽然这里没有直接给出技能槽位冷却字段，但它提供了一个重要能力：

> 能在战斗事件发生时收到内部事件通知

这意味着可以尝试：

- 在技能释放时捕获对应 combat/event
- 识别技能或 effect
- 以该事件作为冷却开始信号
- 自己维护技能 cooldown timer

### 对 agent 的意义

如果走**事件推导型 cooldown** 路线，应优先研究：

- `HOOK_COMBAT_LOG`
- `CombatLogType`
- `EffectType`
- 事件与技能释放之间的对应关系

---

## 3. 项目可读到角色资源值，但这不是直接 cooldown

项目中可以读取：

- `currentEnergy / maxEnergy`
- `currentEndurance / maxEndurance`
- `currentHealth / maxHealth`
- `gliderPercent`
- `breakbarPercent`
- `stance`
- `profession`

### 启发

这些值不是冷却本身，但可作为技能状态上下文：

- 某些职业技能受能量影响
- endurance 与闪避类行为相关
- stance / profession 会影响技能栏内容或技能可用性

### 对 agent 的意义

如果后面要做“技能可用性判断”而不是纯 cooldown 数字，这些字段值得一并纳入。

---

## 4. 存在对 buff/effect 对象的实验性探索痕迹

在 `GameHook()` 附近存在被注释掉的实验代码，大意是：

- 调某个函数 `f1(0xC27D)`
- 取回指针
- 读取内部值
- 打日志

### 启发

这说明作者曾尝试：

> 通过 effect/buff id 反查内部对象或内部数据结构

这对 cooldown 的研究很重要，因为它提示另一条路线：

- 不一定先找 skillbar UI
- 可以先找技能触发后关联的 effect / buff / combat event
- 再由 effect id 反推内部结构

---

## 目前没有发现的内容

以下内容在当前项目中**未发现现成接口**：

- 技能栏对象（skillbar）
- 技能槽位映射（slot 1~10 / heal / utility / elite）
- 技能 ID 到槽位的直接对应
- recharge / cooldown / ammo / charge
- cast bar / queue / aftercast 状态
- 技能按钮 UI 读数

### 结论

当前项目**更支持**：

- Buff 时间跟踪
- 事件驱动状态跟踪
- 角色资源/姿态信息读取

当前项目**不支持现成的技能栏冷却直读**。

---

## 对 cooldown 研究的两条可行路线

### 路线 A：事件推导型 cooldown

思路：

1. 用 combat log / effect event 检测技能触发
2. 建立“技能事件 -> 技能 ID / effect ID”的映射
3. 查询或维护技能基础 cooldown 表
4. 在本地创建 `cooldown_end_timestamp`
5. 实时计算剩余冷却

#### 优点

- 不必先找到复杂 skillbar 内存结构
- 更容易先做出可运行原型
- 与当前项目现有能力更贴近

#### 缺点

- 需要自己维护技能冷却数据库
- 处理减 CD、重置、武器切换、特性修正时会变复杂
- 对 event 与技能的映射准确性要求高

---

### 路线 B：内存直读型 cooldown

思路：

1. 找到技能栏/技能槽对象
2. 定位槽位里的技能 ID、状态、recharge 字段
3. 直接读取每个槽位的剩余冷却

#### 优点

- 理论上更接近游戏真实状态
- 可直接读取动态修改后的冷却
- 更容易支持充能、武器切换、特性影响

#### 缺点

- 这个项目里几乎没有现成线索
- 需要依赖别的仓库或直接逆向客户端
- 实现难度明显更高

---

## 对本项目的总体判断

### 适合的用途

- 学习项目如何建模“剩余时间”
- 研究 buff 时间的存储与更新方式
- 研究 combat log hook 的接入点
- 获取角色资源、职业、姿态等运行时上下文

### 不适合的用途

- 直接抄出技能栏 cooldown 实现
- 直接读取某个技能槽的剩余冷却
- 直接定位 skill slot -> skill id -> recharge 字段

---

## agent 下一步建议搜索清单

以下搜索应该优先在本项目及相关 GW2 项目中执行。

### 一、继续在本项目内扩展搜索

优先搜索关键词：

```text
cooldown
recharge
skill
skillbar
slot
ability
cast
activation
charge
ammo
hotbar
weapon skill
utility skill
heal skill
elite skill
```

同时搜索可能的时间状态关键词：

```text
duration
timestamp
applyTime
endTime
remaining
usable
available
trigger
effect
```

---

### 二、优先关注的结构和调用链

重点查看：

1. `CharacterData`
2. `BuffData`
3. `GameHook()`
4. `HOOK_COMBAT_LOG`
5. `HOOK_DAMAGE_LOG`
6. 所有与 `EffectType` 相关的枚举和分发逻辑
7. 所有以时间戳、持续时间、事件回调更新状态的代码

---

### 三、尝试建立“技能触发 -> effect/buff”映射

agent 可做的事：

1. 收集某一技能释放时的 combat log / effect event
2. 记录对应 `EffectType`
3. 观察是否会产生固定 buff/effect 模式
4. 判断这些 effect 是否能作为 cooldown 启动信号

---

### 四、如果目标是准确 cooldown，转向其他来源

应补充研究：

- 其他 GW2 内存项目
- 与 `skillbar` / `ability` / `recharge` 相关的仓库
- 直接逆向客户端/相关 DLL
- UI 或战斗逻辑中与技能栏刷新有关的调用点

---

## 建议的执行顺序

### 阶段 1：低成本验证

1. 确认 `HOOK_COMBAT_LOG` 是否能稳定捕获技能触发事件
2. 找几个固定技能，记录事件序列
3. 判断事件是否足够作为 cooldown 起点

### 阶段 2：建立原型

1. 维护一张技能基础 cooldown 表
2. 技能触发时记录 `cooldown_end_timestamp`
3. 用现有 buff 时间模型思路计算剩余冷却

### 阶段 3：提高准确度

1. 处理减 CD、重置、职业特性影响
2. 处理武器切换与技能替换
3. 若误差较大，再转向内存直读 skillbar 结构

---

## 最终判断

`hacklib_gw2` 提供的最重要 cooldown 线索不是现成的“recharge 字段”，而是：

1. **Buff 剩余时间的建模方式**
2. **Combat/event hook 的事件入口**
3. **角色资源与职业状态上下文**

因此，agent 应把这个项目视为：

> “做事件驱动 cooldown 跟踪的参考底座”，而不是“现成的技能栏冷却读取器”。

---

## 给 agent 的一句话任务定义

请不要假设 `hacklib_gw2` 已经有技能 cooldown 字段；应优先把它当成一个 **buff/event/time-state 框架** 来利用，并基于 combat log + 时间戳模型实现 cooldown 原型；只有在事件路线精度不足时，再转向 skillbar 内存直读。

