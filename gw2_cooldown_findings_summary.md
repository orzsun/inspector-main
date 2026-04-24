# GW2 冷却链路当前结论汇总

## 1. 当前已确认的核心对象与事件

### 1.1 关键观察点
当前围绕这几个地址做定点观察是有效的：

- `char_skillbar`
- `ptr40_slot = char_skillbar + 0x40`
- `ptr48_slot = char_skillbar + 0x48`（很多时候与 `ptr40` 重复或带 tag）
- `ptr60_slot = char_skillbar + 0x60`
- `focus_slot_addr`

实际命中最有价值的是：

- `PTR40`
- `PTR60`

当前阶段：

- `SLOT` 价值低
- `N48` 价值低
- 主线是 `PTR40 / PTR60`

---

## 2. 已确认的 cooldown 相关结论

### 2.1 `ptr40_target + 0x20` = 初始 cooldown duration
这一点已经基本确认。

已验证样本：

- 技能为 **20s** 时：`ptr40_target+0x20 = 20000`
- 技能为 **32s** 时：`ptr40_target+0x20 = 32000`
- 另一轮里还抓到 **30000**，说明不同技能/状态会得到不同 duration snapshot

结论：

- `ptr40_target+0x20` 不是 remaining
- 它更像 **这段 cooldown state 创建时写入的 duration snapshot / 初始总时长（毫秒）**

### 2.2 `ptr40_target + 0x20` 不是递减 remaining
目前日志没有看到：

- `20000 -> ... -> 0`
- `32000 -> ... -> 0`

相反，看到的是：

- 早期这里是一个小整数（如 `20000` / `32000`）
- 后期这个位置可能变成指针，或者整个 cooldown state 被切换/重建

结论：

- 不要把 `ptr40_target+0x20` 当成“会慢慢归零的剩余冷却”
- 它更像 **初始 duration 字段**

---

## 3. `PTR40` / `PTR60` 的角色判断

### 3.1 `PTR40`
`PTR40` 是当前最早可抓到 cooldown 进入状态的位置。

在进入冷却时：

- `PTR40` 会先命中
- 能拿到 `ptr40_target`
- 其中 `ptr40_target+0x20` 会给出这段 cooldown 的初始 duration

当前可以把它理解成：

- **cooldown state 入口 / 初始 state 容器**

### 3.2 `PTR60`
`PTR60` 更像后续 state / ready 判定相关对象。

在后续日志里：

- `PTR60` 经常在 `PTR40` 之后命中
- `PTR60` 的对象里也会带着 duration 信息
- `PTR60` 更接近“冷却完成 / ready 恢复”的后续状态切换

当前可以把它理解成：

- **后续 cooldown state / ready 相关对象**

---

## 4. 已确认的状态事件

### 4.1 `enter_cd`
当前可以把这类事件当成“技能进入冷却”：

- `PTR40` 命中
- `current_state_candidate=cooling`
- `state_transition_candidate=enter_cd`

这已经能稳定作为：

- **进入 CD 的事件信号**

### 4.2 `leave_cd`
当前可以把这类事件当成“技能冷却结束 / 技能转好”：

- `PTR60` 命中
- `current_state_candidate=ready`
- `state_transition_candidate=leave_cd`

这已经能稳定作为：

- **CD 转好的事件信号**

### 4.3 当前可用的状态机
仅靠内存事件，当前已经能建立一个可用的冷却状态机：

- `PTR40 enter_cd` -> 技能进入冷却
- `PTR60 leave_cd` -> 技能冷却结束

即使暂时还没找到 remaining，也已经能做：

- 是否在 CD 中
- 何时进入 CD
- 何时转好

---

## 5. layer2 / 深层对象的结论

### 5.1 layer2 存在，并且会带上 duration
在多轮日志中已经看到：

- `ptr40_target+0x20/+0x28` 后期会指向下一层对象
- `ptr60_target+0x8` 也会指向下一层对象
- 在这个后续对象里，仍然能看到 duration 被保留

例如：

- `layer2 + 0x28 = 20000`

结论：

