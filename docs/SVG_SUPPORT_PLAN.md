# VMP-ui SVG 支持评估与实现规划

> 文档版本：v1.0  
> 日期：2026-05-25  
> 状态：待评审

---

## 一、结论摘要

当前仓库**没有 SVG 解码能力**，但**已有完整的位图上传与渲染链路**，因此 SVG 支持的根改动点不在渲染器，而在 `ImageManager` 的“文件解码 -> RGBA 像素缓冲”阶段。

当前实现现状：

- `ImageRenderer` 只负责懒加载调用 `ImageManager::loadTexture()`，拿到 `SDL_GPUTexture*` 后按普通贴图批处理绘制。
- `ImageManager` 仅按扩展名区分 `bmp -> SDL_LoadBMP`，其余格式统一走 `stb_image`。
- `stb_image` 不支持 SVG，因此 `.svg` 路径必然走失败分支。
- 示例工程已经把 SVG 明确标记为“不支持”，并给出原因说明。

因此，SVG 支持的推荐实现方向是：

1. 在 `ImageManager` 内增加 SVG 专用解码路径。
2. 将 SVG 先栅格化为 RGBA 位图，再复用现有 `uploadToGpu()`。
3. 首阶段先支持静态 SVG 文件，不改动 `ImageRenderer`、`BatchManager`、shader 或布局系统。

这条路径改动集中、风险可控，且能最大化复用现有代码。

---

## 二、现状评估

### 2.1 当前控制路径

现有图像渲染链路如下：

`CreateImageFromPath()` / `ImagePath()`  
-> 写入 `components::ImageSource.path`  
-> `ImageRenderer` 首帧懒加载调用 `ImageManager::loadTexture(path)`  
-> `ImageManager` 解码文件并上传 `SDL_GPUTexture`  
-> `BatchManager` 按普通纹理矩形绘制

说明：

- SVG 只要能在 `ImageManager` 中产出 RGBA 像素数据，就能无缝进入现有渲染管线。
- 当前 `Image` 组件只保存 `textureId`、UV、颜色，不感知资源类型；这对首阶段是好事，不需要额外改组件协议。

### 2.2 已确认的代码事实

- `src/managers/ImageManager.hpp` 文档和接口只声明支持 bmp/png/jpeg。
- `src/managers/ImageManager.cpp` 中 `loadTexture()` 通过扩展名分流：`bmp` 走 `loadWithSdlBmp()`，其他走 `loadWithStb()`。
- `src/renderers/ImageRenderer.hpp` 中懒加载逻辑只依赖 `ImageManager::loadTexture()` 的成功与否，不关心文件格式。
- `src/common/components/Data.hpp` 中 `ImageSource.path` 注释仍写 `bmp/png/jpeg`。
- `example/ui_demo/View/Mainwindow.h` 明确展示 “ICO / SVG: not supported by current ImageManager decoder”。
- `example/ui_demo/assets/sample.svg` 已提供一份真实样例，包含：
  - 线性渐变
  - 基础几何图形
  - 文本节点 `<text>`

### 2.3 能力缺口

当前缺的不是 GPU 或 UI 组件能力，而是以下能力：

- SVG XML 解析
- viewBox / width / height 计算
- 路径、渐变、变换的栅格化
- 文字节点支持策略
- 失败诊断与缓存策略

### 2.4 影响范围判断

首阶段接入 SVG 时，不需要修改的部分：

- `ImageRenderer`
- `BatchManager`
- `CommandBuffer`
- shader 资源
- Yoga 布局系统
- `Image` / `ImageSource` 对外 DSL 入口

首阶段必须修改的部分：

- `CMakeLists.txt`
- `src/CMakeLists.txt`
- `src/managers/ImageManager.hpp`
- `src/managers/ImageManager.cpp`
- `src/common/components/Data.hpp`
- `example/ui_demo/View/Mainwindow.h`
- `tests/unittest/` 下新增或扩充图像加载测试

---

## 三、需求边界建议

在编码前，先明确本项目的 SVG 需求边界，否则容易把“支持 SVG”做成无限范围。

建议把需求拆成两个层级。

### 3.1 P0 目标：静态 UI 资源 SVG

首批只覆盖 UI 贴图场景常见能力：

