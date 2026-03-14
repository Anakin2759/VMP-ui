/**
 * Implementation for UI factory API
 */

#include "Factory.hpp"

#include "../common/Components.hpp"
#include "../common/Tags.hpp"
#include "../common/Policies.hpp"
#include "../common/Types.hpp"
#include "../common/Events.hpp"
#include "../singleton/Registry.hpp"
#include "../singleton/Dispatcher.hpp"
#include "Utils.hpp"
#include "../core/PlatformWindow.hpp"
#include <SDL3/SDL_video.h>

#include <memory>
#include <system_error>

namespace ui::factory
{

std::expected<std::unique_ptr<Application>, std::error_code> CreateApplication(std::span<char*> argv)
{
    try
    {
        return std::make_unique<Application>(argv);
    }
    catch (const std::exception& e)
    {
        Logger::error("[Factory] UI initialization failed: {}", e.what());
        return std::unexpected(errors::make_error_code(errors::UiErrc::SdlInitFailed));
    }
    catch (...)
    {
        Logger::error("[Factory] Unknown UI initialization failure");
        return std::unexpected(errors::make_error_code(errors::UiErrc::UnknownInitializationFailure));
    }
}

entt::entity CreateBaseWidget(std::string_view alias)
{
    auto entity = Registry::Create();

    auto& baseInfo = Registry::Emplace<components::BaseInfo>(entity);
    baseInfo.alias = std::string(alias);

    Registry::Emplace<components::Position>(entity);
    Registry::Emplace<components::Size>(entity);
    Registry::Emplace<components::Alpha>(entity);
    Registry::Emplace<components::VisibleTag>(entity);
    Registry::Emplace<components::Hierarchy>(entity);
    Registry::Emplace<components::RootTag>(entity); // 默认标记为根节点

    Registry::EmplaceOrReplace<components::LayoutDirtyTag>(entity);

    return entity;
}

void CreateFadeInAnimation(entt::entity entity, float duration)
{
    if (!Registry::Valid(entity)) return;
    auto& alpha = Registry::GetOrEmplace<components::Alpha>(entity);
    alpha.value = 0.0F;
    auto& time = Registry::GetOrEmplace<components::AnimationTime>(entity);
    time.duration = duration;
    time.elapsed = 0.0F;
    auto& alphaAnim = Registry::GetOrEmplace<components::AnimationAlpha>(entity);
    alphaAnim.from = 0.0F;
    alphaAnim.to = 1.0F;
    Registry::EmplaceOrReplace<components::AnimatingTag>(entity);
}

entt::entity CreateButton(const std::string& content, std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    Registry::Emplace<components::ButtonTag>(entity);
    Registry::Emplace<components::Clickable>(entity);
    auto& text = Registry::Emplace<components::Text>(entity);
    text.content = content;
    text.alignment = ui::policies::Alignment::CENTER;
    text.fontSize = 0.0F;
    Registry::Get<components::Size>(entity).sizePolicy = ui::policies::Size::Auto;
    return entity;
}

entt::entity CreateLabel(const std::string& content, std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    Registry::Emplace<components::LabelTag>(entity);
    auto& text = Registry::Emplace<components::Text>(entity);
    text.content = content;
    Registry::Get<components::Size>(entity).sizePolicy = ui::policies::Size::Auto;
    return entity;
}

entt::entity CreateTextEdit(const std::string& placeholder, bool multiline, std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);

    auto& textEdit = Registry::Emplace<components::TextEdit>(entity);
    textEdit.placeholder = placeholder;
    textEdit.inputMode = multiline ? (ui::policies::TextFlag::Default | ui::policies::TextFlag::Multiline)
                                   : ui::policies::TextFlag::Default;
    textEdit.cursorPosition = 0;
    textEdit.selectionStart = 0;
    textEdit.selectionEnd = 0;
    textEdit.hasSelection = false;

    auto& text = Registry::Emplace<components::Text>(entity);
    text.content = "";
    Registry::Emplace<components::Clickable>(entity);
    Registry::Get<components::Size>(entity).minSize = {100.0F, multiline ? 80.0F : 30.0F};
    Registry::Emplace<components::TextEditTag>(entity);

    // Add Caret component for cursor rendering
    Registry::Emplace<components::Caret>(entity);

    return entity;
}

