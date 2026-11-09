# 暗区自动手电

已在 RE01 / UE 5.6 中加入独立 C++ 运行时模块。保留 `BP_ShooterCharacter`、它的 `BP_FirstPersonCharacter` 父类、现有 `SpotLight`、`BP_DarkArea` 和原有 Box；不需要重新接蓝图节点。

## 使用

1. 保存你在 UE 中尚未保存的修改，关闭并重新打开 `RE01.uproject`。首次加入原生模块后，已运行的编辑器需要重新载入工程。本机的 Development Editor 模块已经编译成功。
2. 点击 Play。系统自动识别现有 `BP_ShooterCharacter` 和 `BP_DarkArea`（含子类），给玩家添加一个临时的 `AutomaticDarkAreaFlashlight` 组件。无需修改 GameMode、玩家父类或手动添加组件。
3. 关卡里的每个 `BP_DarkArea` 都使用它现有的 Box。复制暗区实例、调整原有 Box 的位置/大小即可布置更多暗区；同一暗区 Actor 下有多个 Box 也支持。
4. 把现有 `/Game/Actors/BP_FlashLight` 球放到关卡里。玩家碰到它后，球保留 0.2 秒，然后销毁并解锁该玩家的手电。无需给球接蓝图事件。运行时仅将球现有碰撞对 Pawn 的响应改成 Overlap，以免挡住玩家；不保存或改写球资产。

Box 必须启用查询碰撞，例如 `Query Only`。本机测试已确认当前保存的 Box 默认配置满足条件。系统直接查询 Box 与玩家 Capsule 的真实几何重叠，不依赖 Begin/End Overlap 事件，也不更改其他物体的碰撞响应。

## 行为

- 初始没有手电：即使出生或进入暗区，也保持关灯。
- 接触 `BP_FlashLight` 后等待 0.2 秒，球消失时才获得手电。等待期间仍不能亮灯；重复接触不会重置计时，触碰后走开也会完成拾取。
- 拾取后进入任意 Box：开灯。如果拾取完成时已经在暗区内，立即开灯；在暗区外完成拾取则保持关闭。
- 同时位于多个 Box：保持开灯；离开其中一个不会关闭。
- 离开全部 Box：保持亮灯 0.15 秒，届时重新查询实际重叠，仍在外面才关闭。
- 等待期间重新进入任意 Box：取消待关灯状态。下次离开重新计时。
- 计时使用游戏时间，暂停也暂停倒计时。关灯发生在到期后的首个游戏更新帧。
- 真正的空隙若通过时间超过 0.15 秒，灯会熄灭，这是“仍在所有暗区外”的预期结果。
- 支持旋转/缩放的 Box、区域销毁以及运行时生成的玩家和暗区。
- 拾取状态属于当前玩家角色，不做存档或死亡继承；重新开始游戏/生成新角色时需要重新拾取。

## 额外开灯条件

`UDarkAreaFlashlightComponent` 的 `bAllowFlashlight` 默认 `true`，但它不能绕过拾取要求。蓝图中通过 `Get Component By Class`（`DarkAreaFlashlightComponent`）取得组件后，调用 `Set Flashlight Allowed`。关闭条件立即熄灭；恢复条件时，已拾取且人在暗区内才开灯。`Has Collected Flashlight` 为只读状态。

另有 `Can Turn On Flashlight` 的 BlueprintNativeEvent 扩展点，默认返回 true，适合以后加入电量或装备判断。条件组合为 `HasCollectedFlashlight() && bAllowFlashlight && CanTurnOnFlashlight()`。

如果以后要在玩家自己的 BeginPlay 中立即配置该组件，或者要覆盖上述事件，可以先在玩家蓝图中手动添加此原生组件或它的蓝图子类。自动注册会复用已有组件，不会重复添加，也不需要改玩家父类。默认自动添加路径对新生成玩家在下一帧接入。

## 原有蓝图逻辑

本次没有保存/改写任何 `.uasset` 或 `.umap`，包括用户原有的未提交资产。截图中以前的 `EnterDarkArea`、`ExitDarkArea` 和可见性节点也没有被删除。

新组件在 `TG_PostUpdateWork`、玩家移动和普通蓝图逻辑之后更新，仅控制原有 SpotLight 的 Visibility / HiddenInGame，不更改灯的强度、颜色、附着关系或其他组件。普通旧蓝图事件对同一盏灯的可见性写入会被最终自动状态覆盖。今后新增手电控制请统一走该组件的额外条件，避免另建灯光控制逻辑。

资产路径集中在 `DarkAreaFlashlightSubsystem.cpp` 的 `OnWorldBeginPlay` 中。如果以后移动或重命名玩家、暗区、拾取球三个蓝图，应同步更新路径。

## 验证

拾取更新已使用本机 UE 5.6.1 完成 `RE01Editor Win64 Development` 编译和链接，成功生成新版 `Binaries/Win64/UnrealEditor-RE01.dll`。此前编辑器占用 DLL 的问题已在保存并关闭 UE 后解决。本机 MSVC 14.51 虽非 UE 首选版本，但本次构建成功。

新版 UE 自动化报告 `Saved/Automation/DarkAreaFlashlight/index.json`：3 成功、0 失败、0 测试警告。报告生成时间为 2026-11-08 23:26（America/Denver）。下表所列拾取和暗区用例均已使用新版 DLL 执行通过。

| 测试 | 已验证 |
| --- | --- |
| GracePeriod | 进入、接缝、仍在 B、离开后 0.15 秒确认 |
| ReentryAndPermission | 等待期间和到期帧重新进入、重新计时、默认/禁用条件、零延迟 |
| ExistingBlueprintIntegration | 加载真实的玩家、暗区和拾取球蓝图；未拾取不能开灯；0.2 秒后球销毁并解锁；重复触碰不重置计时；暗区内/外拾取；触碰后走开；A→接缝→B→离开；实际 SpotLight 可见性；旋转/缩放 Box；区域销毁；玩家重新生成；不接管无关角色；保持父类且不新增灯 |

集成测试没有执行玩家蓝图的完整 BeginPlay 武器/UI 流程，也没有代替主关卡的可视化 PIE 验收。测试期间未保存任何资产。

手动验收：保存并重开工程，Play。先不碰球直接进入暗区，灯应关闭；再碰球，约 0.2 秒后球消失，只有这之后才允许在暗区开灯。然后按 **A → 接缝 → B → 离开全部** 行走：前三段持续亮，最后保持约 0.15 秒后熄灭，等待期间退回 B 应持续亮。在 PIE 中选中玩家的 `AutomaticDarkAreaFlashlight` 组件，可查看 `Has Collected Flashlight`、`Active Box Count`、`Is Flashlight On`、`Pending Turn Off`，也可启用 `Log Transitions` 查看 Output Log。

## 文件

- `Source/RE01/Flashlight/DarkAreaFlashlightSubsystem.*`：识别现有蓝图、注册暗区、自动接入玩家、胶囊与 Box 几何查询。
- `Source/RE01/Flashlight/DarkAreaFlashlightComponent.*`：复用已有 SpotLight、额外条件、状态刷新与调试字段。
- `Source/RE01/Flashlight/DarkAreaLightState.h`：0.15 秒关闭确认和重新进入取消。
- `Source/RE01/Tests/DarkAreaFlashlightTests.cpp`：UE 自动化测试。
- `Source/RE01*.Target.cs`、`Source/RE01/RE01.Build.cs`、`RE01.uproject`：原生模块构建和加载配置。

在 Session Frontend → Automation 中搜索 `RE01.DarkAreaFlashlight` 可重跑测试。
