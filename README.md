# Puerts Blueprint TypeScript Generator

独立的 Unreal Editor 插件，用于从 Content Browser 中选择的 Blueprint 生成
Puerts `blueprint.mixin` TypeScript 模板。

## 使用方式

1. 启用 `Puerts Blueprint TypeScript Generator` 插件并重启编辑器。
2. 先使用 Puerts 的声明生成器生成 `Typing/ue/ue_bp.d.ts`。
3. 在 Content Browser 中选择一个或多个 Blueprint。
4. 右键选择“生成 Puerts TypeScript 文件”。
5. 也可以打开任意 Blueprint，点击编辑器顶部带 Puerts 图标的“生成 TS”按钮；
   它会直接为当前 Blueprint 执行同一套生成流程。

默认路径映射：

```text
/Game/GZNY/Core/Subsystem/BP_PZTActor
-> <Project>/TypeScript/GZNY/Core/Subsystem/BP_PZTActor.ts
```

输出根目录和蓝图虚拟根路径可在以下位置修改：

```text
编辑 -> 项目设置 -> 插件 -> Puerts Blueprint TypeScript Generator
```

已有文件默认不会直接覆盖。批量生成中发现已有文件时，插件会先询问：

- 是：更新蓝图加载模板，保留已有 import、接口扩展、类方法和其他手写逻辑。
- 否：只创建缺失文件。
- 取消：停止本次生成。

如确实需要恢复旧版的完整覆盖行为，可在插件设置中关闭
`Preserve Existing Implementation When Regenerating`。

默认还会把每个生成模板的 Blueprint GeneratedClass 加入项目当前 GameInstance
蓝图 Class Defaults 中的 `Puerts Mixin Blueprint Classes` 数组。该数组是硬类引用，
可以让 Cooker 发现只在 TypeScript 中使用的蓝图。已有脚本选择“跳过”时也会补充引用。
如不需要，可在插件设置中关闭
`Bind Generated Blueprints to GameInstance`。

## 实现思路

### 1. 保持为独立 Editor 插件

插件只包含 `Editor` 类型模块，不参与游戏运行和打包。它不修改 Puerts 源码，
也不调用 Puerts 的私有 C++ 接口，仅生成符合 `blueprint.mixin` 约定的文本文件。

模块使用 `PostEngineInit` 加载阶段。Blueprint Editor 所属的 Kismet 模块在启动时会向
`GEditor` 注册 Undo Client；如果生成器在默认启动阶段过早强制加载 Kismet，
`GEditor` 尚未构造完成，会在 `UEditorEngine::RegisterForUndo` 中产生访问冲突。
等待 Engine 初始化完成后再注册工具栏可以避免这个启动顺序问题。

模块启动时通过 `ContentBrowserModule` 注册资产右键菜单扩展；模块关闭时使用
保存的 `FDelegateHandle` 移除扩展，避免编辑器热重载后出现重复菜单。

同时通过 `FBlueprintEditorModule` 的资产编辑器扩展委托向 Blueprint Editor 工具栏
增加按钮。扩展回调携带当前编辑器正在编辑的对象，插件从中取得 `UBlueprint`，所以
多个蓝图编辑器同时打开时，每个按钮只处理自己对应的 Blueprint。

工具栏和右键菜单共用 `GenerateBlueprints` 入口，路径计算、覆盖确认、文件写入和结果
通知完全一致。按钮图标直接复用 Puerts 插件的
`Resources/ButtonIcon_40x.png`，不会把截图中的深色工具栏背景写进图标资源。

### 2. 只对 Blueprint 显示生成菜单

内容浏览器打开资产菜单时，插件检查所选资产能否转换为 `UBlueprint`。只要选中
内容中存在 Blueprint，就增加“生成 Puerts TypeScript 文件”菜单项。执行菜单后
再次过滤选择内容，因此支持同时选中多个 Blueprint，也不会误处理材质、纹理等
其他资产。

### 3. 将 Unreal 包路径映射为磁盘目录

生成器通过 Blueprint 的包名获取虚拟路径：

```text
/Game/GZNY/Core/Subsystem/BP_PZTActor
```

先移除设置中的 `BlueprintRootPath`，默认是 `/Game`，然后把剩余的包目录追加到
工程根目录下的 `OutputDirectory`，默认是 `TypeScript`：

```text
虚拟根路径：/Game
包目录：    /Game/GZNY/Core/Subsystem
输出目录：  <Project>/TypeScript/GZNY/Core/Subsystem
输出文件：  BP_PZTActor.ts
```

写入前使用 `IFileManager::MakeDirectory(..., true)` 递归创建目录。输出根目录必须是
工程内的相对路径；绝对路径或包含 `..` 的配置会自动退回 `TypeScript`，避免文件
被意外写到工程外部。

### 4. 从 GeneratedClass 构造 Puerts 类型信息

插件不猜测 `_C` 类名，而是从 `Blueprint->GeneratedClass` 读取真实类型和对象路径：

```text
GeneratedClass：BP_PZTActor_C
对象路径：      /Game/GZNY/Core/Subsystem/BP_PZTActor.BP_PZTActor_C
Puerts 类型：   UE.Game.GZNY.Core.Subsystem.BP_PZTActor.BP_PZTActor_C
```

包路径的每一级都会转换为合法的 TypeScript 标识符。空格、连字符等非法字符替换为
下划线；以数字开头的名称会自动添加下划线。

### 5. 生成 Mixin 模板

最终模板由 Unreal C++ 直接拼接并使用 UTF-8 无 BOM 写入：

