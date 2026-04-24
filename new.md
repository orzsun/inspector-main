# 项目汇总（KX Packet Inspector）

## 简介
KX Packet Inspector 是注入到 GW2 进程内用于捕获、解析、记录与逆向 Guild Wars 2 网络协议的工具集合。项目集成了实时 UI、内存读取、钩子注入、协议静态+动态分析流程与研究性文档。

## 已实现的核心能力（概要）

- **网络捕获与钩子**：稳定的 CMSG/SMSG 拦截（`MsgConn::FlushPacketBuffer`、`Msg::DispatchStream`），支持通用路径与 fast-path 的差异化处理。
- **协议解析链**：基于 schema 的静态解析与运行时发现流程（master dispatch table → schema → handler），大量 packet 文档已整理在 `docs/protocols/`。
- **实时可视化**：集成 ImGui（`libs/ImGui/`、`src/ImGuiManager.cpp`），提供数据包流、调试面板与内存观察面板。
- **Hook 管理与注入框架**：封装 Hook 管理（`src/HookManager.*`），集成 MinHook、SafetyHook、safetyhook 等，提供注入/IPC 文档（`docs/tooling/injection-and-ipc.md`）。
- **技能冷却追踪系统**：实现了 `SkillCooldownTracker`（`src/SkillCooldownTracker.*`）及自动化 trace 管线，用于将热键、CMSG 与内存快照关联并归档（`x64/Release/skill-trace/`）。
- **内存相关读写工具**：`SkillMemoryReader` 等组件用于解析 `CharSkillbar`、子对象指针（0x40/0x48/0x60）并做差异化对比。
- **逆向与证据收集**：丰富的工程化方法学（发现流程、断言字符串命名、reflection dumper 脚本等），并保留大量证据片段（`docs/architectural_evidence/`）。

## 文档与方法学（重要条目）

- `docs/README.md`：项目总览与入门。
- `docs/system-architecture.md`：三服务器架构与消息处理总览。
- `docs/protocols/`：已归档的 CMSG/SMSG 文档（game/login/portal/web_api）。
- `docs/methodologies/`：发现流程与实践指南（hooking、memory-correlation、SMSG/CMSG playbooks、skill-trace 自动化计划与发现结果）。
- `docs/engine_internals/`：引擎内部证据（ImGui、reflection 等）。
haolehao
## 技术细节要点

- Hook 与数据提取：`Msg::DispatchStream` 的中点 Hook（SafetyHook）用于可靠获取 SMSG 明文与 schema/handler 信息；`MsgConn::FlushPacketBuffer` hook 捕获 CMSG。关键偏移和 HandlerInfo 解读已记录在 docs。
- 动态发现流程：使用 logger 获取 Handler/Schema 地址 → 在静态分析工具（Ghidra）中反查结构 → 生成 Markdown 文档并加入 `docs/protocols/`。
- 内存模型与反射：已分析出 `hkReflect` 运行时结构与 Class/Field 布局，提供可复用的 reflection dump 脚本用于生成类型头文件。

## Skill 冷却与自动化调查（现状与发现）

- 已实现：基于 `CMSG_USE_SKILL (0x0017)` 的 archive-key 管线、热键确认 + 新鲜包过滤、自动化 trace（`baseline_pre` / `baseline_post` / `cooldown_stable` / `ready_returned` 等）。
- 主要发现：
  - `CharSkillbar` 顶层字段多为全局或阶段字段，真正的 per-skill 信息更可能位于 child objects（`+0x48` / `+0x60`）。
  - 单次指针回归不足以判定 ready；需要指针+字段值的多次稳定验证。
  - 固定 +500ms 的 `cooldown_stable` 容易过早，已改为分阶段轮询与变化检测逻辑。
  - 已稳定的 action-slot 映射（基于 fresh packets）：`tail_tag 0x29` 系列（如 `0x29:00` → Healing(6)、`0x29:01` → Utility1(Z)），以及部分 secondary（`0x25:83` → Utility2(X)）；部分 archive key 需用最后按键消歧（X/C 冲突）。

## 已完成的自动化与工具产出

- 自动 trace 输出结构化归档（`x64/Release/skill-trace/<profile>/<slot>/<tag_val>/<timestamp>/`），包含 `meta.json`、各阶段 dump 与 diff。
- `SkillMemoryReader`（原型）能通过线程 TLS 扫描或 pattern scan 定位 ContextCollection，并抓取 `CharSkillbar` 快照用于 diff。
- 日志系统（`x64/Release/cd-log/`）用于记录 `[USED]/[CD_START]/[CHARGE-]/[CHARGE+]/[READY]/[FULL]` 事件。

## 当前限制与已识别问题

- Ready 判定仍需增强：需要把地址回归与字段级别的稳定性结合，并做连续确认。
- 部分 archive key 存在冲突，需结合键盘 hook 的最后热键做消歧。
- 部分 fast-path SMSG（如 0x0001）需要专门 hook 才能完整捕获；现有 hook 覆盖大部分但非全部路径。
- 对 CD 的精确毫秒级恢复值尚未可靠推导，目前以状态机（`castable` / `cooldown_path_detected` / `cooldown_active` / `ready_confirmed` 占位）为主。

## 下一步建议（优先级）

1. 巩固 readiness 判定：实现“指针回归 + 字段完全匹配 + 连续 N 次”规则并替换旧的 permissive 判定。
2. 修正尾字段解析与 archive key 规则，统一自动化层与手工 UI 的 tail 解析实现。
3. 为 fast-path SMSG 增加专门 hook（如 0x0001），以覆盖所有 buff/agent 快速更新通路。
4. 将技能 CD 表与 GW2 API 拉取整合（api_cache + overrides），并把 build profile 写入 `build_profiles/` 供 UI 加载。

## 常用路径与参考文件

- 文档入口：[docs/README.md](docs/README.md#L1)
- 技能追踪设计與發現：[docs/methodologies/skill-execution-automation-plan.md](docs/methodologies/skill-execution-automation-plan.md#L1)
- Hook 與注入：[docs/tooling/injection-and-ipc.md](docs/tooling/injection-and-ipc.md#L1)
- Protocols：`docs/protocols/`（game / login / portal / web_api）
- 源碼入口：`src/`（HookManager, PacketParser, PacketProcessor, Skill* 等）

---

*本汇总基于工程内所有主要 Markdown 文档与现有代码（撰写于 2026-04-16）。如需我把 `new.md` 转为英文，或把其拆分为 `README_SUMMARY.md` + `DEVELOPER_GUIDE.md`，我可以继续处理。*