entt::entity CreateImage(void* textureId, float defaultWidth, float defaultHeight, std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    Registry::Emplace<components::ImageTag>(entity);
    auto& image = Registry::Emplace<components::Image>(entity);
    image.textureId = textureId;
    auto& size = Registry::Get<components::Size>(entity);
    size.size = {defaultWidth, defaultHeight};
    return entity;
}

entt::entity CreateArrow(const Vec2& start, const Vec2& end, std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    Registry::Emplace<components::ArrowTag>(entity);
    auto& arrow = Registry::Emplace<components::Arrow>(entity);
    arrow.startPoint = start;
    arrow.endPoint = end;
    auto& size = Registry::Get<components::Size>(entity);
    size.sizePolicy = ui::policies::Size::Auto;
    return entity;
}

entt::entity CreateSpacer(int stretchFactor, std::string_view alias)
{
    auto entity = Registry::Create();
    auto& baseInfo = Registry::Emplace<components::BaseInfo>(entity);
    baseInfo.alias = alias;
    Registry::Emplace<components::SpacerTag>(entity);
    Registry::Emplace<components::Hierarchy>(entity);
    Registry::Emplace<components::Position>(entity);

    // 添加 Size 组件，初始值为 0，避免布局不稳定
    auto& size = Registry::Emplace<components::Size>(entity);
    size.size = {0.0F, 0.0F};
    size.sizePolicy = ui::policies::Size::Auto;

    auto& spacer = Registry::Emplace<components::Spacer>(entity);
    spacer.stretchFactor = static_cast<uint8_t>(std::max(1, stretchFactor));

    Registry::Emplace<components::RootTag>(entity); // 默认标记为根节点
    Registry::EmplaceOrReplace<components::LayoutDirtyTag>(entity);
    return entity;
}

entt::entity CreateSpacer(float width, float height, std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    auto& size = Registry::Get<components::Size>(entity);
    size.size = {width, height};
    size.sizePolicy = ui::policies::Size::Fixed;
    return entity;
}