- 本地文件 `.svg`
- 静态文档，不支持脚本和动画
- `viewBox`
- 基础形状：`rect`、`circle`、`ellipse`、`line`、`polyline`、`polygon`、`path`
- `fill` / `stroke`
- 基本变换 `transform`
- 线性渐变 / 径向渐变
- 基础透明度
- 栅格输出为 RGBA8

### 3.2 非目标：首阶段不做

- SVG 动画
- JavaScript / 外链资源
- 滤镜特效完整支持
- 可交互 DOM
- 运行时编辑 SVG 树
- 保持矢量态直到 GPU 绘制
- 字体回退系统级精确一致性

### 3.3 特别提醒：文本节点是分水岭

仓库自带 `sample.svg` 含 `<text>`。这意味着“简单图标 SVG”并不是唯一需求。

因此建议把文本支持作为选型门槛：

- 如果首阶段要求示例样本原样显示，则库选型必须验证 `<text>` 渲染质量。
- 如果首阶段允许限制，则应在文档和示例里明确：仅保证图标型 SVG，文本型 SVG 暂不承诺。

不建议在需求未定前直接声称“支持 SVG”，否则后续会被文本/滤镜问题反噬。

---

## 四、方案选型

### 4.1 方案 A：在 `ImageManager` 接入 SVG 栅格化库

实现方式：

- 检测 `.svg`
- 调用第三方 SVG 库解析并渲染到内存 RGBA 位图
- 复用现有 `uploadToGpu()` 上传

优点：

- 改动最小
- 完全复用现有纹理绘制路径
- 对外 API 无感知
- 与现有 `ImageSource` / `Image` 模型兼容

缺点：

- SVG 失去矢量缩放优势，最终仍是位图缓存
- 不同显示尺寸可能需要不同栅格缓存策略
- 文本/滤镜能力取决于选用的 SVG 库

结论：**推荐作为首阶段实现方案。**

### 4.2 方案 B：自研 SVG -> Canvas/Path 渲染

实现方式：

- 自己解析 SVG
- 将图元转换到现有 UI 图元或新增矢量命令
- 运行时直接绘制

问题：

- 当前项目没有完整通用矢量栈
- `Canvas` API 仍偏基础，无法低成本覆盖 SVG 语义
- 路径、渐变、文本、描边、裁剪、变换都要自己补

结论：**不适合作为当前阶段方案。**

### 4.3 候选第三方库比较

#### 候选 1：NanoSVG

特点：

- 接入轻量
- 适合简单图标路径

问题：

- 能力覆盖偏弱
- 对复杂 SVG、文本、渐变、兼容性不够稳妥

判断：**只适合“极简图标”场景，不适合作为本项目默认 SVG 能力。**

#### 候选 2：lunasvg

特点：

- C++ 集成方式更接近当前项目
- 目标就是把 SVG 渲染到位图缓冲
- 与 `ImageManager` 的“解码后上传纹理”模式匹配度高

风险：

- 仍需用样本验证文本、渐变、变换、透明度表现
- 引入方式需确认是源码 vendor、单独静态库还是纯头集成

判断：**推荐作为首选 Spike 方案。**

#### 候选 3：resvg（或其 C/C++ 封装）

特点：

- 通常兼容性更强

问题：

- 集成成本更高
- 对当前纯 CMake/C++ 静态库项目不够轻量

判断：**作为兼容性兜底方案，不建议一开始就上。**

### 4.4 最终建议

采用“两段式”策略：

1. 先用 `lunasvg` 做 Spike，验证仓库自带 `sample.svg` 和一组回归样本。
2. 若文本或渐变支持不达标，再升级到更强兼容方案，而不是一开始就引入高复杂度依赖。

---

## 五、推荐实现设计

### 5.1 设计原则

- 不改公开 DSL：继续使用 `CreateImageFromPath()` 和 `ImagePath()`。
- 不改渲染器协议：`ImageRenderer` 仍只消费 GPU 纹理。
- 不把 SVG 特判扩散到全局：格式判断集中在 `ImageManager`。
- 不在首阶段引入异步复杂度：先保持当前同步懒加载行为。

### 5.2 `ImageManager` 扩展建议

建议把当前实现从“按格式直接上传”重构为“两层流程”：

1. 文件解码层
2. GPU 上传层

建议新增内部接口：

