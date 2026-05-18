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
#include <expected>
#include <memory>
#include <system_error>
#include <vector>
#include <entt/entt.hpp>
#include <string>
#include <string_view>
#include "../common/UiErrors.hpp"
#include "../core/Application.hpp"
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
std::expected<std::unique_ptr<Application>, std::error_code> CreateApplication(std::span<char*> argv);
/**
 * @brief 创建一个基础的 UI 组件实体
 * @param alias 组件别名
 * @return entt::entity 创建的实体
 */
entt::entity CreateBaseWidget(std::string_view alias = "");
/**
 * @brief 为指定实体创建一个淡入动画组件
 * @param entity 目标实体
 * @param duration 动画持续时间（秒）
 */
void CreateFadeInAnimation(entt::entity entity, float duration);

/**
 * @brief 创建一个按钮实体
 * @param content 按钮显示的文本内容
 * @param alias 组件别名
 * @return entt::entity 创建的实体
 */
entt::entity CreateButton(const std::string& content, std::string_view alias = "");
entt::entity CreateLabel(const std::string& content, std::string_view alias = "");
entt::entity CreateTextEdit(const std::string& placeholder = "", bool multiline = false, std::string_view alias = "");
entt::entity
    CreateImage(void* textureId, float defaultWidth = 50.0F, float defaultHeight = 50.0F, std::string_view alias = "");
entt::entity CreateArrow(const Vec2& start, const Vec2& end, std::string_view alias = "");
entt::entity CreateSpacer(int stretchFactor = 1, std::string_view alias = "");
entt::entity CreateSpacer(float width, float height, std::string_view alias = "");
entt::entity CreateDialog(std::string_view title, std::string_view alias = "");
entt::entity CreateScrollArea(std::string_view alias = "");
entt::entity CreateWindow(std::string_view title, std::string_view alias = "");

/**
 * @brief \u521b\u5efa\u81ea\u7ed8\u6807\u9898\u680f\u5b9e\u4f53\uff08\u7528\u4e8e\u65e0\u8fb9\u6846\u7a97\u53e3\uff09
 * @param windowEntity \u6240\u5c5e\u7a97\u53e3\u5b9e\u4f53\uff08\u5fc5\u987b\u5df2\u6709 Window \u7ec4\u4ef6\uff09
 * @param alias \u7ec4\u4ef6\u522b\u540d
 * @return entt::entity \u6807\u9898\u680f\u5b9e\u4f53\uff08\u5df2\u4f5c\u4e3a windowEntity
 * \u7684\u9996\u4e2a\u5b50\u8282\u70b9\uff09
 */
entt::entity CreateTitleBar(entt::entity windowEntity, std::string_view alias = "");
entt::entity CreateVBoxLayout(std::string_view alias = "");
entt::entity CreateHBoxLayout(std::string_view alias = "");
entt::entity
    CreateLineEdit(std::string_view initialText = "", std::string_view placeholder = "", std::string_view alias = "");
entt::entity CreateTextBrowser(std::string_view initialText = "",
                               std::string_view placeholder = "",
                               std::string_view alias = "");

/**
 * @brief 创建一个复选框实体
 * @param label 复选框标签文本
 * @param checked 是否默认选中
 * @param alias 组件别名
 * @return entt::entity 创建的实体
 */
entt::entity CreateCheckBox(const std::string& label, bool checked = false, std::string_view alias = "");
entt::entity CreateDropDown(const std::vector<std::string>& options, int selectedIndex = 0, std::string_view alias = "");

// Slider / ProgressBar
entt::entity CreateSlider(std::string_view alias = "");
entt::entity CreateProgressBar(std::string_view alias = "");

// Canvas / Table / Image from path
/**
 * @brief 通过文件路径创建图像实体（支持 bmp/png/jpeg，首次渲染时懒加载）
 * @param path         图像文件路径
 * @param defaultWidth 默认宽度（0 表示自适应）
 * @param defaultHeight 默认高度（0 表示自适应）
 */
entt::entity CreateImageFromPath(std::string_view path,
                                 float defaultWidth = 0.0F,
                                 float defaultHeight = 0.0F,
                                 std::string_view alias = "");

/**
 * @brief 创建 Canvas 实体（可通过 ui::canvas::Draw* 绘制内容）
 */
entt::entity CreateCanvas(float width = 400.0F, float height = 300.0F, std::string_view alias = "");

/**
 * @brief 创建 Table 实体
 * @param columns 初始列数
 */
entt::entity CreateTable(int columns = 3, std::string_view alias = "");

} // namespace ui::factory
