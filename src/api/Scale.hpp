#pragma once

#include <cmath>

#include "common/AppConfig.hpp"
#include "common/Types.hpp"

namespace ui::scale
{
[[nodiscard]] inline float CurrentUiScale() noexcept
{
    return config::AppConfig::instance().platformUiScale();
}

[[nodiscard]] inline float Metric(float value) noexcept
{
    if (!std::isfinite(value) || value == 0.0F) return value;
    return value * CurrentUiScale();
}

[[nodiscard]] inline Vec2 Metric(const Vec2& value) noexcept
{
    return {Metric(value.x()), Metric(value.y())};
}
} // namespace ui::scale