/**
 * ************************************************************************
 *
 * @file TextEditingService.hpp
 * @brief TextEdit 编辑命令服务
 *
 * ************************************************************************
 */

#pragma once

#include <SDL3/SDL.h>

#include <string>

namespace ui::services
{

class TextEditingService
{
public:
    static void handleTextInput(const std::string& rawText);
    static void handleKeyDown(SDL_Keycode key, SDL_Keymod modState);
};

} // namespace ui::services