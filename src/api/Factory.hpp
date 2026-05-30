/**
 * ************************************************************************
 *
 * @file Factory.cpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-03-23
 * @version 0.1
 * @brief 工厂实现 - 提供UI组件和应用程序的创建函数
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once
#include <memory>
#include <vector>
#include "Entity.hpp"
#include <string>
#include <string_view>
#include "core/Application.hpp"
#include "common/Result.hpp"
#ifdef CreateWindow
#undef CreateWindow
#endif
#ifdef CreateWindowA
#undef CreateWindowA
#endif
#ifdef CreateDialog
#undef CreateDialog
#endif
#ifdef CreateDialogA
#undef CreateDialogA
#endif

namespace ui::factory
{
/**
 * @brief 创建 UI 应用程序实例
 * @param argc 命令行参数数量
 * @param argv 命令行参数数组
 * @return UI 应用程序实例
 */
ui::Result<std::unique_ptr<Application>> CreateApplication(std::span<char*> argv);
/**
 * @brief 创建一个基础的 UI 组件实体
 * @param alias 组件别名
 * @return ui::entity 创建的实体
 */
ui::entity CreateBaseWidget(std::string_view alias = "");
/**
 * @brief 为指定实体创建一个淡入动画组件
 * @param entity 目标实体
 * @param duration 动画持续时间（秒）
 */
void CreateFadeInAnimation(ui::entity entity, float duration);

/**
 * @brief 创建一个按钮实体
 * @param content 按钮显示的文本内容
 * @param alias 组件别名
 * @return ui::entity 创建的实体
 */
ui::entity CreateButton(const std::string& content, std::string_view alias = "");
ui::entity CreateLabel(const std::string& content, std::string_view alias = "");
ui::entity CreateTextEdit(const std::string& placeholder = "", bool multiline = false, std::string_view alias = "");
ui::entity
    CreateImage(void* textureId, float defaultWidth = 50.0F, float defaultHeight = 50.0F, std::string_view alias = "");
ui::entity CreateArrow(const Vec2& start, const Vec2& end, std::string_view alias = "");
ui::entity CreateSpacer(int stretchFactor = 1, std::string_view alias = "");
ui::entity CreateSpacer(float width, float height, std::string_view alias = "");
ui::entity CreateDialog(std::string_view title, std::string_view alias = "");
ui::entity CreateScrollArea(std::string_view alias = "");
ui::entity CreateWindow(std::string_view title, std::string_view alias = "");

/**
 * @brief 创建一个标题栏实体并关联到指定窗口
 * @param windowEntity 所属窗口实体（必须已有 Window 组件）
 * @param alias 组件别名
 * @return ui::entity 创建的标题栏实体
 */
ui::entity CreateTitleBar(ui::entity windowEntity, std::string_view alias = "");
ui::entity CreateVBoxLayout(std::string_view alias = "");
ui::entity CreateHBoxLayout(std::string_view alias = "");
ui::entity
    CreateLineEdit(std::string_view initialText = "", std::string_view placeholder = "", std::string_view alias = "");
ui::entity CreateTextBrowser(std::string_view initialText = "",
                             std::string_view placeholder = "",
                             std::string_view alias = "");

/**
 * @brief 创建一个复选框实体
 * @param label 复选框标签文本
 * @param checked 是否默认选中
 * @param alias 组件别名
 * @return ui::entity 创建的实体
 */
ui::entity CreateCheckBox(const std::string& label, bool checked = false, std::string_view alias = "");
ui::entity CreateDropDown(const std::vector<std::string>& options, int selectedIndex = 0, std::string_view alias = "");

/// @brief 关闭 DropDown 弹出层（若已打开），下一帧销毁弹出实体
/// @note StateSystem 在「点击外部」时调用此函数实现失焦自动关闭
void CloseDropDownPopup(ui::entity ddEntity);

// Slider / ProgressBar
ui::entity CreateSlider(std::string_view alias = "");
ui::entity CreateProgressBar(std::string_view alias = "");

// Canvas / Table / Image from path
/**
 * @brief 通过文件路径创建图像实体（支持 bmp/png/jpeg，首次渲染时懒加载）
 * @param path         图像文件路径
 * @param defaultWidth 默认宽度（0 表示自适应）
 * @param defaultHeight 默认高度（0 表示自适应）
 */
ui::entity CreateImageFromPath(std::string_view path,
                               float defaultWidth = 0.0F,
                               float defaultHeight = 0.0F,
                               std::string_view alias = "");

/**
 * @brief 创建 Canvas 实体（可通过 ui::canvas::Draw* 绘制内容）
 */
ui::entity CreateCanvas(float width = 400.0F, float height = 300.0F, std::string_view alias = "");

/**
 * @brief 创建 Table 实体
 * @param columns 初始列数
 */
ui::entity CreateTable(int columns = 3, std::string_view alias = "");

} // namespace ui::factory
