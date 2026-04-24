
**背景说明（必读）：**

之前用外部 EXE（Autoupdater）验证了以下指针链：

```
TLS slot 0 → tlsSlotValue + 0x8 = ContextCollection*
→ + 0x98 = ChCliContext*
→ + 0x98 = pLocalChCliCharacter*
→ + 0x480 = vec3MapPosition
```

**kx-packet-inspector 是注入进 GW2 进程内部运行的 DLL，不是外部进程。** 因此不需要 `ReadProcessMemory`，不需要 TEB/TLS 遍历，可以直接调用 `GetContextCollection()`。这是两个项目的根本区别。

***

**请重写 `SkillMemoryReader::init()` 和 `SkillMemoryReader::read()`，使用以下方案：**

### 第一步：`init()` — 找到 GetContextCollection 函数地址

```cpp
bool SkillMemoryReader::init() {
    // 在本进程内扫描 Gw2-64.exe 模块
    uintptr_t base = (uintptr_t)GetModuleHandleA("Gw2-64.exe");
    if (!base) base = (uintptr_t)GetModuleHandleA("Gw2.exe");
    if (!base) return false;

    // 获取模块大小
    MODULEINFO mi = {};
    GetModuleInformation(GetCurrentProcess(),
                         (HMODULE)base, &mi, sizeof(mi));

    // 用 hacklib 的 FindPattern（项目已有，直接调用）
    // Pattern 已验证，对应函数体：
    // 8B 0D [rip+off]  → mov ecx, [TLS index]
    // 65 48 8B 04 25 58 00 00 00  → mov rax, gs:[58h]
    // BA 08 00 00 00   → mov edx, 8
    // 48 8B 04 C8      → mov rax, [rax+rcx*8]
    // 48 8B 04 02      → mov rax, [rax+rdx]
    // C3               → ret
    auto result = hl::FindPattern(
        "\x8B\x00\x00\x00\x00\x00\x65\x48\x8B\x04\x25\x58\x00\x00\x00"
        "\xBA\x08\x00\x00\x00\x48\x8B\x04\xC8\x48\x8B\x04\x02\xC3",
        "x?????xxxxxxxxxxxxxxxxxxxxxxxxx",
        base, mi.SizeOfImage);

    if (!result) {
        HL_LOG_ERR("SkillMemoryReader: pattern NOT FOUND\n");
        return false;
    }

    m_fnGetCtx = result.as<GetContextCollection_t>();
    HL_LOG_RAW("SkillMemoryReader: GetContextCollection @ 0x%llX\n",
               (uintptr_t)m_fnGetCtx);

    // 立即验证：调用一次，确认返回非空
    auto* ctx = m_fnGetCtx();
    HL_LOG_RAW("SkillMemoryReader: ContextCollection* = 0x%llX\n",
               (uintptr_t)ctx);
    return ctx != nullptr;
}
```


***

### 第二步：`read()` — 直接调用并读技能数据

```cpp
SkillbarSnapshot SkillMemoryReader::read() {
    SkillbarSnapshot snap = {};
    if (!m_fnGetCtx) return snap;

    // 直接调用（DLL 注入，在进程内，完全合法）
    auto* ctx = m_fnGetCtx(); // = ContextCollection*
    if (!ctx) return snap;

    // → +0x98 = ChCliContext*
    auto* chCliCtx = *reinterpret_cast<void**>((uintptr_t)ctx + 0x98);
    if (!chCliCtx) return snap;

    // → +0x98 = pLocalChCliCharacter*
    auto localChar = *reinterpret_cast<uintptr_t*>((uintptr_t)chCliCtx + 0x98);
    if (!localChar) return snap;

    // → +0x520 = ChCliSkillbar*  (待验证偏移，优先试 0x520，备用 0x528/0x518)
    auto chCliSkillbar = *reinterpret_cast<uintptr_t*>(localChar + 0x520);
    if (!chCliSkillbar) return snap;

    // → +0x80 = AsContext*
    snap.asContextAddr = *reinterpret_cast<uintptr_t*>(chCliSkillbar + 0x80);

    // → +0x150 = CharSkillbar*
    snap.charSkillbarAddr = *reinterpret_cast<uintptr_t*>(chCliSkillbar + 0x150);
    if (!snap.charSkillbarAddr) return snap;

    // dump 完整 CharSkillbar
    memcpy(snap.rawCharSkillbar,
           reinterpret_cast<void*>(snap.charSkillbarAddr),
           sizeof(snap.rawCharSkillbar));

    snap.valid = true;
    return snap;
}
```


***

### 第三步：ChCliSkillbar 偏移验证方法

`+0x520` 是估计值，编译注入后在 GUI 里做以下验证：

1. `CharSkillbar @ 0x...` 地址**不为零** → 偏移正确
2. 在 GUI 里显示 `rawCharSkillbar[0x00]~[0x0F]` 的字节，如果全是 `00` 但地址非零，说明偏移多了或少了 `0x8`
3. 分别试 `0x518 / 0x520 / 0x528`，找到哪个偏移让 rawDump 出现非零字节

***

### 额外任务：同时在 GUI 输出角色坐标（验证指针链）

```cpp
// 验证用：读 vec3MapPosition（已知正确偏移）
float pos[^3] = {};
if (localChar) {
    // localChar + 0x480 = vec3MapPosition
    memcpy(pos, reinterpret_cast<void*>(localChar + 0x480), 12);
    ImGui::Text("Position: %.1f, %.1f, %.1f", pos[^0], pos[^1], pos[^2]);
}
```

**如果坐标显示正确，说明从 `GetContextCollection` 到 `pLocalChCliCharacter` 整条链路没问题，剩下只需要找正确的 ChCliSkillbar 偏移。**

<div align="center">⁂</div>

[^1]: https___github.com_orzsun_gw2assistant_tree_maste.md