```cpp
struct DecodedImage
{
    std::vector<unsigned char> pixels;
    uint32_t width = 0;
    uint32_t height = 0;
};

[[nodiscard]] std::optional<DecodedImage> decodeRasterFile(const std::string& path);
[[nodiscard]] std::optional<DecodedImage> decodeSvgFile(const std::string& path);
[[nodiscard]] SDL_GPUTexture* uploadDecodedImage(SDL_GPUDevice* device, const DecodedImage& image);
```

好处：

- 让 `stb`、BMP、SVG 三类路径在“解码层”统一汇合
- 更容易为后续异步解码做准备
- 单元测试可以只测解码结果，不必每次拉起 GPU

### 5.3 格式分流建议

`loadTexture()` 中建议显式分流：

- `bmp` -> SDL 路径
- `png/jpg/jpeg/...` -> stb 路径
- `svg` -> SVG 路径
- 未知扩展 -> 先尝试 raster，再按需决定是否尝试 SVG

不建议继续保留“除 bmp 外一律 stb”的策略，否则格式支持会越来越不可维护。

### 5.4 SVG 栅格尺寸策略

这是实现时最容易埋坑的点。

首阶段建议采用以下规则：

1. 优先使用 SVG 文档自身 `width/height`
2. 如果只有 `viewBox`，按 `viewBox` 尺寸栅格化
3. 如果两者都缺失，则返回失败

先不要在首阶段做“根据控件当前布局尺寸动态多版本栅格化”，原因：

- 当前 `ImageRenderer` 加载发生在拿到 `path` 时，而不是拿到最终显示尺寸后
- 若马上引入尺寸感知缓存，`ImageSource` 和缓存键都要一起改
- 容易把一次文档规划膨胀成资源系统重构

### 5.5 缓存策略建议

当前缓存键是 `path -> SDL_GPUTexture*`。

对首阶段可以继续沿用，但要明确限制：

- 同一路径 SVG 只会以一种分辨率栅格化后缓存
- 当同一 SVG 在 16x16 和 256x256 两种尺寸下被重复使用时，清晰度可能不理想

因此建议：

- 第一阶段：保持 `path` 级缓存，尽快打通功能
- 第二阶段：演进为 `path + rasterWidth + rasterHeight (+ dpiScale)` 级缓存

### 5.6 失败处理与日志

SVG 路径需要补齐更具体的错误日志：

- 文件不存在
- XML 解析失败
- 无有效尺寸
- 栅格化失败
- GPU 上传失败

建议日志前缀统一为：

```text
[ImageManager][SVG] ...
```

这样后续排障不会和普通位图加载混在一起。

---

## 六、分阶段实施计划

### Phase 0: 技术 Spike

目标：确认库选型，不做 API 扩散。

任务：

- 引入 `lunasvg` 到 `third_party/` 或以最小 vendor 方式接入
- 在独立实验代码中验证以下样例：
  - `example/ui_demo/assets/sample.svg`
  - 一个仅 path 的图标样例
  - 一个带透明度和渐变样例
  - 一个非法 SVG 样例
- 输出结论：
  - 文本是否可接受
  - 渐变是否正确
  - 渲染耗时是否可接受
  - 依赖接入方式是否适合当前 CMake 结构

验收：

- 能明确回答“是否采用 lunasvg 进入正式实现”。

### Phase 1: 打通静态 SVG 贴图加载

目标：最小功能可用。

任务：

- 在 `ImageManager` 中新增 `.svg` 分支
- 将 SVG 渲染为 RGBA8 像素
- 复用现有 `uploadToGpu()`
- 更新 `ImageSource.path` 注释与 `ImageManager.hpp` 文档
- 调整 example 文案，改为展示 SVG 卡片是否可加载

验收：

- `CreateImageFromPath("...sample.svg")` 能成功显示
- png/jpeg/bmp 行为无回归
- 非法 SVG 能稳定失败并打印日志，不崩溃

### Phase 2: 测试与回归样本

目标：让 SVG 支持具备可维护性。

任务：

- 新增图像加载单元测试或最小集成测试
- 增加测试资源：
  - simple-shape.svg
  - gradient.svg
  - text.svg
  - invalid.svg
- 覆盖缓存命中、失败重试行为、重复加载行为

验收：

- 测试可稳定证明：
  - SVG 能加载
  - 非 SVG 路径不受影响
  - 失败路径不会把缓存污染成“假成功”

### Phase 3: 尺寸感知缓存优化

目标：解决“同一路径多尺寸显示”的清晰度问题。