entt::entity CreateDialog(std::string_view title, std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    Registry::Emplace<components::DialogTag>(entity);
    auto& size = Registry::Get<components::Size>(entity);
    size.sizePolicy = ui::policies::Size::Fixed;
    auto& dialog = Registry::Emplace<components::Window>(entity);
    dialog.title = std::string(title);
    dialog.flags |= policies::WindowFlag::NoTitleBar;
    constexpr int DEFAULT_DIALOG_WIDTH = 400;
    constexpr int DEFAULT_DIALOG_HEIGHT = 300;
    SDL_Window* sdlWindow = SDL_CreateWindow(dialog.title.c_str(),
                                             DEFAULT_DIALOG_WIDTH,
                                             DEFAULT_DIALOG_HEIGHT,
                                             SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
    dialog.windowID = SDL_GetWindowID(sdlWindow);
    platform::InitCustomWindow(sdlWindow);
    Registry::Remove<components::VisibleTag>(entity);
    auto& dialogLayout = Registry::Emplace<components::LayoutInfo>(entity);
    dialogLayout.direction = policies::LayoutDirection::VERTICAL;
    Registry::Emplace<components::Padding>(entity);
    Registry::EmplaceOrReplace<components::LayoutDirtyTag>(entity);
    Logger::info("[Factory] Enqueuing WindowGraphicsContextSetEvent for dialog entity {}",
                 static_cast<uint32_t>(entity));
    Dispatcher::Trigger<events::WindowGraphicsContextSetEvent>({entity});

    // 自动创建自绘标题栏
    CreateTitleBar(entity, std::string(alias) + "_titleBar");

    return entity;
}

entt::entity CreateScrollArea(std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    Registry::Emplace<components::ScrollArea>(entity);
    auto& layout = Registry::Emplace<components::LayoutInfo>(entity);
    layout.direction = policies::LayoutDirection::VERTICAL;
    auto& size = Registry::Get<components::Size>(entity);
    size.sizePolicy = policies::Size::FillParent;
    return entity;
}

entt::entity CreateWindow(std::string_view title, std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    Registry::Emplace<components::WindowTag>(entity);
    auto& window = Registry::Emplace<components::Window>(entity);
    window.title = std::string(title);
    window.flags &= ~policies::WindowFlag::Modal;
    auto& size = Registry::Get<components::Size>(entity);
    size.sizePolicy = ui::policies::Size::Fixed;
    auto& layoutInfo = Registry::Emplace<components::LayoutInfo>(entity);
    layoutInfo.direction = policies::LayoutDirection::VERTICAL;
    Registry::Emplace<components::Padding>(entity);
    Registry::EmplaceOrReplace<components::LayoutDirtyTag>(entity);
    constexpr int DEFAULT_WINDOW_WIDTH = 800;
    constexpr int DEFAULT_WINDOW_HEIGHT = 600;
    SDL_Window* sdlWindow = SDL_CreateWindow(window.title.c_str(),
                                             DEFAULT_WINDOW_WIDTH,
                                             DEFAULT_WINDOW_HEIGHT,
                                             SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
    window.windowID = SDL_GetWindowID(sdlWindow);
    Logger::info("[Factory] Enqueuing WindowGraphicsContextSetEvent for window entity {}",
                 static_cast<uint32_t>(entity));
    Dispatcher::Trigger<events::WindowGraphicsContextSetEvent>({entity});
    Registry::Remove<components::VisibleTag>(entity);

    return entity;
}

entt::entity CreateTitleBar(entt::entity windowEntity, std::string_view alias)
{
    // 获取窗口组件
    auto* windowComp = Registry::TryGet<components::Window>(windowEntity);
    if (windowComp == nullptr)
    {
        Logger::warn("[Factory] CreateTitleBar: entity {} has no Window component",
                     static_cast<uint32_t>(windowEntity));
        return entt::null;
    }

    constexpr float TITLE_BAR_HEIGHT = 32.0F;
    constexpr float BTN_SIZE = 28.0F;
    constexpr float ICON_SIZE = 16.0F;
    constexpr float ICON_SPACING = 0.0F;
    constexpr float BTN_SPACING = 2.0F;
    constexpr uint32_t ICON_CLOSE = 0xE5CD;
    constexpr uint32_t ICON_MINIMIZE = 0xE931;
    constexpr uint32_t ICON_MAXIMIZE = 0xE930;

    // ---- 标题栏容器 (HBox) ----
    auto titleBar = CreateBaseWidget(alias);
    Registry::Emplace<components::TitleBarTag>(titleBar);
    auto& layout = Registry::Emplace<components::LayoutInfo>(titleBar);
    layout.direction = policies::LayoutDirection::HORIZONTAL;
    auto& titleBarSize = Registry::Get<components::Size>(titleBar);
    titleBarSize.size = {0.0F, TITLE_BAR_HEIGHT};
    titleBarSize.sizePolicy = policies::Size::HFill | policies::Size::VFixed;

    // 背景透明（与窗口背景融合）
    auto& background = Registry::Emplace<components::Background>(titleBar);
    background.color = {0.0F, 0.0F, 0.0F, 0.0F};
    background.enabled = policies::Feature::Enabled;

    // 可点击（使 HitTest 能命中该区域）+ 可拖拽（拖拽移动窗口）
    Registry::Emplace<components::Clickable>(titleBar);
    auto& draggable = Registry::Emplace<components::Draggable>(titleBar);
    draggable.lockX = true; // 不修改实体自身 Position
    draggable.lockY = true;

    // 通过 SDL API 移动窗口
    uint32_t windowID = windowComp->windowID;
    draggable.onDragMove = [windowID](Vec2 delta)
    {
        SDL_Window* sdlWin = SDL_GetWindowFromID(windowID);
        if (sdlWin == nullptr) return;
        int curX = 0;
        int curY = 0;
        SDL_GetWindowPosition(sdlWin, &curX, &curY);
        SDL_SetWindowPosition(sdlWin, curX + static_cast<int>(delta.x()), curY + static_cast<int>(delta.y()));
    };

    // ---- 标题文本 ----
    auto titleLabel = CreateLabel(windowComp->title, std::string(alias) + "_title");
    auto& titleText = Registry::Get<components::Text>(titleLabel);
    titleText.fontSize = 13.0F;
    titleText.alignment = policies::Alignment::LEFT | policies::Alignment::VCENTER;

    // ---- 弹性间隔 ----
    auto spacer = CreateSpacer(1, std::string(alias) + "_spacer");

    // ---- 窗口控制按钮 ----
    auto createWinBtn = [&](const std::string& btnAlias, uint32_t iconCodepoint) -> entt::entity
    {
        auto btn = CreateButton("", btnAlias);
        auto& btnSize = Registry::Get<components::Size>(btn);
        btnSize.size = {BTN_SIZE, BTN_SIZE};
        btnSize.sizePolicy = policies::Size::Fixed;

        auto& btnBg = Registry::GetOrEmplace<components::Background>(btn);
        btnBg.color = {0.0F, 0.0F, 0.0F, 0.0F};
        btnBg.borderRadius = {4.0F, 4.0F, 4.0F, 4.0F};
        btnBg.enabled = policies::Feature::Enabled;

        // 字体图标
        auto& iconComp = Registry::Emplace<components::Icon>(btn);
        iconComp.codepoint = iconCodepoint;
        iconComp.size = {ICON_SIZE, ICON_SIZE};
        iconComp.spacing = ICON_SPACING;
        iconComp.tintColor = {0.85F, 0.85F, 0.85F, 1.0F};

        return btn;
    };

    auto minimizeBtn = createWinBtn(std::string(alias) + "_minimize", ICON_MINIMIZE);
    Registry::Get<components::Clickable>(minimizeBtn).onClick = [windowID]()
    {
        SDL_Window* sdlWin = SDL_GetWindowFromID(windowID);
        if (sdlWin != nullptr) SDL_MinimizeWindow(sdlWin);
    };

    auto maximizeBtn = createWinBtn(std::string(alias) + "_maximize", ICON_MAXIMIZE);
    Registry::Get<components::Clickable>(maximizeBtn).onClick = [windowID]()
    {
        SDL_Window* sdlWin = SDL_GetWindowFromID(windowID);
        if (sdlWin == nullptr) return;
        if ((SDL_GetWindowFlags(sdlWin) & SDL_WINDOW_MAXIMIZED) != 0)
        {
            SDL_RestoreWindow(sdlWin);
        }
        else
        {
            SDL_MaximizeWindow(sdlWin);
        }
    };

    auto closeBtn = createWinBtn(std::string(alias) + "_close", ICON_CLOSE);
    // 关闭按钮悬停时红色背景
    auto& closeBtnHover = Registry::Emplace<components::Hoverable>(closeBtn);
    closeBtnHover.onHover = [closeBtn]()
    {
        auto* closeBg = Registry::TryGet<components::Background>(closeBtn);
        if (closeBg != nullptr) closeBg->color = {0.9F, 0.2F, 0.2F, 1.0F};
    };
    closeBtnHover.onUnhover = [closeBtn]()
    {
        auto* closeBg = Registry::TryGet<components::Background>(closeBtn);
        if (closeBg != nullptr) closeBg->color = {0.0F, 0.0F, 0.0F, 0.0F};
    };
    Registry::Get<components::Clickable>(closeBtn).onClick = [windowEntity]() { ui::utils::CloseWindow(windowEntity); };

    // ---- 组装层级 ----
    auto& titleBarHierarchy = Registry::Get<components::Hierarchy>(titleBar);
    auto addChild = [&](entt::entity child)
    {
        auto& childHierarchy = Registry::Get<components::Hierarchy>(child);
        childHierarchy.parent = titleBar;
        Registry::Remove<components::RootTag>(child);
        titleBarHierarchy.children.push_back(child);
    };

    addChild(titleLabel);
    addChild(spacer);
    addChild(minimizeBtn);
    addChild(maximizeBtn);
    addChild(closeBtn);

    // 将标题栏加入窗口（插入为第一个子节点）
    auto& windowHierarchy = Registry::Get<components::Hierarchy>(windowEntity);
    auto& titleBarH = Registry::Get<components::Hierarchy>(titleBar);
    titleBarH.parent = windowEntity;
    Registry::Remove<components::RootTag>(titleBar);
    windowHierarchy.children.insert(windowHierarchy.children.begin(), titleBar);

    Registry::EmplaceOrReplace<components::LayoutDirtyTag>(titleBar);
    Registry::EmplaceOrReplace<components::LayoutDirtyTag>(windowEntity);

    // 标题栏间距 (Top, Right, Bottom, Left)
    auto& padding = Registry::Emplace<components::Padding>(titleBar);
    padding.values = {0.0F, BTN_SPACING, 0.0F, 8.0F};

    auto& layoutInfo = Registry::Get<components::LayoutInfo>(titleBar);
    layoutInfo.spacing = BTN_SPACING;

    return titleBar;
}

entt::entity CreateVBoxLayout(std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    auto& layout = Registry::Emplace<components::LayoutInfo>(entity);
    layout.direction = ui::policies::LayoutDirection::VERTICAL;
    auto& size = Registry::Get<components::Size>(entity);
    size.sizePolicy = ui::policies::Size::Auto;
    Registry::Emplace<components::Padding>(entity);
    return entity;
}

entt::entity CreateHBoxLayout(std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    auto& layout = Registry::Emplace<components::LayoutInfo>(entity);
    layout.direction = ui::policies::LayoutDirection::HORIZONTAL;
    auto& size = Registry::Get<components::Size>(entity);
    size.sizePolicy = ui::policies::Size::Auto;
    Registry::Emplace<components::Padding>(entity);
    return entity;
}

entt::entity CreateLineEdit(std::string_view initialText, std::string_view placeholder, std::string_view alias)
{
    auto entity = CreateTextEdit(std::string(placeholder), false, alias);
    auto& edit = Registry::Get<components::TextEdit>(entity);
    edit.buffer = std::string(initialText);
    edit.cursorPosition = edit.buffer.size(); // Place cursor at end
    auto& text = Registry::Get<components::Text>(entity);
    text.content = edit.buffer;
    return entity;
}

entt::entity CreateTextBrowser(std::string_view initialText, std::string_view placeholder, std::string_view alias)
{
    auto entity = CreateTextEdit(std::string(placeholder), true, alias);
    auto& edit = Registry::Get<components::TextEdit>(entity);
    edit.buffer = std::string(initialText);
    edit.cursorPosition = 0; // Start at beginning for read-only
    edit.inputMode = policies::TextFlag::ReadOnly | policies::TextFlag::Multiline;
    auto& text = Registry::Get<components::Text>(entity);
    text.content = edit.buffer;

    // 添加 ScrollArea 组件以支持滚动
    auto& scrollArea = Registry::Emplace<components::ScrollArea>(entity);
    scrollArea.scroll = policies::Scroll::Vertical;
    scrollArea.scrollBar = policies::ScrollBar::Draggable | policies::ScrollBar::AutoHide;
    scrollArea.anchor = policies::ScrollAnchor::Smart; // 设置为智能模式

    text.alignment = policies::Alignment::TOP | policies::Alignment::LEFT;
    text.wordWrap = policies::TextWrap::Word; // 自动换行

    // 确保尺寸策略允许填充父容器
    auto& size = Registry::Get<components::Size>(entity);
    size.sizePolicy = policies::Size::FillParent;

    return entity;
}

entt::entity CreateCheckBox(const std::string& label, bool checked, std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    // TODO: 实现 CheckBoxTag 和 CheckBox 组件
    // Registry::Emplace<components::CheckBoxTag>(entity);
    // auto& checkBox = Registry::Emplace<components::CheckBox>(entity);
    // checkBox.checked = checked;
    auto& text = Registry::Emplace<components::Text>(entity);
    text.content = label;
    text.alignment = ui::policies::Alignment::LEFT | ui::policies::Alignment::VCENTER;
    Registry::Get<components::Size>(entity).sizePolicy = ui::policies::Size::Auto;
    return entity;
}

entt::entity CreateSlider(std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    Registry::Emplace<components::SliderInfo>(entity);
    // 默认尺寸：横向滑块高度 28
    auto& size = Registry::Get<components::Size>(entity);
    size.size = {200.0F, 28.0F};
    size.sizePolicy = ui::policies::Size::Fixed;
    Registry::Emplace<components::LayoutInfo>(entity);
    Registry::EmplaceOrReplace<components::LayoutDirtyTag>(entity);
    Registry::Emplace<components::SliderTag>(entity);
    return entity;
}

entt::entity CreateProgressBar(std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    Registry::Emplace<components::ProgressBar>(entity);
    auto& size = Registry::Get<components::Size>(entity);
    size.size = {200.0F, 14.0F};
    size.sizePolicy = ui::policies::Size::Fixed;
    Registry::Emplace<components::LayoutInfo>(entity);
    Registry::EmplaceOrReplace<components::LayoutDirtyTag>(entity);
    Registry::Emplace<components::ProgressBarTag>(entity);
    return entity;
}

} // namespace ui::factory
