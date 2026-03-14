/**
 * ************************************************************************
 *
 * @file main.cpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2025-12-18
 * @version 0.2
 * @brief 客户端主程序（集成 UI 和 Net 模块）
 *
 * ************************************************************************
 * @copyright Copyright (c) 2025 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#include <iostream>

#include "src/utils/Functions.h"

// 引入View层
#include "View/menu.h"

int main([[maybe_unused]] int argc, [[maybe_unused]] char* argv[])
{
    utils::functions::SetConsoleToUTF8();
    try
    {
        auto appResult = ui::factory::CreateApplication(std::span<char*>(argv, argc));
        if (!appResult.has_value())
        {
            std::cerr << "UI 初始化失败: " << appResult.error().message() << std::endl;
            return EXIT_FAILURE;
        }

        auto app = std::move(appResult).value();

        // 创建菜单对话框
        client::view::CreateMenuDialog();
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
