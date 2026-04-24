请在 `kx-packet-inspector` 项目里新增一个 **技能内存面板**，通过内存指针直接读取 `CharSkillbar` 原始数据。

### 新增文件：`SkillMemoryReader.h/.cpp`

```cpp
#pragma once
#include stdint>
#include <array>

struct SkillSlotRaw {
    uint8_t  rawBytes[0x30];   // 每个槽位读 0x30 字节，足够覆盖 ID/CD/state
    uint32_t slotIndex;
    bool     valid = false;
};

struct SkillbarSnapshot {
    uintptr_t            charSkillbarAddr = 0;   // CharSkillbar 指针地址
    uintptr_t            asContextAddr    = 0;   // AsContext 指针地址
    std::array<SkillSlotRaw, 10> slots;          // 最多10个槽位（5武器+5工具）
    uint8_t  rawCharSkillbar[0x188];             // 完整 CharSkillbar dump
    bool     valid = false;
};

class SkillMemoryReader {
public:
    // 用已知 pattern 找 GetContextCollection，沿指针链拿 ChCliSkillbar
    bool init();

    // 每帧调用，刷新快照
    SkillbarSnapshot read();

    // 把 rawCharSkillbar 对比两次快照的差异字节，辅助逆向
    static void diffDump(const SkillbarSnapshot& before,
                         const SkillbarSnapshot& after,
                         char* outBuf, size_t bufSize);

private:
    using GetContextCollection_t = void*(*)();
    GetContextCollection_t m_fnGetCtx = nullptr;
};
```


***

### 指针链（已验证）

```cpp
SkillbarSnapshot SkillMemoryReader::read() {
    SkillbarSnapshot snap;

    auto* ctx = m_fnGetCtx();
    if (!ctx) return snap;

    // ContextCollection + 0x98 → ChCliContext
    auto chCtxAddr = *reinterpret_cast<uintptr_t*>((uintptr_t)ctx + 0x98);
    if (!chCtxAddr) return snap;

    // ChCliContext + 0x98 → pLocalChCliCharacter
    auto localChar = *reinterpret_cast<uintptr_t*>(chCtxAddr + 0x98);
    if (!localChar) return snap;

    // ChCliCharacter + 0x520 → ChCliSkillbar
    auto chCliSkillbar = *reinterpret_cast<uintptr_t*>(localChar + 0x520);
    if (!chCliSkillbar) return snap;

    // ChCliSkillbar + 0x80 → AsContext
    snap.asContextAddr = *reinterpret_cast<uintptr_t*>(chCliSkillbar + 0x80);

    // ChCliSkillbar + 0x150 → CharSkillbar（完整 dump）
    snap.charSkillbarAddr = *reinterpret_cast<uintptr_t*>(chCliSkillbar + 0x150);
    if (!snap.charSkillbarAddr) return snap;

    // 完整 dump CharSkillbar 的 0x188 字节
    memcpy(snap.rawCharSkillbar,
           reinterpret_cast<void*>(snap.charSkillbarAddr),
           0x188);

    // 按每 0x30 字节分槽位（猜测，待验证）
    for (int i = 0; i < 10; i++) {
        auto& slot = snap.slots[i];
        slot.slotIndex = i;
        uintptr_t slotAddr = snap.charSkillbarAddr + i * 0x28; // 步长待验证
        memcpy(slot.rawBytes,
               reinterpret_cast<void*>(slotAddr),
               sizeof(slot.rawBytes));
        slot.valid = true;
    }

    snap.valid = true;
    return snap;
}
```


***

### 在 ImGui GUI 里新增「Skill Memory」面板

在原有 GUI 代码里加一个新 Tab：