- duration 不仅存在于 `PTR40` 早期对象里
- 也会被带到后续 cooldown state 对象里

### 5.2 但 layer2 里还没明确找到 remaining
当前虽然已经扩展观察到：

- `+0x18`
- `+0x1C`
- `+0x20`
- `+0x24`
- `+0x28`
- `+0x2C`
- `+0x30`
- `+0x34`
- `+0x38`
- `+0x3C`
- `+0x40`

但还没有哪一个字段被明确验证为：

- 递减 remaining
- start tick
- end tick

当前结论：

- layer2 很重要
- 但 **remaining 还没被单独定位出来**

---

## 6. 关于 Cheat Engine 的结论

### 6.1 CE 读旧地址容易失效
已经验证出一个重要现象：

- `ptr40_target` 往往是运行时短生命周期对象
- 在 x64dbg/插件命中的那一瞬间可以读到正确值
- 过一会再用 CE 去看同一个旧地址，数据可能已经被改写/复用

因此：

- 不是优先怀疑“CE 被加密”
- 更合理的解释是：**对象已切换/失效**

### 6.2 CE 适合做辅助，不适合做主线
当前更适合的分工是：

- **x64dbg / 插件**：抓命中瞬间、抓对象链和状态切换
- **CE**：只做临时辅助观察

主线仍然应该放在：

- x64dbg 插件的瞬时快照

---

## 7. 关于减 CD buff（动态修正）的结论

## 7.1 当前最重要判断：减 CD 不是简单改初始 duration
基于对照测试，已经看到：

### 组 1：没有减 CD
- 初始 `ptr40_target+0x20 = 20000`
- `enter_cd -> leave_cd` 正常完成

### 组 2：通过 F2 / F3 / F4 在冷却过程中减 CD
- 第一段仍然能看到 `ptr40_target+0x20 = 20000`
- 说明 **初始 duration 没有直接被改写**
- 但中途会出现新的 state 切换，甚至新的 `PTR40`，并且新的 duration snapshot 可能变成别的值（如 `30000`）

结论：

- **减 CD 更像“动态修正 cooldown state”**
- 而不是简单把第一次 `ptr40_target+0x20` 这个值改小

## 7.2 更合理的模型
当前最合理的模型是：

- `ptr40_target+0x20` = 当前这段 state 被创建时的 duration snapshot
- F2/F3/F4 这类减 CD 技能会：
  - 触发新的 state 重建 / 切换
  - 或改变后续 ready 条件
  - 最终让 `leave_cd` 提前发生

换句话说：

- **减 CD 的实现更像“重建/切换 cooldown state”**
- 而不是“原地修改旧 state 的 duration 数值”

---

## 8. 基于文档线索得到的补强结论

从你给的 Discord / 文档线索里，有几个很关键的点：

### 8.1 Alacrity / 减 CD 是单独处理的
文档提到过：

- cooldown 处理因为 Alacrity 被移动到了 `AgentChar`
- `AgentChar` 暴露了 `cooldown_started` 和 `finished` 相关 signal
- `cooldown_end` 从 `SkillbarSkill` 上移除

这说明：

- cooldown 并不是简单静态字段
- 存在单独的 cooldown 管理层

### 8.2 还提到“找到一个告诉 client cd speed 的包”
这条线索非常重要，说明：

- 客户端端的冷却处理，可能会接收一个“冷却速度 / 冷却修正”的动态更新信号

这和我们当前的逆向结果高度一致：

- 减 CD 更像是 **动态修正 ready 时间 / cooldown speed**
- 而不是只写一次 duration

### 8.3 综合结论
文档与逆向结果一致支持：

- `ptr40_target+0x20` 只是初始 duration
- 真正受 Alacrity / 减 CD buff 影响的，是 **更高层 cooldown 管理逻辑**
- `leave_cd` 更接近最终 ready 判定

---

## 9. Remaining 问题的当前结论

### 9.1 Remaining 仍然需要，但暂时没定位到单一字段
你最终还是需要：

- 剩余冷却时间
- 倒计时
- 还剩多少 ms 才能释放

