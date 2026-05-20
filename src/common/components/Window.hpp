/**
 * @file Window.hpp
 * @brief 窗口/对话框组件
 *
 * 包含：Window
 */
#pragma once

#include "../Types.hpp"
#include "../Policies.hpp"
#include <string>
#include <cfloat>

namespace ui::components
{

/**
 * @brief 窗口组件
 */
struct Window
{
    using is_component_tag = void;
    static constexpr float MIN_WID = 300.0F;
    static constexpr float MIN_HIG = 200.0F;
    std::string title;
    Vec2 minSize{MIN_WID, MIN_HIG};
    Vec2 maxSize{FLT_MAX, FLT_MAX};
    policies::WindowFlag flags = policies::WindowFlag::Default;
    uint32_t windowID = 0;
};

} // namespace ui::components
