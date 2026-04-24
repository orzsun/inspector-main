
**问题确认：**

- Pattern scan 正确找到 `GetContextCollection @ 0x7FF704EA6B90` ✓
- 但从当前线程调用该函数返回 null，因为 TLS slot 在当前线程未初始化
- `init()` 不应该调用函数验证，`read()` 也不能直接调用（线程不对）

**请做以下修改：**

### 修改一：`init()` 只找函数地址，不做调用验证

```cpp
bool SkillMemoryReader::init() {
    HMODULE hMod = GetModuleHandleA("Gw2-64.exe");
    if (!hMod) hMod = GetModuleHandleA(nullptr);
    if (!hMod) return false;

    MODULEINFO mi = {};
    GetModuleInformation(GetCurrentProcess(), hMod, &mi, sizeof(mi));
    uintptr_t base = (uintptr_t)hMod;

    auto result = hl::FindPattern(
        "\x8B\x00\x00\x00\x00\x00\x65\x48\x8B\x04\x25\x58\x00\x00\x00"
        "\xBA\x08\x00\x00\x00\x48\x8B\x04\xC8\x48\x8B\x04\x02\xC3",
        "x?????xxxxxxxxxxxxxxxxxxxxxxxxx",
        base, mi.SizeOfImage);

    if (!result) return false;

    m_fnGetCtx = result.as<GetContextCollection_t>();

    // 不在这里调用！只记录地址
    HL_LOG_RAW("SkillMemoryReader: fn @ 0x%llX\n", (uintptr_t)m_fnGetCtx);
    return m_fnGetCtx != nullptr;  // 只要找到地址就算成功
}
```


### 修改二：`read()` 改为进程内多线程 TLS 扫描获取 ContextCollection*

不调用函数，直接扫描所有线程的 TLS，和 Autoupdater 外部版本逻辑相同，但使用 in-process API：

```cpp
static uintptr_t s_cachedCtx = 0;

static uintptr_t FindContextCollection() {
    if (s_cachedCtx) {
        // 验证缓存仍然有效（读 +0x98 应非零）
        uintptr_t chk = *reinterpret_cast<uintptr_t*>(s_cachedCtx + 0x98);
        if (chk) return s_cachedCtx;
        s_cachedCtx = 0;
    }

    DWORD pid = GetCurrentProcessId();

    // 动态加载 NtQueryInformationThread
    using NtQIT_t = NTSTATUS(NTAPI*)(HANDLE, ULONG, PVOID, ULONG, PULONG);
    static auto NtQueryInformationThread = (NtQIT_t)GetProcAddress(
        GetModuleHandleA("ntdll.dll"), "NtQueryInformationThread");
    if (!NtQueryInformationThread) return 0;

    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return 0;

    THREADENTRY32 te = { sizeof(THREADENTRY32) };
    if (!Thread32First(hSnap, &te)) {
        CloseHandle(hSnap);
        return 0;
    }

    uintptr_t result = 0;
    do {
        if (te.th32OwnerProcessID != pid) continue;

        HANDLE hThread = OpenThread(THREAD_QUERY_INFORMATION, FALSE,
                                     te.th32ThreadID);
        if (!hThread) continue;

        // 获取 TEB 地址
        struct { PVOID ExitStatus; PVOID TebBaseAddress; } tbi = {};
        NtQueryInformationThread(hThread, 0, &tbi, sizeof(tbi), nullptr);
        CloseHandle(hThread);

        uintptr_t tebAddr = (uintptr_t)tbi.TebBaseAddress;
        if (!tebAddr) continue;

        // TEB + 0x58 = ThreadLocalStoragePointer
        uintptr_t tlsArrayPtr = *reinterpret_cast<uintptr_t*>(tebAddr + 0x58);
        if (!tlsArrayPtr) continue;

        // TLS slot 0
        uintptr_t tlsSlot0 = *reinterpret_cast<uintptr_t*>(tlsArrayPtr + 0);
        if (!tlsSlot0) continue;

        // ContextCollection* 在 slot + 0x8（已由 Autoupdater 验证）
        uintptr_t candidate = *reinterpret_cast<uintptr_t*>(tlsSlot0 + 0x8);
        if (!candidate) continue;

        // 验证：+0x98 = ChCliContext*，应为有效指针
        uintptr_t chCliCtx = *reinterpret_cast<uintptr_t*>(candidate + 0x98);
        if (!chCliCtx) continue;

        result = candidate;
        break;

    } while (Thread32Next(hSnap, &te));

    CloseHandle(hSnap);
    s_cachedCtx = result;
    return result;
}

SkillbarSnapshot SkillMemoryReader::read() {
    SkillbarSnapshot snap = {};

    uintptr_t ctx = FindContextCollection();
    if (!ctx) return snap;

    uintptr_t chCliCtx   = *reinterpret_cast<uintptr_t*>(ctx + 0x98);
    if (!chCliCtx) return snap;

    uintptr_t localChar  = *reinterpret_cast<uintptr_t*>(chCliCtx + 0x98);
    if (!localChar) return snap;

    // 坐标验证
    memcpy(snap.debugPos, reinterpret_cast<void*>(localChar + 0x480), 12);

    // ChCliSkillbar（偏移待确认）
    uintptr_t chCliSkillbar = *reinterpret_cast<uintptr_t*>(localChar + 0x520);
    if (!chCliSkillbar) return snap;

    snap.asContextAddr    = *reinterpret_cast<uintptr_t*>(chCliSkillbar + 0x80);
    snap.charSkillbarAddr = *reinterpret_cast<uintptr_t*>(chCliSkillbar + 0x150);
    if (!snap.charSkillbarAddr) return snap;

    memcpy(snap.rawCharSkillbar,
           reinterpret_cast<void*>(snap.charSkillbarAddr),
           sizeof(snap.rawCharSkillbar));
    snap.valid = true;
    return snap;
}
```


### 修改三：GUI 诊断加 CtxColl 地址显示

```cpp
ImGui::Text("CtxColl* : 0x%llX", FindContextCollection());
```

**编译后运行，把 Position 和 CtxColl* 的值发过来。**
<span style="display:none">[^1]</span>

<div align="center">⁂</div>

[^1]: image.jpg