```ts
import * as UE from "ue";
import { blueprint, toDelegate, $Nullable, $ref, $unref } from "puerts";

declare module "ue" {
  namespace Game.GZNY.Core.Subsystem.BP_PZTActor {
    interface BP_PZTActor_C {}
  }
}

const UClass = UE.Class.Load(
  "/Game/GZNY/Core/Subsystem/BP_PZTActor.BP_PZTActor_C",
);

const JSClass = blueprint.tojs(UClass);

interface TS_BP_PZTActor
  extends UE.Game.GZNY.Core.Subsystem.BP_PZTActor.BP_PZTActor_C {}

class TS_BP_PZTActor implements TS_BP_PZTActor {
  ReceiveBeginPlay(): void {}
}

blueprint.mixin(JSClass, TS_BP_PZTActor);
```

模板使用 Blueprint GeneratedClass 的完整对象路径调用 `UE.Class.Load`，再把返回的
`UClass` 传给 `blueprint.tojs`，避免依赖 `blueprint.load` 对懒加载命名空间占位对象的
替换行为。

生成前要求 Blueprint 已具有有效的 `GeneratedClass`。如果蓝图尚未成功编译，插件
会跳过该蓝图并在 Output Log 中说明原因。

生命周期空函数根据蓝图反射结果生成：存在 `ReceiveBeginPlay` 时生成 Actor 生命周期
入口；不存在该函数但存在 `Construct` 时生成 Widget 生命周期入口。可以在插件设置中
关闭生命周期空函数生成。

### 6. 保护已有 TypeScript 代码

生成器先统计目标路径中已经存在的 `.ts` 文件。存在冲突时统一询问一次：

- 更新模板：把 `BPClass + blueprint.load + StaticClass()` 及 1.2.1 的重新获取类模板
  迁移回 `UE.Class.Load + blueprint.tojs`，并把生成器创建的 `jsClass` 标识符更新为
  `JSClass`；文件中的业务 import、接口扩展、类方法和辅助函数保持原样。
- 跳过：保留已有脚本，只补充缺失文件。
- 取消：不写入任何文件。

如果已有文件中的加载块已经被手动改成生成器无法识别的形式，迁移会失败并保持原文件
不变。可以在插件设置中关闭保留选项，显式选择完整覆盖。执行完成后通过编辑器
Notification 显示生成、更新模板、跳过和失败数量，详细错误同时写入 Output Log。

### 7. 生成流程

```text
选择 Blueprint
    -> Content Browser 右键菜单 / Blueprint Editor 工具栏按钮
    -> 读取 UBlueprint 和 GeneratedClass
    -> 计算 TypeScript 输出路径
    -> 检查已有文件并确认覆盖策略
    -> 创建镜像目录
    -> 生成 blueprint.mixin 模板
    -> 将 GeneratedClass 硬引用写入并保存当前 GameInstance 蓝图
    -> UTF-8 写入并显示结果通知
```

### 8. GameInstance Cook 硬引用

项目当前 GameInstance 由 `DefaultEngine.ini` 的 `GameInstanceClass` 指定。生成器加载
这个类，在其 Class Defaults 的 `PuertsMixinBlueprintClasses` 数组中去重追加本次处理的
Blueprint GeneratedClass，然后自动保存 GameInstance 蓝图。由于 GameInstance 自身是
游戏启动根资产，Cooker 会沿硬类引用把这些 Mixin 蓝图及其依赖一并 Cook。

该属性定义在 `UTwinScriptGameInstance` 中。若项目未配置 GameInstance 蓝图、当前类不继承
这个基类、DTSCore 尚未重新编译，或者资产保存失败，生成器不会静默忽略，而会在
Notification 和 Output Log 中报告绑定错误。已在内存中写入但自动保存失败时，可以手动
保存 GameInstance 蓝图。

### 9. 主要源码

```text
PuertsBlueprintTsGenerator.uplugin
    插件描述、Editor 模块和 Puerts 插件依赖

Source/PuertsBlueprintTsGenerator/PuertsBlueprintTsGenerator.Build.cs
    ContentBrowser、AssetRegistry、DeveloperSettings 等模块依赖

Source/PuertsBlueprintTsGenerator/Public/
    PuertsBlueprintTsGeneratorSettings.h
    输出目录、蓝图根路径和模板选项

Source/PuertsBlueprintTsGenerator/Private/
    PuertsBlueprintTsGeneratorModule.cpp
    菜单注册、路径映射、模板生成和文件写入
```

## UE 版本兼容

插件不依赖 Puerts 的私有 C++ API，仅依赖 UE Editor 的 Content Browser、Kismet、
AssetRegistry 和文件系统 API。`.uplugin` 未绑定固定 EngineVersion，复制到其他
UE 5.x C++ 工程后，使用该工程对应引擎重新编译即可。

Puerts 插件本身仍需使用与目标 Unreal Engine 版本匹配的发行版。

跨版本维护时优先保留以下原则：

- 不依赖 Puerts Editor 模块内部类。
- 不直接判断 UE 版本对应的 Blueprint 资产类路径字段，而是加载资产并转换为
  `UBlueprint`。
- 使用 UE 5.x 中长期稳定的 Content Browser Extender API。
- 使用 Blueprint Editor 的 `FAssetEditorExtender` 取得当前编辑资产，避免依赖私有
  Blueprint Editor 实现。
- 不在描述文件中写死 `EngineVersion`。
- 每个目标 UE 版本使用自己的引擎工具链重新生成项目文件并编译插件。
