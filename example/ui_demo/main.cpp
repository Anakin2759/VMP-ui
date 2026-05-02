#include <cstdlib>
#include <exception>
#include <iostream>
#include <span>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

#include "../../src/ui/ui/ui.hpp"

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
} // namespace

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    InitializeConsoleUtf8();

    try
    {
        auto appResult = ui::factory::CreateApplication(std::span<char*>(argv, argc));
        if (!appResult.has_value())
        {
            std::cerr << "UI 初始化失败: " << appResult.error().message() << std::endl;
            return EXIT_FAILURE;
        }

        auto app = std::move(appResult).value();

        example::ui_demo::view::CreateMenuDialog();
        ui::utils::TimerCallback(5000, []() { std::cout << "定时任务1执行！" << std::endl; });
        app->exec();
    }
    catch (const std::exception& e)
    {
        std::cerr << "应用程序异常终止: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }
    catch (...)
    {
        std::cerr << "Unknown exception occurred." << std::endl;
        return EXIT_FAILURE;
    }
}
