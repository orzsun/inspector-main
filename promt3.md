<img src="https://r2cdn.perplexity.ai/pplx-full-logo-primary-dark%402x.png" style="height:64px;margin-right:32px"/>

# 还是不行

Position 0.0, CharSkillbar/AsContext 全是 0x0，问题很清楚：**`init()` 要么没被调用，要么 pattern 没找到，要么指针链的第一步就断了**。需要加可见的诊断信息。

***

## 给那个 Agent 的诊断 Prompt


***

**全链路诊断：Position 和所有指针均为 0，需要逐步确认哪一步失败。请做以下修改：**

### 第一步：在 GUI 显示每一步的指针值

把 Skill Memory 面板内容改成这样：

```cpp
if (ImGui::BeginTabItem("Skill Memory")) {

    // 显示 init 状态
    static bool s_initResult = false;
    static bool s_initTried = false;
    static uintptr_t s_dbgModuleBase = 0;
    static uintptr_t s_dbgFnAddr     = 0;
    static uintptr_t s_dbgCtxAddr    = 0;
    static uintptr_t s_dbgChCliCtx   = 0;
    static uintptr_t s_dbgLocalChar  = 0;
    static float     s_dbgPos[^3]     = {};

    if (ImGui::Button("Re-init & Probe") || !s_initTried) {
        s_initTried = true;

        // 模块基址
        HMODULE hMod = GetModuleHandleA("Gw2-64.exe");
        if (!hMod) hMod = GetModuleHandleA(nullptr);
        s_dbgModuleBase = (uintptr_t)hMod;

        // 重新 init
        s_initResult = g_skillReader.init();
        s_dbgFnAddr  = g_skillReader.getCtxFnAddr(); // 加一个 getter

        // 如果 init 成功，调用并追踪指针链
        if (s_initResult && s_dbgFnAddr) {
            using GetCtx_t = void*(*)();
            auto fn = (GetCtx_t)s_dbgFnAddr;
            auto ctx = fn();
            s_dbgCtxAddr = (uintptr_t)ctx;

            if (ctx) {
                s_dbgChCliCtx = *reinterpret_cast<uintptr_t*>(
                    s_dbgCtxAddr + 0x98);
                if (s_dbgChCliCtx) {
                    s_dbgLocalChar = *reinterpret_cast<uintptr_t*>(
                        s_dbgChCliCtx + 0x98);
                    if (s_dbgLocalChar) {
                        memcpy(s_dbgPos,
                            reinterpret_cast<void*>(s_dbgLocalChar + 0x480),
                            12);
                    }
                }
            }
        }
    }

    // 输出诊断
    ImGui::Text("Module base : 0x%llX", s_dbgModuleBase);
    ImGui::Text("init()      : %s",     s_initResult ? "OK" : "FAILED");
    ImGui::Text("FnAddr      : 0x%llX", s_dbgFnAddr);
    ImGui::Text("CtxColl*    : 0x%llX", s_dbgCtxAddr);
    ImGui::Text("ChCliCtx*   : 0x%llX", s_dbgChCliCtx);
    ImGui::Text("LocalChar*  : 0x%llX", s_dbgLocalChar);
    ImGui::Text("Position    : %.1f %.1f %.1f",
                s_dbgPos[^0], s_dbgPos[^1], s_dbgPos[^2]);

    ImGui::EndTabItem();
}
```


### 第二步：在 SkillMemoryReader 加 getter

```cpp
// SkillMemoryReader.h 加一行
uintptr_t getCtxFnAddr() const {
    return (uintptr_t)m_fnGetCtx;
}
```


### 第三步：如果 FnAddr = 0（pattern 没找到），改用这个备用方案

```cpp
// 备用：不用 pattern，直接读当前线程 TLS slot 0
// 已知 tlsIndex=0，ContextCollection* 在 tlsSlot + 0x8
bool SkillMemoryReader::initFallback() {
    uintptr_t tlsArrayPtr = 0;
    // 读 gs:[0x58] — Windows x64 TEB.ThreadLocalStoragePointer
    __asm {
        // MSVC inline asm 写法
    }
    // 或者用 intrinsic:
    tlsArrayPtr = (uintptr_t)__readgsqword(0x58);
    if (!tlsArrayPtr) return false;

    uintptr_t tlsSlot0 = *reinterpret_cast<uintptr_t*>(tlsArrayPtr + 0);
    if (!tlsSlot0) return false;

    uintptr_t candidate = *reinterpret_cast<uintptr_t*>(tlsSlot0 + 0x8);
    if (!candidate) return false;

    // 验证：读 +0x98 应该是合法指针
    uintptr_t chCliCtx = *reinterpret_cast<uintptr_t*>(candidate + 0x98);
    if (!chCliCtx) return false;

    m_pCtxCollection = reinterpret_cast<void*>(candidate);
    return true;
}
```

`__readgsqword(0x58)` 是 MSVC 内置函数，需要 `#include <intrin.h>`，**不需要内联汇编，完全合法**。

### 编译后把以下内容发过来：

1. `Module base` 的值（确认是不是 GW2 基址）
2. `init(): FAILED / OK`
3. `FnAddr` 的值
4. 如果 init FAILED，改用 `initFallback()` 后 `CtxColl*` 的值

<div align="center">⁂</div>

[^1]: image.jpg

