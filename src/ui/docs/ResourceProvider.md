# UI Resource Provider

## 目标

为 ui 模块提供一层稳定的二进制资源访问接口，把上层资源消费者和具体嵌入方案解耦。

当前默认后端是 `cmrc`。
未来如果工具链成熟，可以替换为 `std::embed` 后端，而不修改 `PipelineCache` 等使用方。

## 当前问题

在引入抽象层之前，`PipelineCache` 直接依赖 `cmrc`：

- 公开头文件包含 `cmrc/cmrc.hpp`
- 资源访问逻辑和资源使用逻辑耦合在一起
- 将来切换到 `std::embed` 时，需要直接修改 shader 加载方

这会导致：

- 嵌入方案泄漏到公开 API
- 后端替换成本高
- 无法平滑支持 `filesystem / cmrc / std::embed` 多实现并存

## 抽象结构

### 1. `BinaryResource`

表示一次成功加载后的二进制资源视图：

- `bytes`: 只读字节视图
- `owner`: 生命周期持有者

设计重点：

- 调用方只关心 `data()` / `size()`
- 后端决定资源到底来自静态区、cmrc 虚拟文件系统，还是运行时文件读取
- `owner` 允许零拷贝后端维持底层对象生命周期

### 2. `IResourceProvider`

统一资源访问接口：

- `exists(path)`
- `loadBinary(path)`

设计原则：

- 只暴露 ui 当前真正需要的二进制读取能力
- 不提前引入目录遍历、文本解码、写回等暂时无用的 API
- 保持接口足够小，方便未来增加 `StdEmbedResourceProvider`

### 3. `GetDefaultUiResourceProvider()`

返回 ui 模块默认资源提供器。

当前行为：

- 返回 `cmrc` 实现

未来行为：

- 可以改成 `std::embed` 实现
- 或根据编译宏切换 `cmrc / std::embed / filesystem`

当前仓库已经预留了 `STD_EMBED` 选择骨架：

- `UI_RESOURCE_BACKEND=CMRC`：默认，实际可用
- `UI_RESOURCE_BACKEND=STD_EMBED`：占位实现，仅验证切换链路，不提供真实资源表

## 当前接入点

目前这层抽象先服务于 shader 资源加载：

- `PipelineCache` 只依赖 `IResourceProvider`
- `ResourceProvider.cpp` 提供当前 `cmrc` 后端

这意味着：

- `PipelineCache` 不再直接包含 `cmrc` 头文件
- `RenderSystem` 也不需要知道 `cmrc` 是否存在

## 为什么这个设计适合未来切到 `std::embed`

`std::embed` 和 `cmrc` 的主要差异不在“能否拿到字节”，而在“资源如何声明、如何组织、如何按路径定位”。

这层设计把差异压缩在 provider 实现内部：

- `cmrc` 后端：根据路径字符串从虚拟文件系统取资源
- `std::embed` 后端：根据路径字符串查静态资源表，再返回嵌入对象的字节视图

对调用方来说，二者都只是：

- 给一个逻辑路径
- 拿到一段只读二进制数据

## 将来新增 `std::embed` 后端的建议方式

建议新增一个实现，例如：

- `StdEmbedResourceProvider.cpp`

当前仓库已经进一步补了一个正式入口骨架：

- `StdEmbedResourceTable.hpp`
- `StdEmbedResourceTable.cpp`

这两个文件负责定义 `逻辑路径 -> 静态字节视图` 的统一资源表。
当前实现仍然是空表，但 provider 已经开始通过这张表工作。

核心工作：

1. 维护逻辑路径到嵌入对象的映射表
2. 为每个资源返回稳定的只读字节视图
3. 保证对象生命周期至少覆盖 shader 创建调用期

建议把未来真实的 `std::embed` 对象只放进资源表实现中，不要散落到 `PipelineCache`、`RenderSystem` 等调用方。

推荐切换方式：

1. 保持 `IResourceProvider` 不变
2. 增加 `std::embed` 实现
3. 让 `GetDefaultUiResourceProvider()` 根据编译选项返回不同后端

例如：

- `UI_RESOURCE_BACKEND_CMRC`
- `UI_RESOURCE_BACKEND_STD_EMBED`
- `UI_RESOURCE_BACKEND_FILESYSTEM`

当前骨架里，`STD_EMBED` 还只是占位 provider：

- `exists()` 通过 `StdEmbedResourceTable` 查表
- `loadBinary()` 通过 `StdEmbedResourceTable` 取资源
- 现在四个 shader 资源已经通过构建期生成表接入 `StdEmbedResourceTable`
- 当前 `STD_EMBED` 的“最小可用范围”仅覆盖 shader 加载路径

这样做的目的是先把配置链路、编译宏和调用边界固定下来，再逐步把更多资源补进同一张表。

## 范围边界

当前抽象层只接管“嵌入型二进制资源”的读取，不强行覆盖所有 UI 资源路径。

具体来说：

- shader：适合纳入抽象层
- 默认字体 / 图标字体：当前仍然走文件系统加载

这是刻意的分阶段策略，因为字体资源目前没有和 cmrc 统一嵌入。

## 后续扩展建议

1. 如果后续准备把默认字体和图标字体也嵌入，可继续复用同一接口，不需要重做抽象。
2. 如果需要支持调试期从磁盘热加载，可增加 `filesystem` 后端并在 Debug 下切换。
3. 如果将来 `std::embed` 能稳定覆盖当前编译器矩阵，可以只替换默认 provider 实现，不改上层调用代码。
4. 真正实现 `std::embed` 时，优先补“路径 -> 嵌入对象”的静态映射表，而不是改动 `PipelineCache` 这类调用方。