但目前还没有确认一个单独字段是：

- 始终递减的 remaining

### 9.2 Remaining 现在更适合按“状态机”思路解决
既然已经有：

- `enter_cd`
- `leave_cd`
- duration snapshot
- 中途 state 重建 / 切换

那么当前最可行的 remaining 方案是：

- 不是死找一个静态 remaining 偏移
- 而是做一个 **cooldown state 状态机**

模型可以是：

1. `enter_cd` 时创建当前 cooldown state
2. 记录该 state 的 duration snapshot
3. 中途如果 F2/F3/F4 导致新的 `PTR40/PTR60` / state 切换：
   - 认为 cooldown state 被动态修正
   - 刷新当前 state
4. `leave_cd` 时 remaining = 0

换句话说：

- **remaining 更可能通过“事件 + 状态机 + 当前 active state”计算出来**
- 而不是简单从一个固定偏移一直读到 0

---

## 10. 如果走内存路线，当前最优方案

### 10.1 内存路线是可行的
可以通过内存方式解决 cooldown 问题，但推荐方案不是：

- “继续死找一个固定 remaining 字段”

而是：

- **抓 cooldown state 切换**
- **抓每段 state 的 duration snapshot**
- **用状态机计算当前 remaining**

### 10.2 内存路线的最佳实践
当前最佳思路：

- `PTR40 enter_cd` -> 建立 state
- `ptr40_target+0x20` -> 该 state 的 duration snapshot
- 中途新的 `PTR40/PTR60` / 对象切换 -> 认为 state 被修正或重建
- `PTR60 leave_cd` -> 结束 state

这种方案最符合目前已有证据。

---

## 11. 如果走封包路线，当前最优方案

如果封包侧成立，重点也不应只是抓一个“cooldown_start”包。

更完整的模型应该是三段：

1. **开始包**
   - skill
   - 初始 duration

2. **中途修正包**
   - cooldown speed
   - cooldown modifier
   - ready time refresh

3. **结束包 / ready 包**
   - 冷却结束

因为基于当前所有结论：

- 关键不只是 start
- **真正最重要的是中途修正 cooldown 的那类信号**

也就是：

- 如果封包路线成立，真正该抓的是：
  - cooldown speed / modifier / refresh / finished

---

## 12. 当前阶段可以稳定使用的结论

目前可以稳定使用的、可信度很高的结论有：

### 已确认
- `PTR40` / `PTR60` 是主线
- `SLOT` / `N48` 当前价值低
- `ptr40_target+0x20` 是 cooldown duration snapshot
- `PTR40 enter_cd` 可作为进入冷却事件
- `PTR60 leave_cd` 可作为冷却结束事件
- duration 不是 remaining
- 减 CD / Alacrity 更像动态修正 cooldown state
- F2/F3/F4 这种减 CD 技能更像触发新的 cooldown state / ready 修正，而不是原地改旧 duration

### 未确认
- 单一 remaining 字段
- 单一 start_tick 字段
- 单一 end_tick 字段
- cooldown speed / modifier 的具体字段或具体包

---

## 13. 当前最推荐的方向

### 优先级 1
继续走 **内存状态机**：

- `enter_cd`
- `leave_cd`
- 每段 state 的 duration snapshot
- 中途修正触发的新 state

### 优先级 2
如果还要继续深挖：

- 继续围绕 layer2 / PTR60 后续对象找：
  - remaining
  - start_tick
  - end_tick
  - speed/modifier

### 优先级 3
如果转向封包：

重点找：

- cooldown start
- cooldown speed / modifier update
- cooldown finished / ready

而不是只找一个单独 start 包。

---

## 14. 一句话总总结

**当前已经确认：`ptr40_target+0x20` 是 cooldown 初始时长快照，`PTR40`/`PTR60` 能稳定给出 enter/leave CD 事件；减 CD buff 更像通过动态重建/切换 cooldown state 来生效，而不是简单修改第一次写入的 duration。最终的 remaining 更适合通过“内存事件 + 状态机”来解决，而不是继续死找一个固定递减偏移。**
