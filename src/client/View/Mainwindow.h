/**
 * ************************************************************************
 * 
 * @file Mainwindow.h
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-02-24
 * @version 0.1
 * @brief 主窗口界面定义
 * 
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

#include <ui.hpp>
#include "src/utils/Logger.h"

namespace client::view
{
using namespace ui::chains; // 引入 DSL

/**
 * @brief 创建主窗口（空窗口，点击开始游戏后显示）
 */
inline void CreateMainWindow() // NOLINT
{
    auto gameWindow = ui::factory::CreateWindow("Game", "gameWindow");

    gameWindow | WindowFlag(ui::policies::WindowFlag::Default) | Size(1200.0F, 800.0F) |
        BackgroundColor({0.1F, 0.1F, 0.12F, 1.0F}) | BorderRadius(4.0F) |
        LayoutDirection(ui::policies::LayoutDirection::VERTICAL) | Spacing(10.0F) | Padding(10.0F);

    // 填充空白区域
    auto mainSpacer = ui::factory::CreateSpacer(1, "mainSpacer");
    gameWindow | AddChild(mainSpacer);

    // ===========================================
    // 聊天区域 (左下角游戏风格)
    // ===========================================
    // 1. 聊天总容器 (垂直布局：上面消息，下面输入)
    auto chatContainer = ui::factory::CreateVBoxLayout("chatContainer");

    chatContainer | BackgroundColor({0.05F, 0.05F, 0.08F, 0.8F}) | BorderRadius(4.0F) | FixedSize(500.0F, 250.0F) |
        Spacing(5.0F) | Padding(5.0F);

    // 2. 消息显示区域 (占用大部分垂直空间)
    // 使用 TextBrowser（只读多行）展示消息
    const std::string initialMessages = "[System] Welcome to PestManKill!\n[System] Press Enter to send message.";
    auto messageArea = ui::factory::CreateTextBrowser(initialMessages, "", "messageArea");

    messageArea | SizePolicy(ui::policies::Size::FillParent) | TextContent(initialMessages) |
        TextWordWrap(ui::policies::TextWrap::Char) | TextWrapWidth(490.0F) |
        TextAlignment(ui::policies::Alignment::TOP_LEFT) | Padding(4.0F) | BackgroundColor({0.08F, 0.08F, 0.1F, 0.5F}) |
        BorderRadius(3.0F) | BorderColor({0.3F, 0.3F, 0.35F, 0.8F}) | BorderThickness(1.0F) | FontSize(13.0F);

    chatContainer | AddChild(messageArea);

    // 3. 输入区域 (底部水平排列)
    auto inputRow = ui::factory::CreateHBoxLayout("inputRow");

    inputRow | SizePolicy(ui::policies::Size::HFill | ui::policies::Size::VFixed) | Size(0.0F, 30.0F) | Spacing(5.0F);

    // 输入框 - 填充剩余宽度
    auto chatInput = ui::factory::CreateLineEdit("", "Say something...", "chatInput");

    // 发送按钮 - 固定宽度 (使用回车图标)
    auto sendBtn = ui::factory::CreateButton("", "sendBtn");

    // 发送消息的共享逻辑
    auto sendMessage = [chatInput, messageArea]()
    {
        // 获取输入框内容
        std::string content = ui::text::GetTextEditContent(chatInput);
        if (!content.empty())
        {
            try
            {
                LOG_INFO("发送聊天消息: {}", content);

                // 追加新消息到 TextBrowser
                const std::string fullMsg = "[Me]: " + content;
                std::string currentHistory = ui::text::GetTextEditContent(messageArea);

                if (!currentHistory.empty())
                {
                    currentHistory += "\n";
                }
                currentHistory += fullMsg;

                // 更新显示内容
                ui::text::SetTextEditContent(messageArea, currentHistory);
                ui::text::SetTextContent(messageArea, currentHistory);

                // 清空输入框
                ui::text::SetTextEditContent(chatInput, "");
                ui::text::SetTextContent(chatInput, "");

                // 文本内容变化但尺寸不变，只需标记渲染脏
                ui::utils::MarkRenderDirty(chatInput);
                ui::utils::MarkRenderDirty(messageArea);
            }
            catch (const std::exception& e)
            {
                LOG_ERROR("Chat Error: {}", e.what());
            }
        }
    };

    chatInput | SizePolicy(ui::policies::Size::HFill | ui::policies::Size::VFixed) |
        BackgroundColor({0.15F, 0.15F, 0.18F, 0.9F}) | BorderRadius(3.0F) | BorderColor({0.3F, 0.3F, 0.35F, 1.0F}) |
        BorderThickness(1.0F) | FontSize(13.0F) | OnSubmit(sendMessage);

    sendBtn | Icon("MaterialSymbols", 0xe31b, ui::policies::IconFlag::Default, 20.0F, 0.0F) |
        SizePolicy(ui::policies::Size::HFixed | ui::policies::Size::VFill) | Size(40.0F, 0.0F) |
        BackgroundColor({0.2F, 0.5F, 0.8F, 1.0F}) | BorderRadius(4.0F) | BorderColor({0.3F, 0.6F, 1.0F, 1.0F}) |
        BorderThickness(1.0F) | OnClick(sendMessage);

    inputRow | AddChild(chatInput) | AddChild(sendBtn);

    chatContainer | AddChild(inputRow);

    // 将聊天面板添加到主窗口底部
    gameWindow | AddChild(chatContainer);

    // 显示主窗口（同步尺寸并居中）
    gameWindow | Show();

    LOG_INFO("主窗口已创建");
}

} // namespace client::view
