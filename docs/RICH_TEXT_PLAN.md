# PmkUi 富文本处理架构规划

> **日期**：2026-05-27  
> **输入来源**：需求文档、`src/` 源码阅读（Data.hpp / Policies.hpp / FontManager.hpp / TextTextureCache.hpp / TextRenderer.hpp / TextRenderHelper.hpp / TextUtils.hpp / FontAtlasManager.hpp）  
> **作用范围**：`src/` 文本子系统扩展，不修改现有公共 API 签名  
> **状态**：修订版 v2

---

## 目录

1. [范围定义](#1-范围定义)
2. [现状盘点](#2-现状盘点)
3. [架构设计](#3-架构设计)
4. [改动类型分析](#4-改动类型分析)
5. [工作包拆分](#5-工作包拆分)
6. [风险清单](#6-风险清单)
7. [待确认问题](#7-待确认问题)

---

## 1. 范围定义

### 1.1 本次支持（In-Scope）

| 能力 | 说明 |
|------|------|
| 多色/多字号内联样式 | 同一 `Text` 组件内混合颜色、字号 |
| 粗体 / 斜体 | 通过字体变体 Face 实现（不做软件伪粗/伪斜） |
| 下划线 | 渲染层绘制横线段，激活 `TextFlag::UNDERLINE` |
| 超链接标记 | 仅标记区间 + 触发 `OnClick`，无导航跳转逻辑 |
| 内联图片占位 | 将图片作为 inline-block 字形占位，宽高固定 |
| ANSI 转义码子集 | 颜色（`\e[3Xm`、`\e[9Xm`、`\e[38;2;r;g;bm`）、重置（`\e[0m`）；`TextFlag::ANSI` 激活 |
| 字体家族切换 | Bold/Italic/BoldItalic 变体，同字号注册多 Face |
| Grapheme 簇迭代 | 用 HarfBuzz cluster 信息替代字节索引，满足 CJK + Emoji 正确光标 |

### 1.2 明确不支持（Out-of-Scope）

- HTML 全集（`<table>`、`<script>`、CSS 继承、盒模型）
- Markdown 块级语法（标题 `#`、代码块 ` ``` `、水平线）
- 双向文字（BiDi / RTL），依赖 HarfBuzz 的 BiDi 支持另立专题
- 可编辑富文本（`TextEdit` 组件的富文本编辑）
- 嵌入视频 / 音频
- 自定义字体加载 API（字体路径在 `FontManager` 初始化时决定）
- 外部 CSS 样式表

### 1.3 标记语言选型

**已决策（Q1 已确认）：选用 HTML 子集解析器**。

**支持标签**：`<b>`、`<i>`、`<u>`、`<s>`、`<span>`、`<a href="...">`、`<img src="..." width="..." height="...">`、`<br>`

**支持属性**：

| 属性 | 说明 |
|------|------|
| `style` | 内联 CSS 子集（颜色、字号、字重、字形、文字装饰），由 CSS 子集解析器处理（见 3.8 节） |
| `href` | 超链接目标标识符，触发 `OnClick` |
| `src` | 内联图片资源 ID |
| `width` / `height` | 内联图片宽高（px，整数） |
| `class` | 保留桩，暂不实现选择器 |

**解析容错策略**：
- 嵌套深度上限 16，超出时截断，不 panic
- 未闭合标签就近闭合（最近父标签优先）
- 非法属性值静默忽略，回退到默认样式

---

## 2. 现状盘点

### 2.1 已完成能力

```
FontManager          ─ FT2 + HarfBuzz 成形，单 Face 单字号
FontAtlasManager     ─ FT2 字形 → TextureAtlas GPU 纹理图集
TextTextureCache     ─ LRU 256 条，R8 alpha-mask，整串缓存
TextRenderHelper     ─ 基于 Atlas 的逐字符顶点生成
TextRenderer         ─ renderText / renderTextEdit，批渲染
TextUtils            ─ UTF-8 字节偏移迭代（无 Grapheme 簇感知）
TextEditingService   ─ 光标/选区/多行，字节索引
```

### 2.2 已预留但未实现

```
TextFlag::RICH_TEXT  (bit 4)
TextFlag::ANSI       (bit 6)
TextFlag::UNDERLINE  (bit 7)
```

### 2.3 已知缺口

| 缺口 | 影响 |
|------|------|
| `FontManager` 单 Face | 无法切换粗/斜体 |
| 整串缓存键 | 富文本 Span 粒度缓存键需重设计 |
| 字节光标 | Emoji/CJK Grapheme 簇光标偏移错误 |
| 无 IR 层 | 标记解析结果无处存储 |
| 下划线无渲染支持 | ShapeRenderer / 无专用绘制路径 |

---

## 3. 架构设计

### 3.1 总体分层

```
┌──────────────────────────────────────────────────────┐
│  Text 组件 content (raw string / marked-up string)   │
└──────────────────┬───────────────────────────────────┘
                   │ TextFlag::RICH_TEXT / ANSI 触发
                   ▼
┌──────────────────────────────────────────────────────┐
│  解析层  RichTextParser  (新增)                        │
│  输入: string + TextFlag                              │
│  输出: RichTextDocument (Span 列表 + 元数据)           │
└──────────────────┬───────────────────────────────────┘
                   │
                   ▼
┌──────────────────────────────────────────────────────┐
│  IR 层  RichTextDocument / Span 模型  (新增)           │
│  附加到实体: components::RichTextDoc (新组件)          │
└──────────────────┬───────────────────────────────────┘
                   │
                   ▼
┌──────────────────────────────────────────────────────┐
│  排版层  RichTextLayoutEngine  (新增)                  │
│  输入: RichTextDocument + wrapWidth                   │
│  输出: LineList (每行 GlyphRun 序列)                   │
└──────────────────┬───────────────────────────────────┘
                   │
                   ▼
┌──────────────────────────────────────────────────────┐
│  渲染层  TextRenderer::renderRichText  (扩展现有)      │
│  消费 LineList → 多 Face Atlas 取字形 → 批渲染         │
│  下划线 → ShapeRenderer::addLine  (扩展现有)           │
└──────────────────────────────────────────────────────┘
```

### 3.2 Span 模型（IR）

```cpp
// src/core/RichText.hpp  （新增）
namespace ui::rich
{

enum class SpanType : uint8_t
{
    TEXT,        // 纯文本段
    IMAGE,       // 内联图片占位
    NEWLINE,     // 显式换行
};

struct SpanStyle
{
    Color color{1.f, 1.f, 1.f, 1.f};
    float fontSize       = 0.f;   // 0 = 继承父
    bool  bold           = false;
    bool  italic         = false;
    bool  underline      = false;
    bool  hasLink        = false;
    std::string linkId;            // 触发 OnClick 用的标识符
};

struct Span
{
    SpanType  type  = SpanType::TEXT;
    std::string text;              // type==TEXT 时有效
    std::string imageId;           // type==IMAGE 时有效（对应 ImageSource::path）
    float imageWidth  = 0.f;
    float imageHeight = 0.f;
    SpanStyle style;
};

struct ParsedChunk
{
    size_t            startByteOffset = 0; // 在原始 input 中的起始字节偏移
    std::vector<Span> spans;               // 该分段解析出的 Span 列表
};

struct RichTextDocument
{
    std::vector<Span>        spans;            // 展平后的完整 Span 列表（向后兼容）
    std::vector<ParsedChunk> chunks;           // 增量解析分段缓存
    uint64_t                 contentHash = 0;  // FNV-1a 64-bit，仅 hash 变化时重解析
    bool                     dirty       = true; // 标脏，触发重新排版
};

} // namespace ui::rich
```

作为 ECS 组件附加到实体（`components::RichTextDoc`，含 `is_component_tag`），与 `components::Text` 共存：`Text::content` 仍作原始字符串；`RichTextDoc` 是解析缓存，`dirty` 由内容变更时置位。

### 3.3 解析层

**已决策：HTML 子集解析器**（Q1 决策，原占位方案废弃）。所有解析器均构建在 **HSM2/hfsm2** 状态机框架之上（见 3.9 节），禁止在 hfsm2 之外自行编写大型 switch/if-else 状态机。

```
src/core/RichTextParser.hpp  （新增）
src/core/RichTextParser.cpp  （新增）
```

接口草案（签名不变）：

```cpp
namespace ui::rich
{
// 将 raw string 解析为 RichTextDocument
// parseMode 由 TextFlag::RICH_TEXT / ANSI 决定使用哪种语法
RichTextDocument Parse(std::string_view input, policies::TextFlag parseMode,
                       float defaultFontSize, Color defaultColor);
} // namespace ui::rich
```

解析触发点：`TextRenderer::collect()` 检测 `TextFlag::RICH_TEXT` 或 `ANSI`，若 `RichTextDoc.contentHash` 与当前内容的 FNV-1a 64-bit hash 不一致（`dirty == true`），则调用 `Parse()`，写回组件，更新 `contentHash`，清除 `dirty`。

**HTML Tokenizer 主状态**（hfsm2 `TransitionTable` 定义，类 HTML5 规范命名）：

| 状态 | 触发条件 | 迁移目标 |
|------|----------|-----------|
| `Data` | 遇到 `<` | `TagOpen` |
| `TagOpen` | 遇到字母 | `TagName` |
| `TagOpen` | 遇到 `/` | `EndTagName`（复用 TagName 路径） |
| `TagName` | 遇到空白 | `BeforeAttributeName` |
| `TagName` | 遇到 `>` | `Data`（标签结束） |
| `BeforeAttributeName` | 遇到字母 | `AttributeName` |
| `AttributeName` | 遇到 `=` | `AttributeValue` |
| `AttributeValue` | 遇到 `>` 或引号闭合 | `BeforeAttributeName` / `Data` |
| `SelfClosingTag`（遇 `/>`） | — | `Data` |
| `Text`（非标签内容累积） | — | `Data` |

**ANSI 模式**：独立状态机，解析 `ESC[...m` 序列，输出颜色 Span（路径与 HTML 模式互斥，由 `parseMode` 分发）。

两种解析模式互斥（Q4 已决策）：`TextFlag::RICH_TEXT` 优先，`TextFlag::ANSI` 同时设置时自动忽略。

### 3.4 排版层（RichTextLayoutEngine）

```
src/core/RichTextLayoutEngine.hpp  （新增）
```

职责：将 `RichTextDocument` + `wrapWidth` 转换为 `LineList`。

```cpp
namespace ui::rich
{

struct GlyphRun
{
    std::string text;         // 该 run 的文本内容
    SpanStyle   style;
    float       xOffset = 0;
    float       yOffset = 0;
    float       width   = 0;
    bool        isImage = false;
    std::string imageId;
    float       imageW  = 0;
    float       imageH  = 0;
};

struct LayoutLine
{
    std::vector<GlyphRun> runs;
    float lineWidth  = 0;
    float lineHeight = 0;
    float baseline   = 0;
};

using LineList = std::vector<LayoutLine>;

LineList Layout(const RichTextDocument& doc,
                float wrapWidth,
                float defaultFontSize,
                float lineHeightMultiplier,
                managers::MultiFontManager& fontManager);  // 见 3.5
} // namespace ui::rich
```

换行算法复用现有 `utils::WrapParagraph` 的逻辑，但需感知 Span 边界——不在 Span 内部截断样式，Span 本身可被分行。

### 3.5 多字体管理升级（MultiFontManager）

当前 `FontManager` 是单 Face 单字号。需要扩展为多 Face 注册表。

**不修改 `FontManager` 类本身**，而是在其上新建 `MultiFontManager`（组合而非继承）。

```
src/managers/MultiFontManager.hpp  （新增）
src/managers/MultiFontManager.cpp  （新增）
```

```cpp
namespace ui::managers
{

struct FontKey
{
    std::string family;   // "NotoSansSC"
    float       size;
    bool        bold;
    bool        italic;
    bool operator==(const FontKey&) const noexcept = default;
};

struct FontKeyHash
{
    size_t operator()(const FontKey& k) const noexcept;
};

class MultiFontManager
{
public:
    // 注册一个字体变体（加载到内部 FontManager 池）
    Result<void> registerFont(FontKey key, std::span<const uint8_t> data);

    // 获取最接近的 FontAtlasManager（无精确匹配时降级到默认字体）
    FontAtlasManager* resolve(const FontKey& key);

    // 获取默认字体（向后兼容）
    FontAtlasManager* defaultFont();

private:
    std::unordered_map<FontKey, std::unique_ptr<FontAtlasManager>, FontKeyHash> m_pool;
    FontKey m_defaultKey;
};

} // namespace ui::managers
```

`RenderContext` 中将 `FontManager*` 替换（或补充）为 `MultiFontManager*`；旧 `fontManager` 字段保留兼容，降级指向 `defaultFont()`。

### 3.6 Grapheme 簇方案

**结论：不引入 ICU，利用 HarfBuzz cluster 信息。**

理由：
1. HarfBuzz 已是依赖，其 `hb_buffer_get_glyph_infos()` 的 `cluster` 字段直接给出 UTF-8 字节偏移到字形的映射，足以支持 CJK / Emoji Grapheme 簇的正确光标推进。
2. ICU 体积约 30 MB，超出该项目对依赖体积的预期。

实施：在 `TextUtils.hpp` 中新增 `GraphemeIterator`（基于 HarfBuzz cluster），替换 `TextEditingService` 中直接的字节 `++pos` 逻辑。`cursorPosition` 语义保持字节偏移不变（外部接口不破坏），内部通过 `GraphemeIterator::next/prev` 推进。

### 3.7 下划线渲染

下划线是跨 Span 的装饰线，不适合作为字形处理。实现方案：

`TextRenderer::renderRichText` 在输出 `GlyphRun` 到 batch 之后，若 `style.underline == true`，额外向 `ShapeRenderer` 提交一条水平线段（`y = baseline + 2px`，宽度等于该 run 的 `width`）。`ShapeRenderer` 已有绘制矩形能力，下划线作为高度 1~2px 的填充矩形处理。

### 3.8 CSS 子集解析器

处理 `style` 属性内联值，与 HTML 解析器共用 hfsm2 架构，CSS Tokenizer 状态机独立。

```
src/core/CSSParser.hpp  （新增）
src/core/CSSParser.cpp  （新增）
```

**CSS Tokenizer 主状态**（处理 `style="..."` 内的声明列表）：

| 状态 | 描述 |
|------|------|
| `Ident` | 累积属性名（`color`、`font-size` 等） |
| `Colon` | 遇到 `:` 后等待值 |
| `Value` | 累积属性值 |
| `Semicolon` | 遇到 `;` 后回到 `Ident`，或遇引号终止 |
| `Delim` | 空白/无关字符跳过 |

**支持的 CSS 属性**：

| 属性 | 值格式 | 备注 |
|------|--------|---------|
| `color` | `#RRGGBB`、`#RGB`、`rgb(r,g,b)`、`rgba(r,g,b,a)`、CSS 命名色（16 基础色） | 解析失败静默忽略 |
| `font-size` | `<number>px`（整数或小数） | 单位只接受 `px` |
| `font-weight` | `bold` \| `normal` | — |
| `font-style` | `italic` \| `normal` | — |
| `text-decoration` | `underline` \| `none` | — |

**颜色命名色（16 基础色）**：`black`、`white`、`red`、`green`、`blue`、`yellow`、`cyan`、`magenta`、`orange`、`purple`、`pink`、`brown`、`gray`/`grey`、`lime`、`navy`、`teal`。

**数据结构**：

```cpp
namespace ui::rich
{
// CSS 解析结果，附加到 SpanStyle 上
struct CSSProperties
{
    std::optional<Color>  color;
    std::optional<float>  fontSize;
    std::optional<bool>   bold;
    std::optional<bool>   italic;
    std::optional<bool>   underline;
};

// 后续样式系统预留：StyleSheet（class 选择器桩）
struct StyleRule
{
    std::string    selector;    // ".class-name"，暂不实现匹配
    CSSProperties  properties;
};

struct StyleSheet
{
    std::vector<StyleRule> rules; // 占桩，后续版本实现级联
};

// 解析 style 属性值字符串
CSSProperties ParseInlineStyle(std::string_view styleAttr);

} // namespace ui::rich
```

`StyleSheet` 在此版本为纯数据结构占桩（无选择器匹配逻辑），为后续级联样式系统预留扩展点。

### 3.9 HSM2/hfsm2 状态机架构

所有解析器的词法/标记器状态机统一使用 **hfsm2**（Andrew Gresyk 的 header-only C++ 分层状态机库），不允许自行编写大型 switch/if-else 状态机。

**引入理由**：
- hfsm2 将状态迁移表声明为模板 `TransitionTable`，编译期验证，避免运行时非法迁移
- 分层状态（HSM）支持公共转换逻辑复用（如所有状态均可迁移到 `Error` 超状态）
- Header-only，无额外编译产物，与项目现有零依赖静态链接策略一致

**不引入的代价**：手写 HTML5 词法器约80+ 状态，维护成本极高；与 ANSI 状态机代码风格割裂。

**依赖引入方式**（WP-A4，见第 5 节）：

```
third_party/hfsm2/hfsm2.hpp    # single-header release
```

在 `CMakeLists.txt` 中添加：

```cmake
add_library(hfsm2 INTERFACE)
target_include_directories(hfsm2 INTERFACE ${CMAKE_SOURCE_DIR}/third_party/hfsm2)
target_link_libraries(ui PRIVATE hfsm2)
```

**MSVC 兼容性说明**：hfsm2 v0.9.x 在 MSVC 2022 + C++20 下测试通过；本项目使用 `/std:c++latest`，需在 WP-A4 验收时显式确认（见 R8）。

**状态机复用边界**：HTML Tokenizer、CSS Tokenizer、ANSI Tokenizer 各自为独立的 hfsm2 `Machine`，通过统一上下文接口交换解析结果，汇聚到 `RichTextParser::Parse()` 分发逻辑。

**增量解析集成**：当 `Text::content` 仅尾部追加时（日志场景），`Parse()` 通过 FNV-1a 64-bit hash（`RichTextDocument::contentHash`）检测局部变更，定位到最后一个 `ParsedChunk` 的 `startByteOffset`，仅对变更段重启 Tokenizer，已有 `ParsedChunk` 直接复用。

---

## 4. 改动类型分析

| 文件 | 类型 | 说明 |
|------|------|------|
| `src/core/RichText.hpp` | **新增** | Span / RichTextDocument IR 结构 |
| `src/core/RichTextParser.hpp` | **新增** | 解析器头文件 |
| `src/core/RichTextParser.cpp` | **新增** | 解析器实现（自定义标记 + ANSI 两路） |
| `src/core/RichTextLayoutEngine.hpp` | **新增** | 排版引擎头文件 |
| `src/core/RichTextLayoutEngine.cpp` | **新增** | 排版引擎实现 |
| `src/managers/MultiFontManager.hpp` | **新增** | 多字体注册表 |
| `src/managers/MultiFontManager.cpp` | **新增** | 多字体注册表实现 |
| `src/common/components/Data.hpp` | **修改（扩展）** | 新增 `RichTextDoc` 组件结构体；`Text` 字段不变 |
| `src/common/RenderTypes.hpp` | **修改（扩展）** | `RenderContext` 增加 `MultiFontManager*` 字段（向后兼容） |
| `src/renderers/TextRenderer.hpp` | **修改（扩展）** | 增加 `renderRichText()` 私有方法；`collect()` 增加 RICH_TEXT/ANSI 分支 |
| `src/core/TextUtils.hpp` | **修改（扩展）** | 增加 `GraphemeIterator` 基于 HarfBuzz cluster |
| `src/core/TextEditingService.hpp` | **修改（局部）** | 光标推进改用 `GraphemeIterator::next/prev` |
| `src/api/Text.hpp` / `Text.cpp` | **修改（扩展）** | 新增 Chain DSL：`RichText(string)`、`AnsiText(string)`（可选） |
| `src/managers/FontAtlasManager.hpp` | **只读参考** | 无需修改 |
| `src/managers/FontManager.hpp` | **只读参考** | 无需修改 |
| `src/managers/TextTextureCache.hpp` | **修改（可选）** | 缓存键扩展以支持字体变体（WP-B3 阶段决策） |
| `src/core/CSSParser.hpp` | **新增** | CSS 子集解析器头文件（`ParseInlineStyle` + `StyleSheet` 桩） |
| `src/core/CSSParser.cpp` | **新增** | CSS 子集解析器实现（CSS Tokenizer hfsm2 状态机） |
| `third_party/hfsm2/hfsm2.hpp` | **新增依赖** | hfsm2 header-only 分层状态机库（HTML/CSS/ANSI Tokenizer 共用） |
| `src/common/Policies.hpp` | **修改（注释）** | `TextFlag::RICH_TEXT` 与 `ANSI` 互斥关系标注 |

**改动类型汇总**：  
- 接口扩展（非破坏性）：`RenderContext`、`TextRenderer`、`TextUtils`  
- 局部逻辑修改（有测试覆盖需求）：`TextEditingService` 光标推进  
- 纯新增：`RichText.hpp/cpp`、`RichTextParser`、`CSSParser`、`RichTextLayoutEngine`、`MultiFontManager`  
- 新增依赖：`hfsm2`（header-only）

---

## 5. 工作包拆分

### WP-A：基础设施（无依赖，可立即启动）

**WP-A1：MultiFontManager**

- 目标：支持多 Face 注册与 `resolve(FontKey)` 降级查找
- 文件：`MultiFontManager.hpp/cpp`
- 验收标准：
  - 可注册至少 2 个字体变体（Regular / Bold）
  - `resolve` 在无精确匹配时返回默认字体而非 nullptr
  - 单元测试覆盖：注册、命中、降级三个路径

**WP-A2：RichText IR + 组件**

- 目标：定义 `Span`、`RichTextDocument`、`SpanStyle` 结构
- 文件：`src/core/RichText.hpp`，`Data.hpp` 增加 `RichTextDoc` 组件
- 验收标准：
  - 结构体编译通过，`RichTextDoc` 满足 `Component` concept
  - `dirty` 字段语义注释清晰

**WP-A3：GraphemeIterator**

- 目标：在 `TextUtils.hpp` 中实现基于 HarfBuzz cluster 的 `GraphemeIterator`
- 文件：`src/core/TextUtils.hpp`
- 验收标准：
  - 对 `"你好😀World"` 的 `next/prev` 迭代结果正确（5 个 Grapheme）
  - 单元测试覆盖

**WP-A4：hfsm2 依赖引入与 CMakeLists 集成**

- 目标：将 hfsm2 header-only 库加入 `third_party/hfsm2/`，并集成到构建系统
- 文件：`third_party/hfsm2/hfsm2.hpp`（新增）、`CMakeLists.txt`（顶层或 `src/CMakeLists.txt`，添加 `hfsm2` INTERFACE 库）
- 验收标准：
  - `target_link_libraries(ui PRIVATE hfsm2)` 配置无报错
  - MSVC + `/std:c++latest` 下 hfsm2 基础示例编译通过（验证 C++23 兼容性，见 R8）
  - 不引入额外编译产物（header-only）

---

### WP-B：解析与排版（依赖 WP-A2、WP-A4）

**WP-B0：CSS 子集解析器**

- 目标：实现 `CSSParser.hpp/cpp`，处理 `style` 内联属性值；`StyleSheet` 数据结构占桩
- 文件：`src/core/CSSParser.hpp/cpp`
- 依赖：WP-A4（hfsm2 引入）
- 验收标准：
  - `ParseInlineStyle("color:#FF0000; font-size:14px; font-weight:bold")` 正确解析 3 个属性
  - 颜色支持 `#RRGGBB`、`#RGB`、`rgb(r,g,b)`、`rgba(r,g,b,a)`、16 基础命名色
  - 非法属性值静默忽略，不崩溃
  - `StyleSheet` 结构体编译通过（含 `StyleRule`，暂无匹配实现）

**WP-B1：ANSI 解析器**

- 目标：实现 `Parse()` 的 ANSI 模式（`TextFlag::ANSI`），支持 4/8/24-bit 颜色转义和重置；状态机基于 hfsm2
- 文件：`src/core/RichTextParser.hpp/cpp`
- 依赖：WP-A4（hfsm2 引入）
- 验收标准：
  - 输入 `"\e[31m红色\e[0m普通"` → 两个 Span，颜色正确
  - 非法转义序列作为纯文本透传（不崩溃）

**WP-B2：HTML 子集解析器**（Q1 已决策）

- 目标：实现 `Parse()` 的 HTML 富文本模式（`TextFlag::RICH_TEXT`），基于 hfsm2 HTML Tokenizer 状态机（见 3.3/3.9 节）
- 文件：`src/core/RichTextParser.hpp/cpp`（与 B1 同文件）
- 支持标签：`<b>`、`<i>`、`<u>`、`<s>`、`<span>`、`<a href="...">`、`<img src="..." width="..." height="...">`、`<br>`
- 依赖：WP-A4（hfsm2 引入）、WP-B0（CSS 解析器，处理 `style` 属性）
- 验收标准：
  - 输入 `<b>粗体</b><span style="color:#FF0000">红色</span>` → 两个 Span，样式正确
  - 嵌套标签深度上限 ≤ 16（防止栈溢出）
  - 未闭合标签按最近闭合处理，不 panic
  - `class` 属性被接受但不触发选择器匹配（桩行为）

**WP-B3：RichTextLayoutEngine**

- 目标：将 `RichTextDocument` 排版为 `LineList`（含换行、多字号行高计算）
- 文件：`src/core/RichTextLayoutEngine.hpp/cpp`
- 依赖：WP-A1（MultiFontManager 的测量接口）
- 验收标准：
  - 单行、多样式混排宽度计算误差 < 1px（与 FontAtlasManager 测量一致）
  - 内联图片占位正确参与换行
  - 空文档输入返回 1 条空行（不崩溃）

---

### WP-C：渲染集成（依赖 WP-B）

**WP-C1：TextRenderer 富文本路径**

- 目标：`TextRenderer::collect()` 识别 `RichTextDoc`，调用 `renderRichText()`
- 文件：`src/renderers/TextRenderer.hpp`
- 验收标准：
  - 在 example_ui_demo 中展示多色文本（至少 3 种颜色混排）
  - 不破坏现有纯文本渲染路径（回归测试通过）

**WP-C2：下划线渲染**

- 目标：`TextFlag::UNDERLINE` 或 Span.style.underline → ShapeRenderer 线段
- 文件：`src/renderers/TextRenderer.hpp`
- 验收标准：
  - 下划线与文本基线对齐，颜色与文本颜色一致
  - 换行时下划线跟随每行独立绘制

**WP-C3：TextEditingService Grapheme 光标**

- 目标：用 `GraphemeIterator` 替换字节级光标推进（仅推进逻辑，不改 cursorPosition 语义）
- 文件：`src/core/TextEditingService.hpp`
- 依赖：WP-A3
- 验收标准：
  - Emoji `😀` 单次左/右箭头光标移动跨越完整 4 字节
  - CJK 字符光标推进正确
  - 现有 TextEdit 单元测试全绿

---

### 里程碑

| 里程碑 | 包含工作包 | 外部可见效果 |
|--------|-----------|-------------|
| M1：基础设施就绪 | WP-A1/A2/A3/A4 | 多字体注册可用，GraphemeIterator 通过测试，hfsm2 引入完成 |
| M2：ANSI 可用 | WP-B1 | 日志/终端控件可渲染带颜色的 ANSI 文本 |
| M2.5：CSS 解析器就绪 | WP-B0 | CSS 内联样式可解析，StyleSheet 桩就位 |
| M3：富文本 MVP | WP-B2/B3 + WP-C1/C2 | `Text` 组件可展示 HTML 子集混合样式（颜色/粗体/斜体/下划线） |
| M4：光标修复 | WP-C3 | TextEdit 对 Emoji + CJK 光标行为正确 |

---

## 6. 风险清单

| # | 风险 | 概率 | 影响 | 应对策略 |
|---|------|------|------|---------|
| R1 | 粗/斜体字体文件缺失（项目只有一个 Variable Font） | 中 | 高 | WP-A1 优先验证 NotoSansSC-Variable 的 weight 轴是否覆盖 Bold；若覆盖则通过 FT_Set_Var_Design_Coordinates 实现伪粗体，无需额外字体文件 |
| R2 | `TextTextureCache` 整串 key 与 Span 粒度缓存冲突 | 高 | 中 | 富文本路径绕过 `TextTextureCache`，直接使用 `FontAtlasManager`（逐字符 Atlas 路径），缓存在 Atlas 层；整串缓存仅保留纯文本路径 |
| R3 | HarfBuzz cluster 在复杂脚本（泰文、印度文）下不等同于 Grapheme 边界 | 低 | 低 | 项目当前仅支持 NotoSansSC（CJK+Latin），暂不处理复杂脚本；在 `GraphemeIterator` 文档中注明 |
| R4 | `RichTextLayoutEngine` 多字号行高计算影响 Yoga Flexbox 自动高度 | 中 | 中 | WP-B3 验收要求与现有 `V_AUTO` 行为一致；`Size::V_AUTO` 需从 `LineList` 中取最大行高重新计算 |
| R5 | 解析器对格式错误输入崩溃（嵌套过深/未闭合） | 低 | 高 | 强制深度上限（16 层），未闭合标签降级处理；WP-B2 验收须包含模糊测试用例 |
| R6 | 内联图片纹理句柄生命周期与文本 Span 生命周期不一致 | 中 | 中 | 内联图片 Span 仅存 `imageId`（字符串键），纹理查找在渲染时通过 `ImageManager` 完成，不在 Span 持有原始指针 |
| R7 | `MultiFontManager` 引入后 `RenderContext` 破坏性修改影响所有 Renderer | 高 | 中 | 新字段设为 `MultiFontManager* multiFontManager = nullptr`，现有 Renderer 继续通过 `context.fontManager` 访问默认字体，零破坏 |
| R8 | hfsm2 学习曲线 + MSVC `/std:c++latest` 编译兼容性 | 低 | 中 | WP-A4 验收要求 MSVC 编译通过；hfsm2 有完整文档和示例；若编译失败可降级为宏生成状态机（保留 switch 路径作应急桩） |
| R9 | 增量解析 FNV-1a 64-bit hash 碰撞（极小概率下脏数据） | 极低 | 低 | 64-bit FNV-1a 碰撞概率约10⁻¹⁹，可接受；若极端场景有需要，可改为内容长度 + hash 双重校验 |

---

## 7. 待确认问题

**Q1 ✅ 已决策：富文本标记语言选型 → HTML 子集**

已选定 HTML 子集解析器，详见 1.3 节（支持标签/属性清单）和 3.3 节（解析器状态机架构）。

选项对比（存档）：

| 选项 | 优点 | 缺点 | 结论 |
|------|------|------|------|
| 自定义简化标记（如 `[color=#FF0000]文字[/color]`） | 完全可控语法，无歧义，解析器最小 | 客户端需学习新语法，已有 Markdown/HTML 内容无法直接复用 | ✗ 未选 |
| **HTML 子集**（`<b>`、`<i>`、`<span style="...">`） | **与 Web 习惯一致，易集成** | **属性解析较复杂，`style` 属性需 mini CSS 解析器** | ✅ 已选定 |
| Markdown 内联子集（`**bold**`、`*italic*`） | 对开发者最友好，文档即显示 | 嵌套语义模糊，颜色无标准写法，需扩展语法 | ✗ 未选 |

**Q2：是否需要富文本 `TextEdit`（可编辑富文本）？**  
本规划将 `TextEdit` 组件排除在富文本范围外。若后续有此需求，`RichTextDoc` 的 IR 设计需感知光标在 Span 内的位置，改动较大，建议单独立项。

**Q3：内联图片是否需要动画（GIF / Lottie）？**  
当前规划仅支持静态图片占位。动画内联图片需要帧调度机制介入排版层，建议明确答复后再设计。

**Q4 ✅ 已决策：`TextFlag::RICH_TEXT` 与 `TextFlag::ANSI` 互斥，`RICH_TEXT` 优先**

两者同时设置时，`RICH_TEXT` 模式激活，`ANSI` 自动忽略。实施要点：
- `Policies.hpp` 的 `TextFlag` 注释中标注互斥关系（WP-C1 实施时同步更新）
- `TextRenderer::collect()` 加运行时断言防止同时激活（优先 `RICH_TEXT`，ANSI 路径不进入）

**Q5 ✅ 已决策：增量解析设计已确认**

采用 `contentHash`（FNV-1a 64-bit）+ `ParsedChunk` 分段缓存方案。`RichTextDocument` 新增字段：
- `contentHash`：FNV-1a 64-bit hash，仅变化时重解析
- `chunks`：`std::vector<ParsedChunk>`，每段持有起始字节偏移 + Span 列表
- `dirty`：标脏标志，`contentHash` 不一致时置位

当 `Text::content` 仅尾部追加时（日志场景），仅重解析变更段，已有 `ParsedChunk` 复用。详见 3.2 节数据结构和 3.9 节增量解析集成描述。

---

*文档路径：`docs/RICH_TEXT_PLAN.md`*  
*关联文档：`docs/ARCHITECTURE.md` §6 渲染管线*
