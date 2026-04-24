# 获取当前角色装备/武器的实现说明

## 结论

当前角色“带了什么装备/武器”这件事，**更适合直接从游戏内存读取**，而不是依赖 MumbleLink。

如果目标只是：

- 读取自己当前主副手武器
- 读取自己当前装备槽
- 在本地 UI 面板里展示

那么不需要做世界坐标投影，也不需要先接 MumbleLink 相机数据。

---

## 为什么不是先靠 MumbleLink

`kx-vision` 虽然有 `MumbleLink Integration`，但它的 gear inspection 并不是靠 MumbleLink 直接给装备数据。

MumbleLink 更适合提供：

- 角色名
- profession
- elite spec
- race
- map id
- camera / avatar position
- 一些 UI / mount / combat 状态

这些信息适合做：

- ESP
- 屏幕投影
- 基础角色状态同步

但**不适合直接回答“当前穿了什么装备、拿了什么武器”**。

---

## `kx-vision` 是怎么做 gear inspection 的

`kx-vision` 的 gear inspection 核心依赖的是**内存结构读取**。

关键文件：

- [README.md](/C:/Users/Charlie/Desktop/gw2/kx-vision-main/README.md)
- [src/Game/SDK/EquipmentStructs.h](/C:/Users/Charlie/Desktop/gw2/kx-vision-main/src/Game/SDK/EquipmentStructs.h)
- [src/Game/Extraction/EntityExtractor.cpp](/C:/Users/Charlie/Desktop/gw2/kx-vision-main/src/Game/Extraction/EntityExtractor.cpp)
- [src/Game/Data/PlayerGearData.h](/C:/Users/Charlie/Desktop/gw2/kx-vision-main/src/Game/Data/PlayerGearData.h)

它的核心思路是：

1. 拿到角色对象 `ChCliCharacter`
2. 从角色对象拿到 `ChCliInventory`
3. 从 `ChCliInventory` 里的装备数组读取各个装备槽
4. 每个槽位读取 `ItCliItem`
5. 再从 `ItCliItem` 里拿：
   - `ItemDef`
   - `StatGear`
   - `StatWeapon`

---

## `kx-vision` 已确认的关键结构

根据 [EquipmentStructs.h](/C:/Users/Charlie/Desktop/gw2/kx-vision-main/src/Game/SDK/EquipmentStructs.h)：

- `NUM_EQUIPMENT_SLOTS = 69`
- `ChCliInventory::EQUIPMENT_ARRAY = 0x160`
- `ItCliItem::ITEM_DEF = 0x40`
- `ItCliItem::LOCATION_TYPE = 0x48`
- `ItCliItem::DATA_PTR = 0x58`
- `ItCliItem::STAT_GEAR = 0xA0`
- `ItCliItem::STAT_WEAPON = 0xA8`

这说明 `kx-vision` 已经验证过一条可工作的装备读取路线：

`ChCliCharacter -> ChCliInventory -> EquipmentArray -> ItCliItem -> ItemDef / Stat`

---

## 当前项目已经具备的前置条件

我们当前项目里已经能稳定走到：

`GetContextCollection() -> pChCliContext(+0x98) -> pLocalChCliCharacter(+0x98)`

并且还已经能继续拿到：

- `pChCliSkillbar(+0x520)`
- `pCharSkillbar(+0x150)`
- `pEnergies(+0x3D8)`

也就是说，我们已经有了：

- 稳定的 `GetContextCollection` pattern
- GTH / 上下文解析
- 本地角色对象 `pLocalChCliCharacter`
- 内存安全读取框架
- 本地日志与 UI 展示能力

**真正还没补上的，是从 `pLocalChCliCharacter` 继续找到 `ChCliInventory` 并读取装备数组。**

---

## 对当前项目最直接的实现路线

### 阶段 1：先读当前武器槽

优先目标：

- 主手武器组 1
- 副手武器组 1
- 主手武器组 2
- 副手武器组 2

原因：

- 这比完整 armor/trinket 更容易验证
- 对技能栏自动识别最有帮助
- 可以先直接回答“当前带的是什么武器组”

建议步骤：

1. 在当前项目里补一个 `Inventory/Equipment` 读取模块
2. 从 `pLocalChCliCharacter` 继续找 `ChCliInventory`
3. 按 `kx-vision` 的 `EQUIPMENT_ARRAY = 0x160` 读取槽位
4. 先只读取武器相关槽：
   - `MainhandWeapon1`
   - `OffhandWeapon1`
   - `MainhandWeapon2`
   - `OffhandWeapon2`
5. 读取：
   - `itemId`
   - `statId`
   - `rarity`

### 阶段 2：扩展到完整装备

后续再扩：

- Helm
- Shoulders
- Chest
- Gloves
- Pants
- Boots
- Back
- Amulet
- Accessory1 / 2
- Ring1 / 2

### 阶段 3：与技能栏识别联动

装备/武器读取成功后，可以进一步：

- 验证当前 weapon set 是否与 live skillbar 一致
- 用武器组结果帮助判断技能栏应出现哪些技能
- 减少 `Skill CD Monitor` 对静态 profile 的依赖

---

## 为什么这条路线比继续猜 skillbar 更直接

当前 skillbar 路线的问题是：

- 我们能拿到 `pCharSkillbar`
- 但还没锁定每个 slot 的真实 `skill id` offset
- `F1/F2/F3` 的自动记录已经能触发，但还没有稳定抓到 changed snapshot

而装备读取路线的好处是：

- `kx-vision` 已经给出了相对明确的结构
- 目标字段语义更清晰
- “当前武器组”对技能栏识别有直接帮助
- 即使暂时还不能完全自动识别所有技能栏，也能先把武器信息读出来

---

## 当前项目缺的是什么

不是缺 MumbleLink，也不是缺投影。

当前真正缺的是：

1. `pLocalChCliCharacter -> ChCliInventory` 的对象链确认
2. `ChCliInventory` 结构封装
3. `EquipmentArray` 读取代码
4. `ItCliItem / ItemDef / Stat` 的最小读取封装
5. 在 UI 里做一个基础展示面板

---

## 推荐下一步

对当前项目，最推荐的下一步不是继续先纠结投影或 MumbleLink，而是：

1. 先把 `ChCliInventory` 路线接出来
2. 先读武器槽
3. 用武器槽结果反推技能栏
4. 再决定是否扩到完整装备面板

---

## 一句话总结

“当前角色带了什么装备/武器”这件事，**可以做，而且最合理的方式是直接走内存里的 `ChCliInventory -> EquipmentArray -> ItCliItem` 路线**。

`kx-vision` 已经证明了这条路线是可行的；  
我们当前项目已经具备前半段上下文解析能力，下一步只差把 inventory/equipment 这段结构接进来。
