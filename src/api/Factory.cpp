

#include "Factory.hpp"
#include "Scale.hpp"
#include "common/Tags.hpp"
#include "common/Policies.hpp"
#include "common/Types.hpp"
#include "common/Events.hpp"
#include "common/ErrorCodes.hpp"
#include "common/AppConfig.hpp"
#include "core/RuntimeFacade.hpp"
#include "singleton/Logger.hpp"
#include "singleton/Registry.hpp"
#include "singleton/Dispatcher.hpp"
#include "Hierarchy.hpp"
#include "SDL3/SDL_error.h"
#include "Utils.hpp"
#include "Animation.hpp"
#include "core/PlatformWindow.hpp"
#include "entt/entity/fwd.hpp"
#include "common/components/Window.hpp"
#include "common/components/Layout.hpp"
#include "common/components/Visual.hpp"
#include "common/components/Interaction.hpp"
#include "common/components/Data.hpp"
#include "common/Result.hpp"
#include "core/Application.hpp"
#include "entt/entity/entity.hpp"
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_video.h>
#include <SDL3/SDL_surface.h>
#include <stb_image.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <exception>
#include <memory>
#include <string_view>
#include <string>
#include <span>

namespace ui::factory
{

namespace
{
struct RuntimeServices
{
    Registry* registry;
    Dispatcher* dispatcher;
    RuntimeFacade::WindowLookupService windowLookup;
};

RuntimeServices CurrentServices()
{
    auto& runtime = RuntimeFacade::current();
    return {
        .registry = &runtime.registry(), .dispatcher = &runtime.dispatcher(), .windowLookup = runtime.windowLookup()};
}

Registry& CurrentRegistry()
{
    return *CurrentServices().registry;
}

struct TitleBarDragState
{
    Vec2 dragStartMouseGlobal{0.0F, 0.0F};
    Vec2 dragStartWindowPos{0.0F, 0.0F};
    bool dragAnchorValid = false;
};

SDL_Window* CreateSdlWindowOrRollback(
    entt::entity entity, const char* title, int width, int height, SDL_WindowFlags flags, std::string_view entityType)
{
    auto& reg = CurrentRegistry();
    SDL_Window* sdlWindow = SDL_CreateWindow(title, width, height, flags);
    if (sdlWindow == nullptr)
    {
        Logger::error("[Factory] Failed to create SDL window for {} entity {}: {}",
                      entityType,
                      static_cast<uint32_t>(entity),
                      SDL_GetError());
        reg.destroy(entity);
        return nullptr;
    }

    // 应用应用程序图标（若已通过 AppConfig 配置）
    const auto iconPath = ui::config::AppConfig::instance().appIconPath();
    if (!iconPath.empty())
    {
        int wid = 0;
        int hei = 0;
        int channels = 0;
        unsigned char* pixels = stbi_load(std::string(iconPath).c_str(), &wid, &hei, &channels, 4);
        if (pixels != nullptr)
        {
            SDL_Surface* surface = SDL_CreateSurfaceFrom(wid, hei, SDL_PIXELFORMAT_RGBA32, pixels, wid * 4);
            if (surface != nullptr)
            {
                SDL_SetWindowIcon(sdlWindow, surface);
                SDL_DestroySurface(surface);
            }
            else
            {
                Logger::warn("[Factory] Failed to create surface for app icon: {}", SDL_GetError());
            }
            stbi_image_free(pixels);
        }
        else
        {
            Logger::warn("[Factory] Failed to load app icon '{}': {}", iconPath, stbi_failure_reason());
        }
    }

    return sdlWindow;
}

SDL_WindowFlags DefaultWindowFlags()
{
    SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;
    if (ui::config::AppConfig::instance().platformScalingEnabled())
    {
        flags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
    }
    return flags;
}

bool AssignWindowIdOrRollback(entt::entity entity,
                              components::Window& window,
                              SDL_Window* sdlWindow,
                              RuntimeFacade::WindowLookupService windowLookup,
                              std::string_view entityType)
{
    auto& reg = CurrentRegistry();
    window.windowID = SDL_GetWindowID(sdlWindow);
    if (window.windowID == 0)
    {
        Logger::error("[Factory] Failed to fetch SDL window ID for {} entity {}: {}",
                      entityType,
                      static_cast<uint32_t>(entity),
                      SDL_GetError());
        SDL_DestroyWindow(sdlWindow);
        reg.destroy(entity);
        return false;
    }

    windowLookup.remember(entity);

    return true;
}

entt::entity CreateTitleBarContainer(std::string_view alias, float titleBarHeight)
{
    auto& reg = CurrentRegistry();
    auto titleBar = CreateBaseWidget(alias);
    reg.emplace<components::TitleBarTag>(titleBar);

    auto& layout = reg.emplace<components::LayoutInfo>(titleBar);
    layout.direction = policies::LayoutDirection::HORIZONTAL;
    layout.alignment = policies::Alignment::LEFT | policies::Alignment::VCENTER;

    auto& titleBarSize = reg.get<components::Size>(titleBar);
    titleBarSize.size = {0.0F, scale::Metric(titleBarHeight)};
    titleBarSize.sizePolicy = policies::Size::H_FILL | policies::Size::V_FIXED;

    auto& background = reg.emplace<components::Background>(titleBar);
    background.color = {0.0F, 0.0F, 0.0F, 0.0F};
    background.enabled = policies::Feature::ENABLED;

    reg.emplace<components::Clickable>(titleBar);
    auto& draggable = reg.emplace<components::Draggable>(titleBar);
    draggable.lockX = true;
    draggable.lockY = true;

    return titleBar;
}

void ConfigureTitleBarDragging(entt::entity titleBar, entt::entity windowEntity, uint32_t windowId)
{
    auto& reg = CurrentRegistry();
    auto& draggable = reg.get<components::Draggable>(titleBar);
    auto dragState = std::make_shared<TitleBarDragState>();

    draggable.onDragStart = [windowEntity, windowId, dragState]()
    {
        auto& reg = CurrentRegistry();
        SDL_Window* sdlWindow = SDL_GetWindowFromID(windowId);
        auto* position = reg.try_get<components::Position>(windowEntity);
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
        auto& reg = CurrentRegistry();
        SDL_Window* sdlWindow = SDL_GetWindowFromID(windowId);
        if (sdlWindow == nullptr) return;

        auto* position = reg.try_get<components::Position>(windowEntity);
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
    auto& reg = CurrentRegistry();
    auto button = CreateButton("", buttonAlias);
    auto& buttonSizeComp = reg.get<components::Size>(button);
    buttonSizeComp.size = {scale::Metric(buttonSize), scale::Metric(buttonSize)};
    buttonSizeComp.sizePolicy = policies::Size::FIXED;

    auto& buttonBackground = reg.get_or_emplace<components::Background>(button);
    buttonBackground.color = {0.0F, 0.0F, 0.0F, 0.0F};
    const float buttonRadius = scale::Metric(4.0F);
    buttonBackground.borderRadius = {buttonRadius, buttonRadius, buttonRadius, buttonRadius};
    buttonBackground.enabled = policies::Feature::ENABLED;

    auto& iconComp = reg.emplace<components::Icon>(button);
    iconComp.codepoint = iconCodepoint;
    iconComp.size = {scale::Metric(iconSize), scale::Metric(iconSize)};
    iconComp.spacing = scale::Metric(iconSpacing);
    iconComp.tintColor = {0.85F, 0.85F, 0.85F, 1.0F};

    return button;
}

void AppendChild(entt::entity parent, entt::entity child)
{
    auto& reg = CurrentRegistry();
    auto& parentHierarchy = reg.get<components::Hierarchy>(parent);
    auto& childHierarchy = reg.get<components::Hierarchy>(child);
    childHierarchy.parent = parent;
    reg.remove<components::RootTag>(child);
    parentHierarchy.children.push_back(child);
}

void AttachTitleBarToWindow(entt::entity titleBar, entt::entity windowEntity)
{
    auto& reg = CurrentRegistry();
    auto& windowHierarchy = reg.get<components::Hierarchy>(windowEntity);
    auto& titleBarHierarchy = reg.get<components::Hierarchy>(titleBar);
    titleBarHierarchy.parent = windowEntity;
    reg.remove<components::RootTag>(titleBar);
    windowHierarchy.children.insert(windowHierarchy.children.begin(), titleBar);
}

void MarkAsRoot(entt::entity entity)
{
    CurrentRegistry().emplace_or_replace<components::RootTag>(entity);
}
} // namespace

ui::Result<std::unique_ptr<Application>> CreateApplication(std::span<char*> argv)
{
    try
    {
        return std::make_unique<Application>(argv);
    }
    catch (const std::exception& e)
    {
        Logger::error("[Factory] UI initialization failed: {}", e.what());
        return ui::MakeError(UiErrc::DEVICE_UNAVAILABLE);
    }
    catch (...)
    {
        Logger::error("[Factory] Unknown UI initialization failure");
        return ui::MakeError(UiErrc::UNKNOWN);
    }
}

entt::entity CreateBaseWidget(std::string_view alias)
{
    auto& reg = CurrentRegistry();
    auto entity = reg.create();

    auto& baseInfo = reg.emplace<components::BaseInfo>(entity);
    baseInfo.alias = std::string(alias);

    reg.emplace<components::Position>(entity);
    reg.emplace<components::Size>(entity);
    reg.emplace<components::Alpha>(entity);
    reg.emplace<components::VisibleTag>(entity);
    reg.emplace<components::Hierarchy>(entity);

    utils::MarkLayoutAndVisualChanged(entity);

    return entity;
}

void CreateFadeInAnimation(entt::entity entity, float duration)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    auto& alpha = reg.get_or_emplace<components::Alpha>(entity);
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
    auto& reg = CurrentRegistry();
    auto entity = CreateBaseWidget(alias);
    reg.emplace<components::ButtonTag>(entity);
    reg.emplace<components::Clickable>(entity);
    auto& text = reg.emplace<components::Text>(entity);
    text.content = content;
    text.alignment = policies::Alignment::CENTER;
    text.fontSize = 0.0F;
    reg.get<components::Size>(entity).sizePolicy = policies::Size::AUTO;
    return entity;
}

entt::entity CreateLabel(const std::string& content, std::string_view alias)
{
    auto& reg = CurrentRegistry();
    auto entity = CreateBaseWidget(alias);
    reg.emplace<components::LabelTag>(entity);
    auto& text = reg.emplace<components::Text>(entity);
    text.content = content;
    reg.get<components::Size>(entity).sizePolicy = policies::Size::AUTO;
    return entity;
}

entt::entity CreateTextEdit(const std::string& placeholder, bool multiline, std::string_view alias)
{
    auto& reg = CurrentRegistry();
    auto entity = CreateBaseWidget(alias);

    auto& textEdit = reg.emplace<components::TextEdit>(entity);
    textEdit.placeholder = placeholder;
    textEdit.inputMode =
        multiline ? (policies::TextFlag::DEFAULT | policies::TextFlag::MULTILINE) : policies::TextFlag::DEFAULT;
    textEdit.cursorPosition = 0;
    textEdit.selectionStart = 0;
    textEdit.selectionEnd = 0;
    textEdit.hasSelection = false;

    auto& text = reg.emplace<components::Text>(entity);
    text.content = "";
    reg.emplace<components::Clickable>(entity);
    reg.get<components::Size>(entity).minSize = {scale::Metric(100.0F), scale::Metric(multiline ? 80.0F : 30.0F)};
    reg.emplace<components::TextEditTag>(entity);

    // Add Caret component for cursor rendering
    reg.emplace<components::Caret>(entity);

    return entity;
}

entt::entity CreateImage(void* textureId, float defaultWidth, float defaultHeight, std::string_view alias)
{
    auto& reg = CurrentRegistry();
    auto entity = CreateBaseWidget(alias);
    reg.emplace<components::ImageTag>(entity);
    auto& image = reg.emplace<components::Image>(entity);
    image.textureId = textureId;
    auto& size = reg.get<components::Size>(entity);
    size.size = {scale::Metric(defaultWidth), scale::Metric(defaultHeight)};
    return entity;
}

entt::entity CreateArrow(const Vec2& start, const Vec2& end, std::string_view alias)
{
    auto& reg = CurrentRegistry();
    auto entity = CreateBaseWidget(alias);
    reg.emplace<components::ArrowTag>(entity);
    auto& arrow = reg.emplace<components::Arrow>(entity);
    arrow.startPoint = scale::Metric(start);
    arrow.endPoint = scale::Metric(end);
    auto& size = reg.get<components::Size>(entity);
    size.sizePolicy = policies::Size::AUTO;
    return entity;
}

entt::entity CreateSpacer(int stretchFactor, std::string_view alias)
{
    auto& reg = CurrentRegistry();
    auto entity = reg.create();
    auto& baseInfo = reg.emplace<components::BaseInfo>(entity);
    baseInfo.alias = alias;
    reg.emplace<components::SpacerTag>(entity);
    reg.emplace<components::Hierarchy>(entity);
    reg.emplace<components::Position>(entity);

    auto& size = reg.emplace<components::Size>(entity);
    size.size = {0.0F, 0.0F};
    size.sizePolicy = policies::Size::AUTO;

    auto& spacer = reg.emplace<components::Spacer>(entity);
    spacer.stretchFactor = static_cast<uint8_t>(std::max(1, stretchFactor));

    utils::MarkLayoutAndVisualChanged(entity);
    return entity;
}

entt::entity CreateSpacer(float width, float height, std::string_view alias)
{
    auto& reg = CurrentRegistry();
    auto entity = CreateBaseWidget(alias);
    auto& size = reg.get<components::Size>(entity);
    size.size = {scale::Metric(width), scale::Metric(height)};
    size.sizePolicy = policies::Size::FIXED;
    return entity;
}

entt::entity CreateDialog(std::string_view title, std::string_view alias)
{
    const auto services = CurrentServices();
    auto& reg = *services.registry;
    auto& disp = *services.dispatcher;
    const auto windowLookup = services.windowLookup;
    auto entity = CreateBaseWidget(alias);
    MarkAsRoot(entity);
    reg.emplace<components::DialogTag>(entity);
    auto& size = reg.get<components::Size>(entity);
    size.sizePolicy = policies::Size::FIXED;
    auto& dialog = reg.emplace<components::Window>(entity);
    dialog.title = std::string(title);
    dialog.flags |= policies::WindowFlag::NO_TITLE_BAR;
    constexpr int DEFAULT_DIALOG_WIDTH = 400;
    constexpr int DEFAULT_DIALOG_HEIGHT = 300;
    SDL_Window* sdlWindow = CreateSdlWindowOrRollback(
        entity, dialog.title.c_str(), DEFAULT_DIALOG_WIDTH, DEFAULT_DIALOG_HEIGHT, DefaultWindowFlags(), "dialog");
    if (sdlWindow == nullptr)
    {
        return entt::null;
    }

    if (!AssignWindowIdOrRollback(entity, dialog, sdlWindow, windowLookup, "dialog"))
    {
        return entt::null;
    }

    platform::InitCustomWindow(sdlWindow);
    reg.remove<components::VisibleTag>(entity);
    auto& dialogLayout = reg.emplace<components::LayoutInfo>(entity);
    dialogLayout.direction = policies::LayoutDirection::VERTICAL;
    dialogLayout.alignment = policies::Alignment::CENTER;
    reg.emplace<components::Padding>(entity);
    utils::MarkLayoutAndVisualChanged(entity);
    Logger::info("[Factory] Triggering WindowGraphicsContextSetEvent for dialog entity {}",
                 static_cast<uint32_t>(entity));
    disp.trigger<events::WindowGraphicsContextSetEvent>({entity});

    // 自定义 Dialog 默认无标题栏（NO_TITLE_BAR），如需自绘标题栏请显式调用 CreateTitleBar。

    return entity;
}

entt::entity CreateScrollArea(std::string_view alias)
{
    auto& reg = CurrentRegistry();
    auto entity = CreateBaseWidget(alias);
    reg.emplace<components::ScrollArea>(entity);
    auto& layout = reg.emplace<components::LayoutInfo>(entity);
    layout.direction = policies::LayoutDirection::VERTICAL;
    layout.alignment = policies::Alignment::TOP_LEFT;
    auto& size = reg.get<components::Size>(entity);
    size.sizePolicy = policies::Size::FILL_PARENT;
    return entity;
}

entt::entity CreateWindow(std::string_view title, std::string_view alias)
{
    const auto services = CurrentServices();
    auto& reg = *services.registry;
    auto& disp = *services.dispatcher;
    const auto windowLookup = services.windowLookup;
    auto entity = CreateBaseWidget(alias);
    MarkAsRoot(entity);
    reg.emplace<components::WindowTag>(entity);
    auto& window = reg.emplace<components::Window>(entity);
    window.title = std::string(title);
    window.flags &= ~policies::WindowFlag::MODAL;
    auto& size = reg.get<components::Size>(entity);
    size.sizePolicy = policies::Size::FIXED;
    auto& layoutInfo = reg.emplace<components::LayoutInfo>(entity);
    layoutInfo.direction = policies::LayoutDirection::VERTICAL;
    layoutInfo.alignment = policies::Alignment::CENTER;
    reg.emplace<components::Padding>(entity);
    ui::utils::MarkLayoutAndVisualChanged(entity);
    constexpr int DEFAULT_WINDOW_WIDTH = 800;
    constexpr int DEFAULT_WINDOW_HEIGHT = 600;
    SDL_Window* sdlWindow = CreateSdlWindowOrRollback(
        entity, window.title.c_str(), DEFAULT_WINDOW_WIDTH, DEFAULT_WINDOW_HEIGHT, DefaultWindowFlags(), "window");
    if (sdlWindow == nullptr)
    {
        return entt::null;
    }

    if (!AssignWindowIdOrRollback(entity, window, sdlWindow, windowLookup, "window"))
    {
        return entt::null;
    }

    Logger::info("[Factory] Triggering WindowGraphicsContextSetEvent for window entity {}",
                 static_cast<uint32_t>(entity));
    disp.trigger<events::WindowGraphicsContextSetEvent>({entity});
    reg.remove<components::VisibleTag>(entity);

    return entity;
}

entt::entity CreateTitleBar(entt::entity windowEntity, std::string_view alias)
{
    auto& reg = CurrentRegistry();
    auto* windowComp = reg.try_get<components::Window>(windowEntity);
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
    uint32_t const windowID = windowComp->windowID;
    ConfigureTitleBarDragging(titleBar, windowEntity, windowID);

    auto titleLabel = CreateLabel(windowComp->title, std::string(alias) + "_title");
    auto& titleText = reg.get<components::Text>(titleLabel);
    titleText.fontSize = scale::Metric(13.0F);
    titleText.alignment = policies::Alignment::LEFT | policies::Alignment::VCENTER;

    auto spacer = CreateSpacer(1, std::string(alias) + "_spacer");

    auto minimizeBtn =
        CreateWindowControlButton(std::string(alias) + "_minimize", ICON_MINIMIZE, BTN_SIZE, ICON_SIZE, ICON_SPACING);
    reg.get<components::Clickable>(minimizeBtn).onClick = [windowID]()
    {
        SDL_Window* sdlWin = SDL_GetWindowFromID(windowID);
        if (sdlWin != nullptr) SDL_MinimizeWindow(sdlWin);
    };

    auto maximizeBtn =
        CreateWindowControlButton(std::string(alias) + "_maximize", ICON_MAXIMIZE, BTN_SIZE, ICON_SIZE, ICON_SPACING);
    reg.get<components::Clickable>(maximizeBtn).onClick = [windowID]()
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
    Registry* const regPtr = &reg;
    auto& closeBtnHover = reg.emplace<components::Hoverable>(closeBtn);
    closeBtnHover.onHover = [regPtr, closeBtn]()
    {
        auto& reg = *regPtr;
        auto* closeBg = reg.try_get<components::Background>(closeBtn);
        if (closeBg != nullptr) closeBg->color = {0.9F, 0.2F, 0.2F, 1.0F};
    };
    closeBtnHover.onUnhover = [regPtr, closeBtn]()
    {
        auto& reg = *regPtr;
        auto* closeBg = reg.try_get<components::Background>(closeBtn);
        if (closeBg != nullptr) closeBg->color = {0.0F, 0.0F, 0.0F, 0.0F};
    };
    reg.get<components::Clickable>(closeBtn).onClick = [windowEntity]() { utils::CloseWindow(windowEntity); };

    AppendChild(titleBar, titleLabel);
    AppendChild(titleBar, spacer);
    AppendChild(titleBar, minimizeBtn);
    AppendChild(titleBar, maximizeBtn);
    AppendChild(titleBar, closeBtn);
    AttachTitleBarToWindow(titleBar, windowEntity);

    utils::MarkLayoutAndVisualChanged(titleBar);
    utils::MarkLayoutAndVisualChanged(windowEntity);

    auto& padding = reg.emplace<components::Padding>(titleBar);
    padding.values = {0.0F, scale::Metric(BTN_SPACING), 0.0F, scale::Metric(8.0F)};

    auto& layoutInfo = reg.get<components::LayoutInfo>(titleBar);
    layoutInfo.spacing = scale::Metric(BTN_SPACING);

    return titleBar;
}

entt::entity CreateVBoxLayout(std::string_view alias)
{
    auto& reg = CurrentRegistry();
    auto entity = CreateBaseWidget(alias);
    auto& layout = reg.emplace<components::LayoutInfo>(entity);
    layout.direction = policies::LayoutDirection::VERTICAL;
    layout.alignment = policies::Alignment::TOP_LEFT;
    auto& size = reg.get<components::Size>(entity);
    size.sizePolicy = policies::Size::AUTO;
    reg.emplace<components::Padding>(entity);
    return entity;
}

entt::entity CreateHBoxLayout(std::string_view alias)
{
    auto& reg = CurrentRegistry();
    auto entity = CreateBaseWidget(alias);
    auto& layout = reg.emplace<components::LayoutInfo>(entity);
    layout.direction = policies::LayoutDirection::HORIZONTAL;
    layout.alignment = policies::Alignment::LEFT | policies::Alignment::VCENTER;
    auto& size = reg.get<components::Size>(entity);
    size.sizePolicy = policies::Size::AUTO;
    reg.emplace<components::Padding>(entity);
    return entity;
}

entt::entity CreateLineEdit(std::string_view initialText, std::string_view placeholder, std::string_view alias)
{
    auto& reg = CurrentRegistry();
    auto entity = CreateTextEdit(std::string(placeholder), false, alias);
    auto& edit = reg.get<components::TextEdit>(entity);
    edit.buffer = std::string(initialText);
    edit.cursorPosition = edit.buffer.size(); // Place cursor at end
    auto& text = reg.get<components::Text>(entity);
    text.content = edit.buffer;
    return entity;
}

entt::entity CreateTextBrowser(std::string_view initialText, std::string_view placeholder, std::string_view alias)
{
    auto& reg = CurrentRegistry();
    auto entity = CreateTextEdit(std::string(placeholder), true, alias);
    auto& edit = reg.get<components::TextEdit>(entity);
    edit.buffer = std::string(initialText);
    edit.cursorPosition = 0; // Start at beginning for read-only
    edit.inputMode = policies::TextFlag::READ_ONLY_MULTILINE;
    auto& text = reg.get<components::Text>(entity);
    text.content = edit.buffer;

    auto& scrollArea = reg.emplace<components::ScrollArea>(entity);
    scrollArea.scroll = policies::Scroll::VERTICAL;
    scrollArea.scrollBar = policies::ScrollBar::DRAGGABLE | policies::ScrollBar::AUTO_HIDE;
    scrollArea.anchor = policies::ScrollAnchor::SMART;

    text.alignment = policies::Alignment::TOP | policies::Alignment::LEFT;
    text.wordWrap = policies::TextWrap::WORD;

    auto& size = reg.get<components::Size>(entity);
    size.sizePolicy = policies::Size::FILL_PARENT;

    return entity;
}

entt::entity CreateCheckBox(const std::string& label, bool checked, std::string_view alias)
{
    auto& reg = CurrentRegistry();
    auto entity = CreateBaseWidget(alias);
    reg.emplace<components::CheckBoxTag>(entity);
    auto& checkBox = reg.emplace<components::CheckBox>(entity);
    checkBox.checked = checked;
    checkBox.label = label;
    auto& text = reg.emplace<components::Text>(entity);
    text.content = label;
    text.alignment = policies::Alignment::LEFT | policies::Alignment::VCENTER;
    auto& padding = reg.get_or_emplace<components::Padding>(entity);
    padding.values = {0.0F, 0.0F, 0.0F, scale::Metric(24.0F)}; // Top, Right, Bottom, Left
    auto& size = reg.get<components::Size>(entity);
    size.sizePolicy = policies::Size::AUTO;
    size.size = {scale::Metric(120.0F), scale::Metric(22.0F)};
    auto& clickable = reg.emplace<components::Clickable>(entity);
    Registry* const regPtr = &reg;
    clickable.onClick = [regPtr, entity]()
    {
        auto& reg = *regPtr;
        auto* checkBoxComp = reg.try_get<components::CheckBox>(entity);
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

namespace
{

entt::entity FindWindowRoot(Registry& reg, entt::entity entity)
{
    entt::entity current = entity;
    while (current != entt::null && reg.valid(current))
    {
        if (reg.any_of<components::WindowTag>(current))
        {
            return current;
        }
        const auto* hier = reg.try_get<components::Hierarchy>(current);
        current = (hier != nullptr) ? hier->parent : entt::null;
    }
    return entt::null;
}

} // namespace

void CloseDropDownPopup(entt::entity ddEntity)
{
    auto& reg = CurrentRegistry();
    auto* dropDown = reg.try_get<components::DropDown>(ddEntity);
    if (dropDown == nullptr) return;
    if (dropDown->popupEntity == entt::null || !reg.valid(dropDown->popupEntity))
    {
        dropDown->open = false;
        return;
    }

    const entt::entity popupToDestroy = dropDown->popupEntity;
    dropDown->popupEntity = entt::null;
    dropDown->open = false;
    ui::utils::MarkVisualChanged(ddEntity);

    ui::utils::InvokeTask(
        [regPtr = &reg, popupToDestroy]()
        {
            auto& reg = *regPtr;
            if (!reg.valid(popupToDestroy)) return;

            const auto* popupHier = reg.try_get<components::Hierarchy>(popupToDestroy);
            if (popupHier != nullptr && popupHier->parent != entt::null)
            {
                hierarchy::RemoveChild(popupHier->parent, popupToDestroy);
            }

            std::vector<entt::entity> toDestroy;
            std::vector<entt::entity> stack{popupToDestroy};
            while (!stack.empty())
            {
                const entt::entity cur = stack.back();
                stack.pop_back();
                if (!reg.valid(cur)) continue;
                toDestroy.push_back(cur);
                if (const auto* hier = reg.try_get<components::Hierarchy>(cur))
                {
                    for (const entt::entity child : hier->children)
                    {
                        stack.push_back(child);
                    }
                }
            }
            for (entt::entity ent : std::ranges::reverse_view(toDestroy))
            {
                if (reg.valid(ent))
                {
                    reg.destroy(ent);
                }
            }
        });
}

namespace
{
void OpenDropDownPopup(entt::entity ddEntity)
{
    auto& reg = CurrentRegistry();
    auto* dropDown = reg.try_get<components::DropDown>(ddEntity);
    if (dropDown == nullptr || dropDown->options.empty()) return;

    const entt::entity windowRoot = FindWindowRoot(reg, ddEntity);
    if (windowRoot == entt::null) return;
    // 计算下拉菜单弹出位置和大小
    const Rect ddRect = ui::utils::GetEntityRect(ddEntity);

    constexpr float ITEM_H = 26.0F;
    constexpr float ITEM_PAD = 6.0F;
    const float popupW = ddRect.width();
    const float popupH = ITEM_H * static_cast<float>(dropDown->options.size());

    const auto popup = CreateBaseWidget("__dd_popup__");
    auto& popupPos = reg.get<components::Position>(popup);
    popupPos.value = {ddRect.x(), ddRect.y() + ddRect.height()};
    popupPos.positionPolicy = policies::Position::ABSOLUTE_POS;

    auto& popupSize = reg.get<components::Size>(popup);
    popupSize.sizePolicy = policies::Size::FIXED;
    popupSize.size = {scale::Metric(popupW), scale::Metric(popupH)};

    reg.emplace<components::DropDownPopupPanel>(popup).owner = ddEntity;

    reg.emplace<components::ZOrderIndex>(popup).value = 1000;

    auto& popupLayout = reg.emplace<components::LayoutInfo>(popup);
    popupLayout.direction = policies::LayoutDirection::VERTICAL;
    popupLayout.alignment = policies::Alignment::TOP_LEFT;

    const int optCount = static_cast<int>(dropDown->options.size());
    for (int idx = 0; idx < optCount; ++idx)
    {
        const std::string& optText = dropDown->options.at(static_cast<std::size_t>(idx));

        const auto optBtn = CreateBaseWidget("__dd_option__");
        reg.emplace<components::Clickable>(optBtn);
        auto& popupItem = reg.emplace<components::DropDownPopupItem>(optBtn);
        popupItem.owner = ddEntity;
        popupItem.optionIndex = idx;

        auto& btnText = reg.emplace<components::Text>(optBtn);
        btnText.content = optText;
        btnText.alignment = policies::Alignment::LEFT | policies::Alignment::VCENTER;

        auto& btnSize = reg.get<components::Size>(optBtn);
        btnSize.sizePolicy = policies::Size::FIXED;
        btnSize.size = {scale::Metric(popupW), scale::Metric(ITEM_H)};

        auto& btnPad = reg.get_or_emplace<components::Padding>(optBtn);
        btnPad.values = {0.0F, 0.0F, 0.0F, ITEM_PAD};

        reg.emplace<components::Hoverable>(optBtn);

        Registry* const regPtr = &reg;
        reg.get<components::Clickable>(optBtn).onClick = [regPtr, ddEntity, idx]()
        {
            auto& reg = *regPtr;
            auto* ddComp = reg.try_get<components::DropDown>(ddEntity);
            if (ddComp == nullptr) return;
            ddComp->selectedIndex = idx;
            if (auto* textComp = reg.try_get<components::Text>(ddEntity))
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
    dropDown->open = true;
    ui::utils::MarkLayoutAndVisualChanged(windowRoot);
}

} // namespace

entt::entity CreateDropDown(const std::vector<std::string>& options, int selectedIndex, std::string_view alias)
{
    auto& reg = CurrentRegistry();
    auto entity = CreateBaseWidget(alias);
    reg.emplace<components::DropDownTag>(entity);
    auto& dropDown = reg.emplace<components::DropDown>(entity);
    dropDown.options = options;
    dropDown.selectedIndex = selectedIndex;
    auto& text = reg.emplace<components::Text>(entity);
    text.content = dropDown.selectedText();
    text.alignment = policies::Alignment::LEFT | policies::Alignment::VCENTER;
    auto& padding = reg.get_or_emplace<components::Padding>(entity);
    padding.values = {0.0F, scale::Metric(20.0F), 0.0F, scale::Metric(6.0F)};
    auto& clickable = reg.emplace<components::Clickable>(entity);
    clickable.onClick = [entity]()
    {
        auto* ddComp = CurrentRegistry().try_get<components::DropDown>(entity);
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
    auto& size = reg.get<components::Size>(entity);
    size.sizePolicy = policies::Size::AUTO;
    size.size = {scale::Metric(140.0F), scale::Metric(26.0F)};
    return entity;
}

entt::entity CreateSlider(std::string_view alias)
{
    auto& reg = CurrentRegistry();
    auto entity = CreateBaseWidget(alias);
    reg.emplace<components::SliderInfo>(entity);
    auto& size = reg.get<components::Size>(entity);
    size.size = {scale::Metric(200.0F), scale::Metric(28.0F)};
    size.sizePolicy = policies::Size::FIXED;
    reg.emplace<components::LayoutInfo>(entity);
    utils::MarkLayoutAndVisualChanged(entity);
    reg.emplace<components::SliderTag>(entity);
    return entity;
}

entt::entity CreateProgressBar(std::string_view alias)
{
    auto& reg = CurrentRegistry();
    auto entity = CreateBaseWidget(alias);
    reg.emplace<components::ProgressBar>(entity);
    auto& size = reg.get<components::Size>(entity);
    size.size = {scale::Metric(200.0F), scale::Metric(14.0F)};
    size.sizePolicy = policies::Size::FIXED;
    reg.emplace<components::LayoutInfo>(entity);
    utils::MarkLayoutAndVisualChanged(entity);
    reg.emplace<components::ProgressBarTag>(entity);
    return entity;
}

entt::entity CreateImageFromPath(std::string_view path, float defaultWidth, float defaultHeight, std::string_view alias)
{
    auto& reg = CurrentRegistry();
    auto entity = CreateBaseWidget(alias);
    reg.emplace<components::ImageTag>(entity);
    reg.emplace<components::Image>(entity);
    reg.emplace<components::ImageSource>(entity, std::string(path));
    auto& size = reg.get<components::Size>(entity);
    if (defaultWidth > 0.0F || defaultHeight > 0.0F)
    {
        size.size = {scale::Metric(defaultWidth), scale::Metric(defaultHeight)};
        size.sizePolicy = policies::Size::FIXED;
    }
    reg.emplace<components::LayoutInfo>(entity);
    utils::MarkLayoutAndVisualChanged(entity);
    return entity;
}

entt::entity CreateCanvas(float width, float height, std::string_view alias)
{
    auto& reg = CurrentRegistry();
    auto entity = CreateBaseWidget(alias);
    reg.emplace<components::CanvasTag>(entity);
    reg.emplace<components::CanvasDrawList>(entity);
    auto& size = reg.get<components::Size>(entity);
    size.size = {scale::Metric(width), scale::Metric(height)};
    size.sizePolicy = policies::Size::FIXED;
    reg.emplace<components::LayoutInfo>(entity);
    utils::MarkLayoutAndVisualChanged(entity);
    return entity;
}

entt::entity CreateTable(int columns, std::string_view alias)
{
    auto& reg = CurrentRegistry();
    auto entity = CreateBaseWidget(alias);
    reg.emplace<components::TableTag>(entity);
    auto& info = reg.emplace<components::TableInfo>(entity);
    info.columnCount = columns;
    info.rowHeight = scale::Metric(info.rowHeight);
    info.headerHeight = scale::Metric(info.headerHeight);
    auto& size = reg.get<components::Size>(entity);
    size.sizePolicy = policies::Size::FILL_PARENT;

    auto& scrollArea = reg.emplace<components::ScrollArea>(entity);
    scrollArea.scroll = policies::Scroll::VERTICAL;
    scrollArea.scrollBar = policies::ScrollBar::DRAGGABLE | policies::ScrollBar::AUTO_HIDE;

    auto& padding = reg.emplace<components::Padding>(entity);
    padding.values.x() = info.headerHeight;

    reg.emplace<components::LayoutInfo>(entity);
    utils::MarkLayoutAndVisualChanged(entity);
    return entity;
}

} // namespace ui::factory
