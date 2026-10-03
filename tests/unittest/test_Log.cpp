#include <atomic>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "ui/api/Log.hpp"

namespace
{

std::atomic<int> g_callbackCount{0};
ui::log::Level g_lastLevel = ui::log::Level::NO_DEBUG;
std::string g_lastMessage;

void CaptureLog(ui::log::Level level, std::string_view message)
{
    g_lastLevel = level;
    g_lastMessage.assign(message);
    g_callbackCount.fetch_add(1, std::memory_order_relaxed);
}

class ExternalLogCallbackTest : public ::testing::Test
{
   protected:
    void SetUp() override
    {
        g_callbackCount.store(0, std::memory_order_relaxed);
        g_lastLevel = ui::log::Level::NO_DEBUG;
        g_lastMessage.clear();
        ui::log::SetCallback(CaptureLog);
    }

    void TearDown() override
    {
        ui::log::SetCallback(nullptr);
        ui::log::SetDebugLevel(ui::log::Level::NO_DEBUG);
    }
};

TEST_F(ExternalLogCallbackTest, DoesNotRequireRuntime)
{
    ui::log::SetDebugLevel(ui::log::Level::DEBUG);

    EXPECT_NO_FATAL_FAILURE(ui::log::Info("external callback"));
    EXPECT_EQ(g_callbackCount.load(std::memory_order_relaxed), 1);
    EXPECT_EQ(g_lastLevel, ui::log::Level::INFO);
    EXPECT_EQ(g_lastMessage, "external callback");
}

TEST_F(ExternalLogCallbackTest, FiltersBelowConfiguredLevel)
{
    ui::log::SetDebugLevel(ui::log::Level::WARNING);

    ui::log::Debug("debug");
    ui::log::Info("info");
    ui::log::Warning("warning");
    ui::log::Error("error");

    EXPECT_EQ(g_callbackCount.load(std::memory_order_relaxed), 2);
    EXPECT_EQ(g_lastLevel, ui::log::Level::ERROR);
    EXPECT_EQ(g_lastMessage, "error");
}

TEST_F(ExternalLogCallbackTest, NoDebugFiltersEverything)
{
    ui::log::SetDebugLevel(ui::log::Level::NO_DEBUG);

    ui::log::Debug("debug");
    ui::log::Critical("critical");

    EXPECT_EQ(g_callbackCount.load(std::memory_order_relaxed), 0);
}

TEST_F(ExternalLogCallbackTest, NullCallbackDisablesDelivery)
{
    ui::log::SetDebugLevel(ui::log::Level::DEBUG);
    ui::log::SetCallback(nullptr);

    EXPECT_NO_FATAL_FAILURE(ui::log::Error("not delivered"));
    EXPECT_EQ(g_callbackCount.load(std::memory_order_relaxed), 0);
}

}  // namespace