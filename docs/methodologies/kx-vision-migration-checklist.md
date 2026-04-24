# KX-Vision 可迁移能力清单

本文总结 `C:\Users\Charlie\Desktop\gw2\kx-vision-main` 中对当前 `kx-packet-inspector` 最有价值的能力，并标注哪些已经同步落地。

## 已同步落地

### 1. Capture/Store 再由 UI 消费

来源：
- `docs/concepts/multithreading-model.md`
- `docs/engine_internals/game-thread-hook.md`

当前项目中的落地点：
- `GameThreadHook` 在游戏逻辑线程抓取快照
- `SkillMemoryReader` 持有最近一次有效快照缓存
- UI 侧通过 `readNoWait()` 非阻塞读取缓存/队列，不再在 Present 路径里等待未来捕获

价值：
- 避免 UI / Present 线程因为等待 gameplay 数据而卡死
- 与 GW2 的 TLS / 线程模型保持一致

### 2. 高层入口优先：ContextCollection

来源：
- `docs/engine_internals/data-access-patterns.md`

当前项目中的落地点：
- 使用已验证的 `GetContextCollection` getter pattern
- 解析链以 `ContextCollection -> ChCliContext -> LocalCharacter -> Skillbar` 为主

价值：
- 比低层枚举更稳定
- 更适合继续扩展技能、角色状态、目标状态等功能

### 3. 启动期初始化，而不是等 UI 首次打开才初始化

来源：
- `docs/engine_internals/game-thread-hook.md`
- `docs/concepts/multithreading-model.md`

当前项目中的落地点：
- `MainThread` 在 hooks 初始化后立即调用 `g_skillReader.init()`

价值：
- 避免第一次打开 `Skill Memory` 标签时才触发 pattern scan / hook install
- 减少 UI 首次打开时的卡顿风险

## 已吸收但仍需继续打磨

### 4. Hook 路径日志

来源：
- `docs/engine_internals/game-thread-hook.md`

当前状态：
- 已写入 `kx_gth_install.log`
- 已记录 getter 定位、hook 安装、对象链解析、关键空指针失败点

后续建议：
- 增加节流/分级日志，避免高频帧路径产生过多 I/O

### 5. 高层数据访问统一层

来源：
- `docs/engine_internals/data-access-patterns.md`

当前状态：
- `SkillTraceCoordinator` / `Skill Memory` 已经在使用 `g_skillReader`

后续建议：
- 将更多角色状态、目标状态、buff / cooldown 读取统一收敛到同一访问层

## 适合后续扩展

### 6. 单位系统 / 距离换算

来源：
- `docs/engine_internals/unit-systems.md`

适合的场景：
- 位置、位移、技能范围、实体距离等分析

### 7. 双模式构建

来源：
- `docs/dual-mode-build.md`

适合的场景：
- 后续需要同时支持独立 DLL 注入与 GW2AL 插件化加载

## 当前不建议优先迁移

- ESP 可视化逻辑
- MumbleLink 为主的数据通道
- 玩家/NPC/物件可视化展示层

这些更适合独立 overlay 工具，不是当前 `kx-packet-inspector` 的核心路线。
