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

#include <string>

#include "../../../src/ui/ui/ui.hpp"

#include "Logging.hpp"

namespace example::ui_demo::view
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

    // ===========================================
    // 控件测试区域
    // ===========================================
    auto controlsTestPanel = ui::factory::CreateVBoxLayout("controlsTestPanel");
    controlsTestPanel | BackgroundColor({0.06F, 0.06F, 0.09F, 0.85F}) | BorderRadius(6.0F) |
        BorderColor({0.25F, 0.25F, 0.32F, 0.9F}) | BorderThickness(1.0F) |
        SizePolicy(ui::policies::Size::HFill | ui::policies::Size::VFixed) | Size(0.0F, 360.0F) | Padding(10.0F) |
        Spacing(8.0F);

    auto testTitle = ui::factory::CreateLabel("UI 控件测试区", "testTitle");
    testTitle | TextColor({1.0F, 0.9F, 0.6F, 1.0F}) | FontSize(18.0F) |
        TextAlignment(ui::policies::Alignment::LEFT | ui::policies::Alignment::VCENTER);
    controlsTestPanel | AddChild(testTitle);

    auto buttonRow = ui::factory::CreateHBoxLayout("buttonRow");
    buttonRow | Spacing(8.0F) | SizePolicy(ui::policies::Size::HFill | ui::policies::Size::VFixed) | Size(0.0F, 36.0F);

    auto testButton = ui::factory::CreateButton("测试按钮", "testButton");
    testButton | FixedSize(120.0F, 32.0F) | BackgroundColor({0.2F, 0.5F, 0.85F, 1.0F}) | BorderRadius(4.0F) |
        BorderColor({0.3F, 0.65F, 1.0F, 1.0F}) | BorderThickness(1.0F) | OnClick([]() { ::example::ui_demo::LogInfo("测试按钮点击"); });

    auto testCheckBox = ui::factory::CreateCheckBox("复选框(占位)", false, "testCheckBox");
    testCheckBox | SizePolicy(ui::policies::Size::HFill | ui::policies::Size::VFill) |
        BackgroundColor({0.14F, 0.14F, 0.17F, 0.9F}) | BorderRadius(4.0F) | BorderColor({0.3F, 0.3F, 0.35F, 1.0F}) |
        BorderThickness(1.0F) | Padding(8.0F) |
        TextAlignment(ui::policies::Alignment::LEFT | ui::policies::Alignment::VCENTER);

    buttonRow | AddChild(testButton) | AddChild(testCheckBox);
    controlsTestPanel | AddChild(buttonRow);

    auto testLineEdit = ui::factory::CreateLineEdit("", "输入测试文本...", "testLineEdit");
    testLineEdit | SizePolicy(ui::policies::Size::HFill | ui::policies::Size::VFixed) | Size(0.0F, 34.0F) |
        BackgroundColor({0.15F, 0.15F, 0.18F, 0.95F}) | BorderRadius(4.0F) | BorderColor({0.35F, 0.35F, 0.4F, 1.0F}) |
        BorderThickness(1.0F) | Padding(8.0F) | FontSize(13.0F);
    controlsTestPanel | AddChild(testLineEdit);

    auto progressBar = ui::factory::CreateProgressBar("testProgress");
    progressBar | SizePolicy(ui::policies::Size::HFill | ui::policies::Size::VFixed) | Size(0.0F, 16.0F) |
        ProgressValue(0.35F) | ProgressFillColor({0.2F, 0.75F, 0.45F, 1.0F}) |
        ProgressBackgroundColor({0.2F, 0.2F, 0.24F, 1.0F}) | ProgressLabel(ui::policies::LabelVisibility::Visible);
    controlsTestPanel | AddChild(progressBar);

    auto sliderRow = ui::factory::CreateHBoxLayout("sliderRow");
    sliderRow | Spacing(8.0F) | SizePolicy(ui::policies::Size::HFill | ui::policies::Size::VFixed) | Size(0.0F, 120.0F);

    auto horizontalSlider = ui::factory::CreateSlider("horizontalSlider");
    horizontalSlider | SizePolicy(ui::policies::Size::HFill | ui::policies::Size::VFixed) | Size(0.0F, 28.0F) |
        SliderRange(0.0F, 100.0F) | SliderValue(35.0F) | SliderStep(5.0F) |
        OnSliderValueChanged(
            [progressBar](float value)
            {
                progressBar | ProgressValue(value / 100.0F);
                ::example::ui_demo::LogInfo("水平滑块值: {}", value);
            });

    auto verticalSlider = ui::factory::CreateSlider("verticalSlider");
    verticalSlider | SliderOrientation(ui::policies::Orientation::Vertical) | SliderRange(0.0F, 100.0F) |
        SliderValue(60.0F) | FixedSize(28.0F, 110.0F);

    auto sliderColumn = ui::factory::CreateVBoxLayout("sliderColumn");
    sliderColumn | SizePolicy(ui::policies::Size::HFill | ui::policies::Size::VFill);
    sliderColumn | AddChild(horizontalSlider);

    sliderRow | AddChild(sliderColumn) | AddChild(verticalSlider);
    controlsTestPanel | AddChild(sliderRow);

    auto testScroll = ui::factory::CreateScrollArea("testScroll");
    testScroll | SizePolicy(ui::policies::Size::HFill | ui::policies::Size::VFill) |
        BackgroundColor({0.1F, 0.1F, 0.13F, 0.7F}) | BorderRadius(4.0F) | BorderColor({0.3F, 0.3F, 0.35F, 0.9F}) |
        BorderThickness(1.0F) | Padding(6.0F) | ScrollMode(ui::policies::Scroll::Vertical) |
        ScrollBarPolicy(ui::policies::ScrollBar::Draggable | ui::policies::ScrollBar::AutoHide) |
        ScrollAnchor(ui::policies::ScrollAnchor::Top) | ScrollSpeed(20.0F);

    auto scrollContent = ui::factory::CreateVBoxLayout("scrollContent");
    scrollContent | SizePolicy(ui::policies::Size::HFill | ui::policies::Size::Auto) | Spacing(4.0F);

    for (int i = 1; i <= 12; ++i)
    {
        auto item = ui::factory::CreateLabel("滚动项 " + std::to_string(i), "scrollItem" + std::to_string(i));
        item | SizePolicy(ui::policies::Size::HFill | ui::policies::Size::VFixed) | Size(0.0F, 22.0F) |
            BackgroundColor({0.16F, 0.16F, 0.2F, 0.8F}) | BorderRadius(3.0F) | Padding(6.0F) |
            TextAlignment(ui::policies::Alignment::LEFT | ui::policies::Alignment::VCENTER) | FontSize(12.0F);
        scrollContent | AddChild(item);
    }

    testScroll | AddChild(scrollContent);
    controlsTestPanel | AddChild(testScroll);

    gameWindow | AddChild(controlsTestPanel);

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
                ::example::ui_demo::LogInfo("发送聊天消息: {}", content);

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
                ::example::ui_demo::LogError("Chat Error: {}", e.what());
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

    ::example::ui_demo::LogInfo("主窗口已创建");
}

} // namespace example::ui_demo::view
