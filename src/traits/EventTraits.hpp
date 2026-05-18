#pragma once

#include "../common/Events.hpp"

namespace ui::traits
{

template <typename T>
concept Events = requires { typename T::is_event_tag; };

} // namespace ui::traits