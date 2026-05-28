#pragma once

#include <ui.hpp>

#include "Mainwindow.h"

namespace example::ui_demo::view
{
using namespace ui::chains;

/**
 * @brief 创建初始菜单对话框
 */
inline void CreateMenuDialog()
{
    auto view = ui::Registry::View<ui::components::BaseInfo>();
    for (auto entity : view)
    {
        if (view.get<ui::components::BaseInfo>(entity).alias == "menuDialog")
        {
            return; // 已存在，直接返回
        }
    }

    // 创建菜单对话框
    auto menuDialog = ui::factory::CreateDialog("PestManKill Menu", "menuDialog");

    menuDialog | Size(160.0F, 300.0F) | BackgroundColor({0.15F, 0.15F, 0.15F, 0.95F}) | BorderRadius(12.0F)
        | LayoutDirection(ui::policies::LayoutDirection::VERTICAL) | Spacing(15.0F) | Padding(20.0F);

    // 创建标题标签
    auto titleLabel = ui::factory::CreateLabel("欢迎来到 害虫杀", "titleLabel");

    titleLabel | TextAlignment(ui::policies::Alignment::CENTER) | TextColor({1.0F, 0.9F, 0.3F, 1.0F})
        | FontSize(18.0F); // 金黄色

    menuDialog | AddChild(titleLabel);

    // 创建分隔间距
    menuDialog | AddChild(ui::factory::CreateSpacer(1, "spacer1"));

    // 定义通用按钮样式
    auto buttonStyle = FixedSize(150.0F, 40.0F) | TextAlignment(ui::policies::Alignment::CENTER) | BorderRadius(5.0F)
                     | BorderThickness(2.0F);

    // 创建开始按钮
    auto startBtn = ui::factory::CreateButton("开始", "startBtn");

    startBtn | buttonStyle | BackgroundColor({0.2F, 0.4F, 0.8F, 1.0F}) | BorderColor({0.4F, 0.6F, 1.0F, 1.0F})
        | FontSize(14.0F) | BorderRadius(10.0F)
        | OnClick(
            [menuDialog]()
            {
                CreateMainWindow();
                ui::utils::CloseWindow(menuDialog);
            });

    menuDialog | AddChild(startBtn);

    // 创建设置按钮
    auto settingsBtn = ui::factory::CreateButton("设置", "settingsBtn");

    settingsBtn | buttonStyle | TextColor({1.0F, 1.0F, 1.0F, 1.0F}) | BackgroundColor({0.3F, 0.3F, 0.3F, 1.0F})
        | BorderColor({0.5F, 0.5F, 0.5F, 1.0F}) | FontSize(14.0F);

    menuDialog | AddChild(settingsBtn);

    // 创建退出按钮
    auto exitBtn = ui::factory::CreateButton("退出", "exitBtn");

    exitBtn | buttonStyle | BackgroundColor({0.6F, 0.2F, 0.2F, 1.0F}) | BorderColor({0.8F, 0.3F, 0.3F, 1.0F})
        | FontSize(14.0F)
        | OnClick(
            []()
            {
                ui::log::Info("退出menu.");
                ui::utils::QuitUiEventLoop();
            });

    menuDialog | AddChild(exitBtn);

    // 创建底部间距
    menuDialog | AddChild(ui::factory::CreateSpacer(1, "spacer2"));

    // 创建版本信息标签
    auto versionLabel = ui::factory::CreateLabel("v0.1.0 - 2026", "versionLabel");

    versionLabel | TextAlignment(ui::policies::Alignment::CENTER) | TextColor({0.6F, 0.6F, 0.6F, 1.0F})
        | FontSize(12.0F);

    menuDialog | AddChild(versionLabel);

    // 显示菜单对话框
    ui::log::Info("Showing menu dialog...");
    menuDialog | Show();
    ui::log::Info("CreateMenuDialog completed.");
}

} // namespace example::ui_demo::view
