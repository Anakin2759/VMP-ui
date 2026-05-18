

#include "Factory.hpp"

#include "../common/Components.hpp"
#include "../common/Tags.hpp"
#include "../common/Policies.hpp"
#include "../common/Types.hpp"
#include "../common/Events.hpp"
#include "../common/UiErrors.hpp"
#include "../core/RuntimeFacade.hpp"
#include "../singleton/Logger.hpp"
#include "../singleton/Registry.hpp"
#include "../singleton/Dispatcher.hpp"
#include "Hierarchy.hpp"
#include "Utils.hpp"
#include "Animation.hpp"
#include "../core/PlatformWindow.hpp"
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>

#include <cmath>
#include <memory>

namespace ui::factory
{

namespace
{
struct TitleBarDragState
{
    Vec2 dragStartMouseGlobal{0.0F, 0.0F};
    Vec2 dragStartWindowPos{0.0F, 0.0F};
    bool dragAnchorValid = false;
};

SDL_Window* CreateSdlWindowOrRollback(
    entt::entity entity, const char* title, int width, int height, SDL_WindowFlags flags, std::string_view entityType)
{
    SDL_Window* sdlWindow = SDL_CreateWindow(title, width, height, flags);
    if (sdlWindow == nullptr)
    {
        Logger::error("[Factory] Failed to create SDL window for {} entity {}: {}",
                      entityType,
                      static_cast<uint32_t>(entity),
                      SDL_GetError());
        Registry::Destroy(entity);
        return nullptr;
    }

    return sdlWindow;
}

bool AssignWindowIdOrRollback(entt::entity entity,
                              components::Window& window,
                              SDL_Window* sdlWindow,
                              std::string_view entityType)
{
    window.windowID = SDL_GetWindowID(sdlWindow);
    if (window.windowID == 0)
    {
        Logger::error("[Factory] Failed to fetch SDL window ID for {} entity {}: {}",
                      entityType,
                      static_cast<uint32_t>(entity),
                      SDL_GetError());
        SDL_DestroyWindow(sdlWindow);
        Registry::Destroy(entity);
        return false;
    }

    RuntimeFacade::current().windowLookup().remember(entity);

    return true;
}

entt::entity CreateTitleBarContainer(std::string_view alias, float titleBarHeight)
{
    auto titleBar = CreateBaseWidget(alias);
    Registry::Emplace<components::TitleBarTag>(titleBar);

    auto& layout = Registry::Emplace<components::LayoutInfo>(titleBar);
    layout.direction = policies::LayoutDirection::HORIZONTAL;
    layout.alignment = policies::Alignment::LEFT | policies::Alignment::VCENTER;

    auto& titleBarSize = Registry::Get<components::Size>(titleBar);
    titleBarSize.size = {0.0F, titleBarHeight};
    titleBarSize.sizePolicy = policies::Size::HFill | policies::Size::VFixed;

    auto& background = Registry::Emplace<components::Background>(titleBar);
    background.color = {0.0F, 0.0F, 0.0F, 0.0F};
    background.enabled = policies::Feature::Enabled;

    Registry::Emplace<components::Clickable>(titleBar);
    auto& draggable = Registry::Emplace<components::Draggable>(titleBar);
    draggable.lockX = true;
    draggable.lockY = true;

    return titleBar;
}

void ConfigureTitleBarDragging(entt::entity titleBar, entt::entity windowEntity, uint32_t windowId)
{
    auto& draggable = Registry::Get<components::Draggable>(titleBar);
    auto dragState = std::make_shared<TitleBarDragState>();

    draggable.onDragStart = [windowEntity, windowId, dragState]()
    {
        SDL_Window* sdlWindow = SDL_GetWindowFromID(windowId);
        auto* position = Registry::TryGet<components::Position>(windowEntity);
        if (sdlWindow == nullptr || position == nullptr)
        {
            dragState->dragAnchorValid = false;
            return;
        }

        float globalMouseX = 0.0F;
        float globalMouseY = 0.0F;
        SDL_GetGlobalMouseState(&globalMouseX, &globalMouseY);

        int windowX = 0;
        int windowY = 0;
        SDL_GetWindowPosition(sdlWindow, &windowX, &windowY);

        dragState->dragStartMouseGlobal = Vec2{globalMouseX, globalMouseY};
        dragState->dragStartWindowPos = Vec2{static_cast<float>(windowX), static_cast<float>(windowY)};
        position->value = dragState->dragStartWindowPos;
        dragState->dragAnchorValid = true;
    };

    draggable.onDragMove = [windowEntity, windowId, dragState]([[maybe_unused]] Vec2 delta)
    {
        SDL_Window* sdlWindow = SDL_GetWindowFromID(windowId);
        if (sdlWindow == nullptr) return;

        auto* position = Registry::TryGet<components::Position>(windowEntity);
        if (position == nullptr) return;

        float globalMouseX = 0.0F;
        float globalMouseY = 0.0F;
        SDL_GetGlobalMouseState(&globalMouseX, &globalMouseY);

        int currentX = 0;
        int currentY = 0;
        SDL_GetWindowPosition(sdlWindow, &currentX, &currentY);

        constexpr float POSITION_EPSILON = 0.01F;
        if (!dragState->dragAnchorValid)
        {
            dragState->dragStartMouseGlobal = Vec2{globalMouseX, globalMouseY};
            dragState->dragStartWindowPos = Vec2{static_cast<float>(currentX), static_cast<float>(currentY)};
            dragState->dragAnchorValid = true;
        }
        else if (std::abs(position->value.x()) < POSITION_EPSILON && std::abs(position->value.y()) < POSITION_EPSILON)
        {
            position->value = Vec2{static_cast<float>(currentX), static_cast<float>(currentY)};
        }

        const Vec2 globalDelta = Vec2{globalMouseX, globalMouseY} - dragState->dragStartMouseGlobal;
        position->value = dragState->dragStartWindowPos + globalDelta;

        const int targetX = static_cast<int>(std::lround(position->value.x()));
        const int targetY = static_cast<int>(std::lround(position->value.y()));

        if (targetX != currentX || targetY != currentY)
        {
            SDL_SetWindowPosition(sdlWindow, targetX, targetY);
        }
    };

    draggable.onDragEnd = [dragState]() { dragState->dragAnchorValid = false; };
}

entt::entity CreateWindowControlButton(
    const std::string& buttonAlias, uint32_t iconCodepoint, float buttonSize, float iconSize, float iconSpacing)
{
    auto button = CreateButton("", buttonAlias);
    auto& buttonSizeComp = Registry::Get<components::Size>(button);
    buttonSizeComp.size = {buttonSize, buttonSize};
    buttonSizeComp.sizePolicy = policies::Size::Fixed;

    auto& buttonBackground = Registry::GetOrEmplace<components::Background>(button);
    buttonBackground.color = {0.0F, 0.0F, 0.0F, 0.0F};
    buttonBackground.borderRadius = {4.0F, 4.0F, 4.0F, 4.0F};
    buttonBackground.enabled = policies::Feature::Enabled;

    auto& iconComp = Registry::Emplace<components::Icon>(button);
    iconComp.codepoint = iconCodepoint;
    iconComp.size = {iconSize, iconSize};
    iconComp.spacing = iconSpacing;
    iconComp.tintColor = {0.85F, 0.85F, 0.85F, 1.0F};

    return button;
}

void AppendChild(entt::entity parent, entt::entity child)
{
    auto& parentHierarchy = Registry::Get<components::Hierarchy>(parent);
    auto& childHierarchy = Registry::Get<components::Hierarchy>(child);
    childHierarchy.parent = parent;
    Registry::Remove<components::RootTag>(child);
    parentHierarchy.children.push_back(child);
}

void AttachTitleBarToWindow(entt::entity titleBar, entt::entity windowEntity)
{
    auto& windowHierarchy = Registry::Get<components::Hierarchy>(windowEntity);
    auto& titleBarHierarchy = Registry::Get<components::Hierarchy>(titleBar);
    titleBarHierarchy.parent = windowEntity;
    Registry::Remove<components::RootTag>(titleBar);
    windowHierarchy.children.insert(windowHierarchy.children.begin(), titleBar);
}

void MarkAsRoot(entt::entity entity)
{
    Registry::EmplaceOrReplace<components::RootTag>(entity);
}
} // namespace

std::expected<std::unique_ptr<Application>, std::error_code> CreateApplication(std::span<char*> argv)
{
    try
    {
        return std::make_unique<Application>(argv);
    }
    catch (const std::exception& e)
    {
        Logger::error("[Factory] UI initialization failed: {}", e.what());
        return std::unexpected(ui::errors::make_error_code(ui::errors::UiErrc::SdlInitFailed));
    }
    catch (...)
    {
        Logger::error("[Factory] Unknown UI initialization failure");
        return std::unexpected(ui::errors::make_error_code(ui::errors::UiErrc::UnknownInitializationFailure));
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

    utils::MarkLayoutAndVisualChanged(entity);

    return entity;
}

void CreateFadeInAnimation(entt::entity entity, float duration)
{
    if (!Registry::Valid(entity)) return;
    auto& alpha = Registry::GetOrEmplace<components::Alpha>(entity);
    alpha.value = 0.0F;
    animation::TweenOptions options;
    options.duration = duration;
    options.easing = policies::Easing::EASE_OUT_QUAD;
    options.mode = policies::Play::ONCE;
    options.autoCleanup = true;
    animation::StartAlphaAnimation(entity, 0.0F, 1.0F, options);
}

entt::entity CreateButton(const std::string& content, std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    Registry::Emplace<components::ButtonTag>(entity);
    Registry::Emplace<components::Clickable>(entity);
    auto& text = Registry::Emplace<components::Text>(entity);
    text.content = content;
    text.alignment = policies::Alignment::CENTER;
    text.fontSize = 0.0F;
    Registry::Get<components::Size>(entity).sizePolicy = policies::Size::Auto;
    return entity;
}

entt::entity CreateLabel(const std::string& content, std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    Registry::Emplace<components::LabelTag>(entity);
    auto& text = Registry::Emplace<components::Text>(entity);
    text.content = content;
    Registry::Get<components::Size>(entity).sizePolicy = policies::Size::Auto;
    return entity;
}

entt::entity CreateTextEdit(const std::string& placeholder, bool multiline, std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);

    auto& textEdit = Registry::Emplace<components::TextEdit>(entity);
    textEdit.placeholder = placeholder;
    textEdit.inputMode =
        multiline ? (policies::TextFlag::Default | policies::TextFlag::Multiline) : policies::TextFlag::Default;
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
    size.sizePolicy = policies::Size::Auto;
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
    size.sizePolicy = policies::Size::Auto;

    auto& spacer = Registry::Emplace<components::Spacer>(entity);
    spacer.stretchFactor = static_cast<uint8_t>(std::max(1, stretchFactor));

    utils::MarkLayoutAndVisualChanged(entity);
    return entity;
}

entt::entity CreateSpacer(float width, float height, std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    auto& size = Registry::Get<components::Size>(entity);
    size.size = {width, height};
    size.sizePolicy = policies::Size::Fixed;
    return entity;
}

entt::entity CreateDialog(std::string_view title, std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    MarkAsRoot(entity);
    Registry::Emplace<components::DialogTag>(entity);
    auto& size = Registry::Get<components::Size>(entity);
    size.sizePolicy = policies::Size::Fixed;
    auto& dialog = Registry::Emplace<components::Window>(entity);
    dialog.title = std::string(title);
    dialog.flags |= policies::WindowFlag::NoTitleBar;
    constexpr int DEFAULT_DIALOG_WIDTH = 400;
    constexpr int DEFAULT_DIALOG_HEIGHT = 300;
    SDL_Window* sdlWindow = CreateSdlWindowOrRollback(entity,
                                                      dialog.title.c_str(),
                                                      DEFAULT_DIALOG_WIDTH,
                                                      DEFAULT_DIALOG_HEIGHT,
                                                      SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN,
                                                      "dialog");
    if (sdlWindow == nullptr)
    {
        return entt::null;
    }

    if (!AssignWindowIdOrRollback(entity, dialog, sdlWindow, "dialog"))
    {
        return entt::null;
    }

    platform::InitCustomWindow(sdlWindow);
    Registry::Remove<components::VisibleTag>(entity);
    auto& dialogLayout = Registry::Emplace<components::LayoutInfo>(entity);
    dialogLayout.direction = policies::LayoutDirection::VERTICAL;
    dialogLayout.alignment = policies::Alignment::CENTER;
    Registry::Emplace<components::Padding>(entity);
    utils::MarkLayoutAndVisualChanged(entity);
    Logger::info("[Factory] Triggering WindowGraphicsContextSetEvent for dialog entity {}",
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
    layout.alignment = policies::Alignment::TOP_LEFT;
    auto& size = Registry::Get<components::Size>(entity);
    size.sizePolicy = policies::Size::FillParent;
    return entity;
}

entt::entity CreateWindow(std::string_view title, std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    MarkAsRoot(entity);
    Registry::Emplace<components::WindowTag>(entity);
    auto& window = Registry::Emplace<components::Window>(entity);
    window.title = std::string(title);
    window.flags &= ~policies::WindowFlag::Modal;
    auto& size = Registry::Get<components::Size>(entity);
    size.sizePolicy = policies::Size::Fixed;
    auto& layoutInfo = Registry::Emplace<components::LayoutInfo>(entity);
    layoutInfo.direction = policies::LayoutDirection::VERTICAL;
    layoutInfo.alignment = policies::Alignment::CENTER;
    Registry::Emplace<components::Padding>(entity);
    ui::utils::MarkLayoutAndVisualChanged(entity);
    constexpr int DEFAULT_WINDOW_WIDTH = 800;
    constexpr int DEFAULT_WINDOW_HEIGHT = 600;
    SDL_Window* sdlWindow = CreateSdlWindowOrRollback(entity,
                                                      window.title.c_str(),
                                                      DEFAULT_WINDOW_WIDTH,
                                                      DEFAULT_WINDOW_HEIGHT,
                                                      SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN,
                                                      "window");
    if (sdlWindow == nullptr)
    {
        return entt::null;
    }

    if (!AssignWindowIdOrRollback(entity, window, sdlWindow, "window"))
    {
        return entt::null;
    }

    Logger::info("[Factory] Triggering WindowGraphicsContextSetEvent for window entity {}",
                 static_cast<uint32_t>(entity));
    Dispatcher::Trigger<events::WindowGraphicsContextSetEvent>({entity});
    Registry::Remove<components::VisibleTag>(entity);

    return entity;
}

entt::entity CreateTitleBar(entt::entity windowEntity, std::string_view alias)
{
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

    auto titleBar = CreateTitleBarContainer(alias, TITLE_BAR_HEIGHT);
    uint32_t windowID = windowComp->windowID;
    ConfigureTitleBarDragging(titleBar, windowEntity, windowID);

    auto titleLabel = CreateLabel(windowComp->title, std::string(alias) + "_title");
    auto& titleText = Registry::Get<components::Text>(titleLabel);
    titleText.fontSize = 13.0F;
    titleText.alignment = policies::Alignment::LEFT | policies::Alignment::VCENTER;

    auto spacer = CreateSpacer(1, std::string(alias) + "_spacer");

    auto minimizeBtn =
        CreateWindowControlButton(std::string(alias) + "_minimize", ICON_MINIMIZE, BTN_SIZE, ICON_SIZE, ICON_SPACING);
    Registry::Get<components::Clickable>(minimizeBtn).onClick = [windowID]()
    {
        SDL_Window* sdlWin = SDL_GetWindowFromID(windowID);
        if (sdlWin != nullptr) SDL_MinimizeWindow(sdlWin);
    };

    auto maximizeBtn =
        CreateWindowControlButton(std::string(alias) + "_maximize", ICON_MAXIMIZE, BTN_SIZE, ICON_SIZE, ICON_SPACING);
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

    auto closeBtn =
        CreateWindowControlButton(std::string(alias) + "_close", ICON_CLOSE, BTN_SIZE, ICON_SIZE, ICON_SPACING);
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
    Registry::Get<components::Clickable>(closeBtn).onClick = [windowEntity]() { utils::CloseWindow(windowEntity); };

    AppendChild(titleBar, titleLabel);
    AppendChild(titleBar, spacer);
    AppendChild(titleBar, minimizeBtn);
    AppendChild(titleBar, maximizeBtn);
    AppendChild(titleBar, closeBtn);
    AttachTitleBarToWindow(titleBar, windowEntity);

    utils::MarkLayoutAndVisualChanged(titleBar);
    utils::MarkLayoutAndVisualChanged(windowEntity);

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
    layout.direction = policies::LayoutDirection::VERTICAL;
    layout.alignment = policies::Alignment::TOP_LEFT;
    auto& size = Registry::Get<components::Size>(entity);
    size.sizePolicy = policies::Size::Auto;
    Registry::Emplace<components::Padding>(entity);
    return entity;
}

entt::entity CreateHBoxLayout(std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    auto& layout = Registry::Emplace<components::LayoutInfo>(entity);
    layout.direction = policies::LayoutDirection::HORIZONTAL;
    layout.alignment = policies::Alignment::LEFT | policies::Alignment::VCENTER;
    auto& size = Registry::Get<components::Size>(entity);
    size.sizePolicy = policies::Size::Auto;
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
    Registry::Emplace<components::CheckBoxTag>(entity);
    auto& checkBox = Registry::Emplace<components::CheckBox>(entity);
    checkBox.checked = checked;
    checkBox.label   = label;
    auto& text = Registry::Emplace<components::Text>(entity);
    text.content   = label;
    text.alignment = policies::Alignment::LEFT | policies::Alignment::VCENTER;
    // 为文字留出左侧方框空间（约 24px）
    auto& padding = Registry::GetOrEmplace<components::Padding>(entity);
    padding.values = {0.0F, 0.0F, 0.0F, 24.0F}; // Top, Right, Bottom, Left
    auto& size = Registry::Get<components::Size>(entity);
    size.sizePolicy = policies::Size::Auto;
    size.size       = {120.0F, 22.0F};
    // 挂载点击回调：点击时切换 checked 状态并触发 onChanged
    auto& clickable = Registry::Emplace<components::Clickable>(entity);
    clickable.onClick = [entity]()
    {
        auto* checkBoxComp = Registry::TryGet<components::CheckBox>(entity);
        if (checkBoxComp == nullptr) return;
        checkBoxComp->checked = !checkBoxComp->checked;
        if (checkBoxComp->onChanged)
        {
            checkBoxComp->onChanged(checkBoxComp->checked);
        }
        ui::utils::MarkVisualChanged(entity);
    };
    return entity;
}

// ── DropDown 内部辅助函数（仅 Factory.cpp 可见）─────────────────────────────
// NOLINTBEGIN(readability-*,misc-*)
namespace
{

/// 向上遍历层级，找到最近的 WindowTag 祖先
entt::entity FindWindowRoot(entt::entity entity)
{
    entt::entity current = entity;
    while (current != entt::null && Registry::Valid(current))
    {
        if (Registry::AnyOf<components::WindowTag>(current))
        {
            return current;
        }
        const auto* hier = Registry::TryGet<components::Hierarchy>(current);
        current = (hier != nullptr) ? hier->parent : entt::null;
    }
    return entt::null;
}

/// 关闭并销毁弹出列表（defer 到下一帧执行，避免在 onClick 内部销毁自身实体）
void CloseDropDownPopup(entt::entity ddEntity)
{
    auto* dropDown = Registry::TryGet<components::DropDown>(ddEntity);
    if (dropDown == nullptr) return;
    if (dropDown->popupEntity == entt::null || !Registry::Valid(dropDown->popupEntity))
    {
        dropDown->open = false;
        return;
    }

    const entt::entity popupToDestroy = dropDown->popupEntity;
    dropDown->popupEntity = entt::null;
    dropDown->open        = false;
    ui::utils::MarkVisualChanged(ddEntity);

    // 延迟到下一帧销毁，确保当前事件处理完毕（避免在 onClick 内删除含 onClick 的实体）
    ui::utils::InvokeTask([popupToDestroy]()
    {
        if (!Registry::Valid(popupToDestroy)) return;

        // 从父级 children 列表中移除
        const auto* popupHier = Registry::TryGet<components::Hierarchy>(popupToDestroy);
        if (popupHier != nullptr && popupHier->parent != entt::null)
        {
            hierarchy::RemoveChild(popupHier->parent, popupToDestroy);
        }

        // 深度优先收集子树中所有实体后逆序销毁
        std::vector<entt::entity> toDestroy;
        std::vector<entt::entity> stack{popupToDestroy};
        while (!stack.empty())
        {
            const entt::entity cur = stack.back();
            stack.pop_back();
            if (!Registry::Valid(cur)) continue;
            toDestroy.push_back(cur);
            if (const auto* hier = Registry::TryGet<components::Hierarchy>(cur))
            {
                for (const entt::entity child : hier->children)
                {
                    stack.push_back(child);
                }
            }
        }
        for (entt::entity ent : std::ranges::reverse_view(toDestroy))
        {
            if (Registry::Valid(ent))
            {
                Registry::Destroy(ent);
            }
        }
    });
}

/// 打开弹出列表（在 ddEntity 正下方创建悬浮选项面板）
void OpenDropDownPopup(entt::entity ddEntity)
{
    auto* dropDown = Registry::TryGet<components::DropDown>(ddEntity);
    if (dropDown == nullptr || dropDown->options.empty()) return;

    // 找到窗口根节点，弹出层挂在其下以获得正确的绝对坐标空间
    const entt::entity windowRoot = FindWindowRoot(ddEntity);
    if (windowRoot == entt::null) return;

    // 计算 dropdown 的屏幕绝对矩形
    const Rect ddRect = ui::utils::GetEntityRect(ddEntity);

    constexpr float ITEM_H   = 26.0F;
    constexpr float ITEM_PAD = 6.0F;   // 文字左侧内边距
    const float popupW = ddRect.width();
    const float popupH = ITEM_H * static_cast<float>(dropDown->options.size());

    // 创建弹出面板
    const auto popup = CreateBaseWidget("__dd_popup__");
    auto& popupPos = Registry::Get<components::Position>(popup);
    popupPos.value          = {ddRect.x(), ddRect.y() + ddRect.height()};
    popupPos.positionPolicy = policies::Position::Absolute;

    auto& popupSize = Registry::Get<components::Size>(popup);
    popupSize.sizePolicy = policies::Size::Fixed;
    popupSize.size       = {popupW, popupH};

    auto& popupBg = Registry::Emplace<components::Background>(popup);
    popupBg.color        = Color{0.13F, 0.13F, 0.18F, 0.97F};
    popupBg.borderRadius = {4.0F, 4.0F, 4.0F, 4.0F};
    popupBg.enabled      = policies::Feature::Enabled;

    auto& popupBorder = Registry::Emplace<components::Border>(popup);
    popupBorder.color        = Color{0.35F, 0.35F, 0.50F, 1.0F};
    popupBorder.thickness    = 1.0F;
    popupBorder.borderRadius = {4.0F, 4.0F, 4.0F, 4.0F};
    popupBorder.enabled      = policies::Feature::Enabled;

    Registry::Emplace<components::ZOrderIndex>(popup).value = 1000;

    auto& popupLayout = Registry::Emplace<components::LayoutInfo>(popup);
    popupLayout.direction = policies::LayoutDirection::VERTICAL;
    popupLayout.alignment = policies::Alignment::TOP_LEFT;

    // 为每个选项创建按钮
    const int optCount = static_cast<int>(dropDown->options.size());
    for (int idx = 0; idx < optCount; ++idx)
    {
        const std::string optText = dropDown->options[static_cast<std::size_t>(idx)];
        const bool isSelected     = (idx == dropDown->selectedIndex);

        const auto optBtn = CreateBaseWidget("");
        Registry::Emplace<components::Clickable>(optBtn);

        auto& btnText = Registry::Emplace<components::Text>(optBtn);
        btnText.content   = optText;
        btnText.alignment = policies::Alignment::LEFT | policies::Alignment::VCENTER;

        auto& btnSize = Registry::Get<components::Size>(optBtn);
        btnSize.sizePolicy = policies::Size::Fixed;
        btnSize.size       = {popupW, ITEM_H};

        auto& btnPad = Registry::GetOrEmplace<components::Padding>(optBtn);
        btnPad.values = {0.0F, 0.0F, 0.0F, ITEM_PAD};

        // 高亮当前选中项
        if (isSelected)
        {
            auto& selBg = Registry::Emplace<components::Background>(optBtn);
            selBg.color   = Color{0.25F, 0.45F, 0.80F, 0.40F};
            selBg.enabled = policies::Feature::Enabled;
        }
        Registry::Emplace<components::Hoverable>(optBtn);

        // 点击选项：更新选中值、关闭弹出层
        Registry::Get<components::Clickable>(optBtn).onClick = [ddEntity, idx]()
        {
            auto* ddComp = Registry::TryGet<components::DropDown>(ddEntity);
            if (ddComp == nullptr) return;
            ddComp->selectedIndex = idx;
            if (auto* textComp = Registry::TryGet<components::Text>(ddEntity))
            {
                textComp->content = ddComp->selectedText();
            }
            if (ddComp->onChanged)
            {
                ddComp->onChanged(idx);
            }
            ui::utils::MarkVisualChanged(ddEntity);
            CloseDropDownPopup(ddEntity);
        };

        hierarchy::AddChild(popup, optBtn);
    }

    hierarchy::AddChild(windowRoot, popup);
    dropDown->popupEntity = popup;
    dropDown->open        = true;
    ui::utils::MarkLayoutAndVisualChanged(windowRoot);
}

} // namespace (DropDown helpers)
// NOLINTEND(readability-*,misc-*)

entt::entity CreateDropDown(const std::vector<std::string>& options, int selectedIndex, std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    Registry::Emplace<components::DropDownTag>(entity);
    auto& dropDown = Registry::Emplace<components::DropDown>(entity);
    dropDown.options       = options;
    dropDown.selectedIndex = selectedIndex;
    // 显示当前选中项文字
    auto& text = Registry::Emplace<components::Text>(entity);
    text.content   = dropDown.selectedText();
    text.alignment = policies::Alignment::LEFT | policies::Alignment::VCENTER;
    auto& padding = Registry::GetOrEmplace<components::Padding>(entity);
    padding.values = {0.0F, 20.0F, 0.0F, 6.0F}; // 右侧为箭头留 20px
    auto& clickable = Registry::Emplace<components::Clickable>(entity);
    clickable.onClick = [entity]()
    {
        auto* ddComp = Registry::TryGet<components::DropDown>(entity);
        if (ddComp == nullptr) return;
        if (ddComp->open)
        {
            CloseDropDownPopup(entity);
        }
        else
        {
            OpenDropDownPopup(entity);
        }
    };
    auto& size = Registry::Get<components::Size>(entity);
    size.sizePolicy = policies::Size::Auto;
    size.size       = {140.0F, 26.0F};
    return entity;
}

entt::entity CreateSlider(std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    Registry::Emplace<components::SliderInfo>(entity);
    // 默认尺寸：横向滑块高度 28
    auto& size = Registry::Get<components::Size>(entity);
    size.size = {200.0F, 28.0F};
    size.sizePolicy = policies::Size::Fixed;
    Registry::Emplace<components::LayoutInfo>(entity);
    utils::MarkLayoutAndVisualChanged(entity);
    Registry::Emplace<components::SliderTag>(entity);
    return entity;
}

entt::entity CreateProgressBar(std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    Registry::Emplace<components::ProgressBar>(entity);
    auto& size = Registry::Get<components::Size>(entity);
    size.size = {200.0F, 14.0F};
    size.sizePolicy = policies::Size::Fixed;
    Registry::Emplace<components::LayoutInfo>(entity);
    utils::MarkLayoutAndVisualChanged(entity);
    Registry::Emplace<components::ProgressBarTag>(entity);
    return entity;
}

entt::entity CreateImageFromPath(std::string_view path,
                                 float defaultWidth,
                                 float defaultHeight,
                                 std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    Registry::Emplace<components::ImageTag>(entity);
    Registry::Emplace<components::Image>(entity);
    Registry::Emplace<components::ImageSource>(entity, std::string(path));
    auto& size = Registry::Get<components::Size>(entity);
    if (defaultWidth > 0.0F || defaultHeight > 0.0F)
    {
        size.size = {defaultWidth, defaultHeight};
        size.sizePolicy = policies::Size::Fixed;
    }
    Registry::Emplace<components::LayoutInfo>(entity);
    utils::MarkLayoutAndVisualChanged(entity);
    return entity;
}

entt::entity CreateCanvas(float width, float height, std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    Registry::Emplace<components::CanvasTag>(entity);
    Registry::Emplace<components::CanvasDrawList>(entity);
    auto& size = Registry::Get<components::Size>(entity);
    size.size = {width, height};
    size.sizePolicy = policies::Size::Fixed;
    Registry::Emplace<components::LayoutInfo>(entity);
    utils::MarkLayoutAndVisualChanged(entity);
    return entity;
}

entt::entity CreateTable(int columns, std::string_view alias)
{
    auto entity = CreateBaseWidget(alias);
    Registry::Emplace<components::TableTag>(entity);
    auto& info = Registry::Emplace<components::TableInfo>(entity);
    info.columnCount = columns;
    auto& size = Registry::Get<components::Size>(entity);
    size.sizePolicy = policies::Size::FillParent;
    Registry::Emplace<components::LayoutInfo>(entity);
    utils::MarkLayoutAndVisualChanged(entity);
    return entity;
}

} // namespace ui::factory
