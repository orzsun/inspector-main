背景更新

状态机重命名已落地：fully_recharged → cooldown_path_detected，meta.json 新增 final_state / state_confidence / note 字段，命名修正已确认生效

当前 castable 语义可信，cooldown_path_detected 语义为「进入 CD 路径的状态切换」，不代表 CD 结束

下一阶段目标：开始解析 buff 相关 SMSG，为 Alacrity 检测做准备

本轮任务：SMSG buff 字段初步展开

任务一：对 SMSG 0x0028 做原始字段展开

SMSG_AGENT_ATTRIBUTE_UPDATE (0x0028) 已被头文件标注为 "agent stats, buffs/debuffs"，是当前最高优先级目标。

在 logger 或 UI 里增加一个专用的 0x0028 观察面板，对每个收到的包做以下展开：

text
[0x0028] AgentAttributeUpdate
  raw bytes: XX XX XX XX ...
  u16 @ 0:   agent_id candidate
  u32 @ 2:   field_A
  u32 @ 6:   field_B
  ... 后续每 4 字节一列打印
采集步骤：

进入战斗前记录一段 baseline（无 buff 状态）

获得 Alacrity 时记录（找一个有 Alacrity 输出的队友，或用会给自己 Alacrity 的技能）

Alacrity 消失时记录

对比三段日志，找出只在「有 Alacrity」时出现变化的字段

任务二：对 SMSG 0x0016 做同样展开

SMSG_AGENT_STATE_BULK (0x0016) 作为次优先级，同样做原始字段展开并与 0x0028 的时间线对齐。

重点关注：

包长度是否固定（判断是否为批量结构）

是否每次 Alacrity 上下线都伴随一个 0x0016 包

任务三：确认 0x0001 fast path 是否包含 buff 变化

SMSG_AGENT_UPDATE_BATCH (0x0001) 属于 fast path，当前 hook 可能无法捕获。

请确认：

0x0001 是否已经在当前 logger 里出现过

如果没有出现，是否因为走了 fast path 而被跳过

如果被跳过，评估为它增加专用 hook 的工作量（不要本轮实现，只需评估）

任务四：建立 Alacrity buff 的初步字段假设

完成 0x0028 展开后，根据观察结果填写以下表格（可以是初步猜测，标注置信度）：

text
Alacrity buff 字段假设表：
  包 opcode:       0x0028（待确认）
  buff_id 字段位置: offset XX, type u32, value XXXXXX
  apply/remove 区分: 字段 YY 为 1 = apply, 0 = remove（待确认）
  agent_id 位置:    offset ZZ（确认是否是自身 agent）
  置信度:           low / medium / high
本轮不需要做的事

不需要实现 Alacrity 对 CD 的实时修正逻辑（那是下一步）

不需要处理其他 buff（专注 Alacrity）

不需要修改状态机（已经稳定）

验收标准

0x0028 展开面板可以显示原始字段，已用 Alacrity 上下线事件验证

0x0016 同样展开，时间线与 0x0028 对齐记录

0x0001 fast path 情况评估已写入文档

Alacrity 字段假设表已填写（哪怕是低置信度）