任务：

- 扩展缓存键为尺寸敏感键
- 评估是否需要在 `ImageSource` 中记录期望栅格尺寸
- 评估是否把 DPI 因子纳入缓存键

说明：

- 这一阶段是质量优化，不是首发阻塞项。

---

## 七、任务拆分到文件

### 7.1 构建层

文件：

- `CMakeLists.txt`
- `src/CMakeLists.txt`

改动：

- 增加 SVG 库源码目录或静态库目标
- 将其链接到 `ui`
- 如有必要增加 include path

### 7.2 解码层

文件：

- `src/managers/ImageManager.hpp`
- `src/managers/ImageManager.cpp`

改动：

- 新增 SVG 解码函数
- 重构为统一解码结果结构
- 增强日志与扩展名分发逻辑

### 7.3 数据注释与约束

文件：

- `src/common/components/Data.hpp`

改动：

- 将 `ImageSource.path` 注释从 `bmp/png/jpeg` 更新为包含 `svg`

### 7.4 示例与回归展示

文件：

- `example/ui_demo/View/Mainwindow.h`

改动：

- 把 “SVG not supported” 文案改成真实展示区
- 追加 `sample.svg` 卡片
- 保留对 ICO 的限制说明，除非同时实现 ICO

### 7.5 测试层

文件：

- `tests/unittest/CMakeLists.txt`
- `tests/unittest/...` 新增测试文件

改动：

- 增加最小 SVG 加载回归
- 视当前测试设施决定是否做纯解码测试或 GPU 集成测试

---

## 八、风险与规避

### 风险 1：文本节点渲染效果不稳定

原因：

- `sample.svg` 含 `<text>`，而不同 SVG 库对文本能力差异很大。

规避：

- 把文本样例纳入 Phase 0 Spike 的强制验收项。

### 风险 2：SVG 缓存分辨率固定导致模糊

原因：

- 当前缓存键只有 `path`。

规避：

- Phase 1 先接受限制
- Phase 3 再升级为尺寸敏感缓存

### 风险 3：同步栅格化造成首帧卡顿

原因：

- 当前懒加载发生在渲染收集阶段，同步执行。

规避：

- 首阶段只做小型 UI 资源 SVG
- 后续若卡顿明显，再接入现有线程池把解码下沉到 CPU 任务

### 风险 4：第三方库引入后编译链复杂化

原因：

- 当前项目第三方依赖全部通过源码子目录管理。

规避：

- 优先选择 CMake/源码集成简单的库
- 不在首阶段引入额外语言工具链

---

## 九、验收标准

实现完成后，建议以以下标准验收：

### 功能验收

- `CreateImageFromPath()` 能加载 `.svg`
- `ImagePath()` 设置为 `.svg` 路径时可正常显示
- 仓库自带 `sample.svg` 可见且无明显错误
- 非法 SVG 返回失败但程序不崩溃

### 回归验收

- bmp/png/jpeg 加载结果与当前一致
- 图像缓存命中逻辑仍有效
- `ImageRenderer` 不需要为了 SVG 添加额外特殊分支

### 工程验收

- Debug 构建通过
- 开启测试时新增测试通过
- 示例工程文案与实际能力一致

---

## 十、建议的执行顺序

建议按以下顺序推进，而不是一次性大改：

1. 做 `lunasvg` Spike，验证 `sample.svg`
2. 在 `ImageManager` 中最小接入 `.svg` 分支
3. 更新 example 展示和文案
4. 补测试样本和失败路径
5. 最后再决定是否做尺寸感知缓存

这样可以把“能力验证”和“工程接入”分开，避免在库选型未稳定前就重构资源缓存。

---

## 十一、最终建议

对当前仓库来说，SVG 支持不是渲染架构问题，而是**资源解码层缺一个 SVG 栅格化后端**。

推荐结论：

- **短期目标**：以 `ImageManager + SVG 栅格化库` 方式实现静态 SVG 贴图支持。
- **推荐路径**：先用 `lunasvg` 完成 Spike，再决定是否正式接入。
- **首阶段边界**：只做静态本地 SVG，不扩展到动画、脚本、完整滤镜生态。
- **后续优化**：如功能稳定，再做尺寸感知缓存和异步解码。

该方案能最大化复用现有代码，同时把变更控制在单一模块内，符合当前项目的工程结构与演进节奏。