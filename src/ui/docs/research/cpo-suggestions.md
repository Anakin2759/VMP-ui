# UI 模块里可以利用 CPO 的地方

## 结论

可以用，但应该只在 API 层选择性引入，不要把整个 UI 模块都改成 CPO 风格。

对这个仓库来说，最合适的不是一上来全面使用 `tag_invoke`，而是先在 `src/ui/api/` 层引入“函数对象型 CPO”。

原因很直接：

1. UI 模块现在的公开风格已经很接近 CPO 了
2. 当前 API 大量是 `SetX(entity, ...) + chains::X(...)` 的重复壳层
3. 这类重复最适合被 CPO 收敛
4. `LayoutSystem`、`RenderSystem` 这类内部系统并不适合为了 CPO 而 CPO

## 当前代码里最适合引入 CPO 的位置

### 1. 属性设置 API

这是最值得做的一块。

当前 `src/ui/api/` 下已经有非常明显的模式：

1. 一个自由函数负责真正修改 ECS 组件
2. 一个 `chains` 包装函数负责把它转成 `entity | Xxx(...)`

典型文件：

1. `src/ui/api/Controls.hpp`
2. `src/ui/api/Layout.hpp`
3. `src/ui/api/Size.hpp`
4. `src/ui/api/Text.hpp`
5. `src/ui/api/Visibility.hpp`

这类地方适合统一成“同一个 CPO 同时支持直接调用和链式调用”。

### 建议形态

先不要直接上 `tag_invoke`，先上简单函数对象：

```cpp
namespace ui::cpo
{
template <auto Fn>
struct EntityActionCpo
{
    template <typename... Args>
    void operator()(::entt::entity entity, Args&&... args) const
    {
        Fn(entity, std::forward<Args>(args)...);
    }

    template <typename... Args>
    auto bind(Args&&... args) const
    {
        return ui::chains::Chain{
            [... args = std::forward<Args>(args)](::entt::entity entity) mutable
            {
                Fn(entity, std::move(args)...);
            }};
    }
};

inline constexpr EntityActionCpo<&ui::controls::SetScrollMode> scroll_mode{};
inline constexpr EntityActionCpo<&ui::controls::SetScrollSpeed> scroll_speed{};
}
```

这样可以写成：

```cpp
ui::cpo::scroll_mode(entity, ui::policies::Scroll::Vertical);
entity | ui::cpo::scroll_mode.bind(ui::policies::Scroll::Vertical);
```

如果你想进一步贴近现在的链式用法，可以再包一层别名：

```cpp
inline auto ScrollMode(ui::policies::Scroll mode)
{
    return ui::cpo::scroll_mode.bind(mode);
}
```

这样迁移是渐进式的，不会把现有调用点全部打碎。

### 这块的收益

1. 减少 `SetX` 和 `chains::X` 的重复样板代码
2. 把“直接调用”和“链式调用”统一到同一个语义实体上
3. 将来如果要做 DSL 映射，CPO 名字也更适合作为注册键

## 2. 层级操作 API

如果 `src/ui/api/Hierarchy.hpp` 里继续扩展 `AddChild`、`RemoveChild`、`InsertChild`、`Reparent` 这类操作，CPO 也很合适。

原因是这类 API 有两个明显特点：

1. 语义强，名字稳定
2. 同时存在“立即调用”和“链式组合”的需求

例如：

```cpp
ui::cpo::add_child(parent, child);
parent | ui::cpo::add_child.bind(child);
```

这比维护一批 `SetX + Call<Fn>` 壳层更干净。

## 3. 主题/样式类 API

如果后面真的做 Theme 系统，这块也适合 CPO。

例如可以有：

1. `apply_theme`
2. `apply_variant`
3. `resolve_color`
4. `resolve_spacing`

这类点的特点是“调用名稳定，但不同对象的具体实现会变化”。

这里就开始接近 `tag_invoke` 型 CPO 的适用场景了，因为主题来源将来可能是：

1. 全局主题对象
2. 控件局部覆写
3. 运行时主题资源

## 4. 资源提供/后端抽象

这块不是第一优先级，但如果后面 UI 模块继续抽象多渲染后端、多资源来源，CPO 可以用在“能力查询”而不是“普通 setter”。

更适合的操作名是：

1. `load_font`
2. `load_icon`
3. `measure_text`
4. `draw_batch`
5. `resolve_resource`

这里如果要做，建议直接考虑 `tag_invoke`，因为这是“允许不同类型提供定制实现”的典型场景。

## 哪些地方不建议上 CPO

### 1. 系统内部实现

例如：

1. `LayoutSystem`
2. `RenderSystem`
3. `StateSystem`

这些地方主要问题是流程复杂、条件分支多、状态交织多。

这里引入 CPO 不会减少复杂度，只会把控制流再打散一层。

### 2. ECS 纯组件访问

例如简单的 `Registry::Get`、`Registry::TryGet`、`EmplaceOrReplace` 这种调用，不值得再包一层 CPO。

原因是：

1. 底层语义已经很明确
2. 再包会让调试路径变长
3. 不会显著减少重复

### 3. 工厂函数全集合直接改成 CPO

`CreateButton`、`CreateLabel`、`CreateWindow` 这类工厂，短期内不建议直接全部改成 CPO。

原因是工厂的“名词语义”很强，直接保留 `CreateX` 可读性更高。

如果以后做 DSL/反射注册，可以在工厂之上加一层统一创建入口，比如：

1. `make_widget(button_tag, ...)`
2. `make_widget(label_tag, ...)`

但这属于下一阶段，不是当前最划算的改造点。

## 这个仓库里更推荐哪种 CPO

## 第一阶段：函数对象型 CPO

推荐用于 `src/ui/api/`。

特点：

1. 实现简单
2. 基本不增加编译期复杂度
3. 容易和现有 `chains::Call` 风格共存
4. 对调用方侵入最小

这阶段的目标不是“追求最纯正的现代 C++ 风格”，而是消掉重复壳层。

## 第二阶段：tag_invoke 型 CPO

只建议用于真正需要“对外开放定制”的点：

1. 主题解析
2. 资源解析
3. 后端能力抽象
4. 文本测量/图标加载这类后端相关操作

不要把普通 setter 也做成 `tag_invoke`，那会明显过度设计。

## 一条现实的迁移路径

### Phase 1

挑一个最小闭环，比如 `Controls.hpp`，把下面几组先改成 CPO：

1. `scroll_mode`
2. `scroll_bar_policy`
3. `scroll_anchor`
4. `scroll_speed`

原因：

1. 这组 API 聚合度高
2. 调用点集中
3. 已经有明确的链式封装模式

### Phase 2

扩展到：

1. `Layout.hpp`
2. `Size.hpp`
3. `Visibility.hpp`
4. `Text.hpp`

把大量 `SetX + inline auto X(...)` 的重复形式统一掉。

### Phase 3

如果后面要做 DSL，再把这些 CPO 注册成 DSL 可调用的动作名。

这样 DSL 层不会直接依赖一堆散乱的自由函数，而是依赖稳定的动作对象。

## 一个更贴合当前项目的判断

如果你的目标是：

1. 减少 API 样板代码
2. 给 DSL 打基础
3. 保留当前 `entity | Xxx(...)` 使用习惯

那么 UI 模块确实有一批很适合用 CPO 的地方，而且价值主要集中在 `src/ui/api/`，不是 `src/ui/systems/`。

如果只做一件事，我建议先从 `Controls.hpp` 开始做函数对象型 CPO 试点，不要先动内部系统，也不要先上全面 `tag_invoke`。