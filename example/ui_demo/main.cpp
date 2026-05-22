#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <exception>
#include <span>
#include <utility>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h> // IWYU pragma: keep
#include <consoleapi2.h>
#include <winnls.h>
#endif

#include <ui.hpp>

#include "View/menu.h"

namespace
{
void InitializeConsoleUtf8()
{
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);
#endif
}

void WriteStderr(const char* text) noexcept
{
    if (text == nullptr)
    {
        return;
    }

    const auto textSize = std::strlen(text);
    if (std::fwrite(text, 1U, textSize, stderr) != textSize)
    {
        std::clearerr(stderr);
    }
}
} // namespace

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[]) noexcept
{
    try
    {
        InitializeConsoleUtf8();

        auto appResult = ui::factory::CreateApplication(std::span<char*>(argv, argc));
        if (!appResult.has_value())
        {
            WriteStderr("UI init failed: ");
            WriteStderr(appResult.error().message().c_str());
            WriteStderr("\n");
            return EXIT_FAILURE;
        }

        auto app = std::move(*appResult);

        example::ui_demo::view::CreateMenuDialog();
        ui::utils::TimerCallback(5000, []() { ui::log::Info("定时任务1执行！"); });
        app->exec();
        return EXIT_SUCCESS;
    }
    catch (const std::exception& exception)
    {
        WriteStderr("Application terminated by exception: ");
        WriteStderr(exception.what());
        WriteStderr("\n");
        return EXIT_FAILURE;
    }
    catch (...)
    {
        WriteStderr("Unknown exception occurred.\n");
        return EXIT_FAILURE;
    }
}
