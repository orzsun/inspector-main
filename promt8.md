本轮任务：拆分 ready 状态机，区分 castable 与 fully_recharged

任务一：识别技能是否为多充能技能

在 trace 启动时，根据已知映射或 GW2 API 查询，标记当前技能属于以下哪类：

text
skill_type = "single_charge"   // 单次 CD 技能（Healing、Utility 等）
skill_type = "multi_charge"    // 多充能技能（Weapon4、Elite 等）
暂时可以用手动配置表先解决，例如：

json
{
  "tag08_val00": "multi_charge",   // Weapon4(4)
  "tag04_val00": "multi_charge",   // Elite(Q)
  "tag29_val00": "single_charge",  // Healing(6)
  "tag29_val01": "single_charge",  // Utility1(Z)
  "tag25_val83": "single_charge"   // Utility2(X)
}
任务二：将 ready 状态拆分为两个独立状态

将原来的 ready_confirmed 和 ready_candidate 替换为以下三个状态：

text
castable
  含义：技能当前可以释放（至少还有 1 层 charge，或 CD 结束）
  判定：ptr48/ptr60 地址回归到任意「已知可释放」基准状态
  适用：multi_charge 技能的中间状态

fully_recharged
  含义：技能完全恢复到初始 baseline 状态（所有 charge 回满 / CD 归零）
  判定：ptr48/ptr60 地址 + 字段值与 baseline_pre 完全一致，连续 2 次轮询满足
  适用：single_charge 技能的最终状态，multi_charge 技能的全回满状态

cooldown_active
  含义：技能当前不可释放（charge 耗尽且 CD 未结束）
  判定：ptr48/ptr60 状态与 baseline_pre 明显不同，且技能不可激活
对 single_charge 技能：状态流为 cooldown_active → fully_recharged
对 multi_charge 技能：状态流为 castable（剩余 charge）→ cooldown_active（全耗尽）→ castable（部分恢复）→ fully_recharged（全满）

任务三：multi_charge 技能的 baseline 重新定义

对多充能技能，baseline_pre 不能只采一次，需要区分：

text
baseline_full    // 所有 charge 满时的内存状态（在技能栏 UI 显示满格时采集）
baseline_one_left  // 仅剩 1 层 charge 时的状态（可选，用于更精细区分）
请在 trace 启动时，如果检测到是 multi_charge 技能，额外标注当前是否处于满 charge 状态，并将采集结果存为 baseline_full.txt（与原 baseline_pre.txt 区分）。

验证步骤

完成后，对以下情况各采一次完整 trace 验证状态机行为：

Weapon4(4)：连按两次，观察是否正确停在 castable 而非 fully_recharged

Elite(Q)：连按两次，同上

Healing(6)：按一次，观察是否正确走到 fully_recharged

Utility1(Z)：按一次，同上

每份 trace 的 meta.json 应包含 skill_type 和最终 ready_state 字段。

使用 Claude Sonnet 4.6 Thinking 准备
当前这轮日志说明，现有状态机里的 fully_recharged 命名与游戏内实际语义不一致。结合最新 trace，castable 基本可以稳定理解为“技能当前还能释放”，这一点在多充能技能上尤其明显；例如 Weapon4(4) 在一次释放后虽然已经消耗了一层 charge，但仍然可以继续释放，此时更适合归类为 castable。相反，当前代码里被标成 fully_recharged 的那类样本，并没有充分证据表明它代表“技能已经完全回满”或“冷却彻底结束”。从游戏内观察和 trace 行为来看，这个状态更像是“技能开始进入完整 CD 流程”或“进入了不可继续释放的那条冷却路径”，因此 fully_recharged 这个名字会误导后续分析。
更准确地说，现阶段我们已经能比较可靠地区分出一种“仍可释放”的状态，但还不能把另一种状态解释为“真正回满”。日志更支持这样一种结论：当前 castable 对应“至少还有 1 层 charge，或者当前仍可按下释放”，而当前被命名为 fully_recharged 的状态，实际上更接近“开始进入 CD”或“进入完整冷却相关状态切换”的阶段，而不是“冷却完全恢复”。因此，下一步更合理的做法不是继续沿用 fully_recharged 这个名字，而是先改成一个中性且不误导的命名，例如 post_cooldown_transition 或 cooldown_path_detected，等我们真正抓到“技能图标重新亮起 / charge 回满”的稳定信号后，再定义真正意义上的 fully_recharged。

这个分析非常准确，你找到了一个经典的「命名超前于证据」的问题。直接给你新 prompt：

背景更新

最新 trace 日志揭示了一个命名语义问题：fully_recharged 这个状态名称与实际观测到的行为不符。

当前证据只能支持以下解释：

castable：技能当前仍可释放（至少有 1 层 charge，或尚未进入完整 CD）✅ 语义可信

fully_recharged：命名过于乐观，实际观测到的更像是「进入完整 CD 路径的状态切换」，而非「CD 彻底结束、技能回满」❌ 语义不准确

本轮任务：重命名状态，对齐证据

任务一：将 fully_recharged 重命名为 cooldown_path_detected

在所有代码、日志、meta.json、diff 文件名中，将 fully_recharged 统一替换为 cooldown_path_detected。

新的语义定义：

text
cooldown_path_detected
  含义：检测到技能进入了完整 CD 相关的状态切换路径
  不代表：CD 已结束、技能已回满、charge 已恢复
  实际意义：这是 CD 流程的起点信号，不是终点信号
保留 castable 不变，语义维持「当前仍可释放」。

完整状态枚举更新为：

text
castable              // 仍可释放（charge 未耗尽）
cooldown_path_detected // 进入 CD 路径（charge 耗尽 / 开始完整冷却）
cooldown_active       // CD 进行中（过渡状态，暂时保留）
ready_confirmed       // 保留占位，暂不使用，等待真实"回满"信号确认后再定义
任务二：在 meta.json 中增加 state_confidence 字段

为每个 trace 的最终状态增加可信度标记，避免后续分析误用：

json
{
  "final_state": "cooldown_path_detected",
  "state_confidence": "transition_only",
  "note": "State indicates CD path entry, not CD completion. fully_recharged not yet observable."
}
state_confidence 枚举值：

"confirmed"：有充分证据支持的状态（目前只有 castable 达到此级别）

"transition_only"：只能证明发生了状态切换，不能证明切换的方向或终态

"inferred"：基于间接证据的推断，需要后续验证

任务三：更新注释和文档

在代码中所有涉及原 fully_recharged 的注释旁，补充一行：

cpp
// NOTE: This state was previously named fully_recharged.
// Current evidence only supports "CD path entry detected", not "skill ready again".
// Do not interpret this as cooldown completion until a verified ready signal is found.
不需要做的事（本轮范围外）

不需要寻找真正的「回满」信号（那是下一阶段的工作）

不需要改变检测逻辑，只改命名和语义标注

不需要重新采集 trace，现有 trace 重命名即可

验收标准

完成后检查：

代码中不再出现 fully_recharged 字符串

现有已归档 trace 的 meta.json 里的状态名已批量更新

castable 对应样本的 state_confidence = "confirmed"

cooldown_path_detected 对应样本的 state_confidence = "transition_only"