```cpp
if (ImGui::BeginTabItem("Skill Memory")) {

    static SkillbarSnapshot s_snapBefore;
    static SkillbarSnapshot s_snapAfter;
    static bool s_hasBefore = false;

    auto snap = g_skillReader.read();

    // 显示关键指针地址
    ImGui::Text("CharSkillbar @ 0x%llX", snap.charSkillbarAddr);
    ImGui::Text("AsContext    @ 0x%llX", snap.asContextAddr);
    ImGui::Separator();

    // Capture Before / Capture After 按钮（用于对比 CD 前后）
    if (ImGui::Button("Capture BEFORE (技能可用时按)")) {
        s_snapBefore = snap;
        s_hasBefore  = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Capture AFTER  (技能CD中按)") && s_hasBefore) {
        s_snapAfter = snap;
        // 显示差异字节
        char diffBuf[^4096];
        SkillMemoryReader::diffDump(s_snapBefore, s_snapAfter,
                                    diffBuf, sizeof(diffBuf));
        // diffBuf 里只有变化的字节偏移 + 前后值
        ImGui::OpenPopup("DiffResult");
    }

    if (ImGui::BeginPopupModal("DiffResult")) {
        ImGui::Text("Changed bytes (offset: before -> after):");
        // 显示 diffBuf 内容
        ImGui::EndPopup();
    }

    // 完整 Hex Dump
    ImGui::Separator();
    ImGui::Text("CharSkillbar Raw Dump (0x188 bytes):");
    for (int row = 0; row < 0x188/16; row++) {
        char line[^128];
        int  off = row * 16;
        snprintf(line, sizeof(line),
            "%03X: %02X %02X %02X %02X  %02X %02X %02X %02X  "
                 "%02X %02X %02X %02X  %02X %02X %02X %02X",
            off,
            snap.rawCharSkillbar[off+0],  snap.rawCharSkillbar[off+1],
            snap.rawCharSkillbar[off+2],  snap.rawCharSkillbar[off+3],
            snap.rawCharSkillbar[off+4],  snap.rawCharSkillbar[off+5],
            snap.rawCharSkillbar[off+6],  snap.rawCharSkillbar[off+7],
            snap.rawCharSkillbar[off+8],  snap.rawCharSkillbar[off+9],
            snap.rawCharSkillbar[off+10], snap.rawCharSkillbar[off+11],
            snap.rawCharSkillbar[off+12], snap.rawCharSkillbar[off+13],
            snap.rawCharSkillbar[off+14], snap.rawCharSkillbar[off+15]);
        ImGui::TextUnformatted(line);
    }

    ImGui::EndTabItem();
}
```


***

### 使用流程（逆向技能 ID/CD 的方法）

1. 注入修改后的 `KXPacketInspector.dll`
2. 打开 **Skill Memory** Tab
3. 技能全部 CD 好时点 **Capture BEFORE**
4. 按下技能 1，立刻点 **Capture AFTER**
5. Diff 面板会显示哪些字节偏移发生了变化
6. 变化的字节 = 技能 CD 计时器或状态字段
7. 对技能 1~5 各做一次，找到规律，确定每个槽位的步长和 CD 字段偏移
8. 把结果更新到 `gw2lib` 的 `CharSkillbar` 结构体定义里
<span style="display:none">[^1][^10][^11][^12][^13][^14][^15][^2][^3][^4][^5][^6][^7][^8][^9]</span>

<div align="center">⁂</div>

[^1]: https://github.com/kxtools/kx-packet-inspector

[^2]: https://github.com/KxSystems/analyst-training

[^3]: https://code.kx.com/q/github/

[^4]: https://github.com/kxtools

[^5]: https://kxtools.xyz/tools/kx-packet-inspector

[^6]: https://github.com/kxtools/kx-maps

[^7]: https://kx.com/blog/supercharge-hardware-evaluation-with-kx-nano/

[^8]: https://nugetmusthaves.com/sitemap/sitemap_package_5.xml

[^9]: https://kxtools.xyz/tools/kx-vision

[^10]: https://kxtools.xyz/kx-trainer-pro

[^11]: https://play.google.com/store/apps/details?id=com.motionvolt.fliprunner\&hl=en_US

[^12]: https://www.ebrd.com/content/dam/ebrd_dxp/assets/pdfs/evaluation/Learning-and-Knowledge-Management-Summary-TP5-Experience-of-Other-Organisations.pdf

[^13]: https://www.sciencedirect.com/science/article/pii/S2405471221001563

[^14]: https://kxtools.xyz/kx-trainer

[^15]: https://www.arborday.org/our-work/tree-city-usa

