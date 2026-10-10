#include "src/utils/Logger.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

namespace ui::tests
{
namespace
{
struct LoggerFormatProbe
{
    int* calls;
};
}  // namespace
}  // namespace ui::tests

template <>
struct std::formatter<ui::tests::LoggerFormatProbe> : std::formatter<std::string_view>
{
    auto format(const ui::tests::LoggerFormatProbe& probe, std::format_context& context) const
    {
        ++*probe.calls;
        return std::formatter<std::string_view>::format("probe", context);
    }
};

namespace ui::tests
{
namespace
{
using utils::LogLevel;
using utils::Logger;

class LoggerTest : public ::testing::Test
{
   protected:
    struct Record
    {
        LogLevel level;
        std::string message;
        std::source_location location;
    };

    std::mutex m_mutex;
    std::vector<Record> m_records;
    Logger m_logger{[this](LogLevel level, std::string_view message, std::source_location location)
                    {
                        const std::scoped_lock lock(m_mutex);
                        m_records.push_back({level, std::string(message), location});
                    }};
};

TEST_F(LoggerTest, FormatsArgumentsDynamicStringsAndBraces)
{
    m_logger.debug("中文消息");
    const std::string format = "{} {} {:.2f} {{ok}}";
    constexpr int INTEGER_VALUE = 42;
    constexpr double FLOAT_VALUE = 1.25;
    m_logger.info(format, INTEGER_VALUE, std::string("text"), FLOAT_VALUE);
    m_logger.warn("warning");
    m_logger.error("error");
    ASSERT_EQ(m_records.size(), 4U);
    EXPECT_EQ(m_records[0].message, "中文消息");
    EXPECT_EQ(m_records[1].message, "42 text 1.25 {ok}");
    EXPECT_EQ(m_records[0].level, LogLevel::debug);
    EXPECT_EQ(m_records[1].level, LogLevel::info);
    EXPECT_EQ(m_records[2].level, LogLevel::warn);
    EXPECT_EQ(m_records[3].level, LogLevel::error);
}

TEST_F(LoggerTest, FiltersBeforeInvokingFormatterAndSupportsOff)
{
    int calls = 0;
    const LoggerFormatProbe probe{&calls};
    m_logger.SetLevel(LogLevel::warn);
    m_logger.debug("{}", probe);
    m_logger.info("{}", probe);
    EXPECT_EQ(calls, 0);
    EXPECT_TRUE(m_records.empty());
    m_logger.warn("{}", probe);
    EXPECT_EQ(calls, 1);
    m_logger.SetLevel(LogLevel::off);
    m_logger.error("{}", probe);
    EXPECT_EQ(calls, 1);
    EXPECT_EQ(m_records.size(), 1U);
    EXPECT_EQ(m_logger.GetLevel(), LogLevel::off);
}

TEST_F(LoggerTest, CapturesBusinessCallSite)
{
    const auto expected = std::source_location::current();
    m_logger.info("location");
    ASSERT_EQ(m_records.size(), 1U);
    EXPECT_EQ(m_records.front().location.line(), expected.line() + 1);
    EXPECT_STREQ(m_records.front().location.file_name(), expected.file_name());
    EXPECT_STREQ(m_records.front().location.function_name(), expected.function_name());
}

TEST_F(LoggerTest, InvalidFormatFallsBackWithoutThrowing)
{
    ::testing::internal::CaptureStderr();
    EXPECT_NO_THROW(m_logger.error("{"));
    const auto output = ::testing::internal::GetCapturedStderr();
    EXPECT_NE(output.find("<log formatting failed>"), std::string::npos);
    EXPECT_TRUE(m_records.empty());
    m_logger.info("still usable");
    EXPECT_EQ(m_records.size(), 1U);
}

TEST_F(LoggerTest, RedirectedStderrDoesNotReceiveColorControlCodes)
{
    ::testing::internal::CaptureStderr();
    Logger stderrLogger;
    stderrLogger.info("plain stderr");
    const auto output = ::testing::internal::GetCapturedStderr();
    EXPECT_EQ(output.find("\x1b["), std::string::npos);
    EXPECT_NE(output.find("plain stderr"), std::string::npos);
}

TEST_F(LoggerTest, ConcurrentCallbacksCaptureEveryRecord)
{
    constexpr int THREAD_COUNT = 4;
    constexpr int RECORD_COUNT = 64;
    std::vector<std::jthread> workers;
    workers.reserve(THREAD_COUNT);
    for (int thread = 0; thread < THREAD_COUNT; ++thread)
    {
        workers.emplace_back([this, thread]
                             {
                                 for (int record = 0; record < RECORD_COUNT; ++record)
                                 {
                                     m_logger.info("{}:{}", thread, record);
                                 }
                             });
    }
    workers.clear();
    ASSERT_EQ(m_records.size(), static_cast<std::size_t>(THREAD_COUNT * RECORD_COUNT));
    std::vector<std::string> messages;
    messages.reserve(m_records.size());
    for (const auto& record : m_records)
    {
        messages.push_back(record.message);
    }
    std::ranges::sort(messages);
    EXPECT_EQ(std::unique(messages.begin(), messages.end()), messages.end());
}

TEST_F(LoggerTest, ReentrantCallbackCanReplaceConfigurationWithoutDeadlock)
{
    int calls = 0;
    Logger other([&calls](LogLevel, std::string_view, std::source_location) { ++calls; });
    m_logger.SetCallback([this, &calls, &other](LogLevel, std::string_view, std::source_location)
                         {
                             ++calls;
                             m_logger.SetLevel(LogLevel::info);
                             m_logger.SetCallback({});
                             m_logger.info("nested same logger");
                             other.info("nested other logger");
                         });
    ::testing::internal::CaptureStderr();
    m_logger.info("outer");
    const auto output = ::testing::internal::GetCapturedStderr();
    EXPECT_EQ(calls, 1);
    EXPECT_NE(output.find("nested same logger"), std::string::npos);
    EXPECT_NE(output.find("nested other logger"), std::string::npos);
}

TEST_F(LoggerTest, ThrowingCallbackDoesNotEscapeAndGuardResets)
{
    int calls = 0;
    m_logger.SetCallback([&calls](LogLevel, std::string_view, std::source_location)
                         {
                             ++calls;
                             throw std::runtime_error("sink failed");
                         });
    ::testing::internal::CaptureStderr();
    EXPECT_NO_THROW(m_logger.error("original failure"));
    EXPECT_NO_THROW(m_logger.error("next failure"));
    const auto output = ::testing::internal::GetCapturedStderr();
    EXPECT_EQ(calls, 2);
    EXPECT_NE(output.find("<log callback/output failed>"), std::string::npos);
    EXPECT_NE(output.find("original failure"), std::string::npos);
    EXPECT_NE(output.find("next failure"), std::string::npos);
}

TEST_F(LoggerTest, ReplacementKeepsInFlightOwnedCaptureAlive)
{
    std::atomic<bool> entered{false};
    std::atomic<bool> release{false};
    auto state = std::make_shared<int>(0);
    const std::weak_ptr<int> lifetime = state;
    m_logger.SetCallback([owned = std::move(state), &entered, &release](LogLevel, std::string_view,
                                                                      std::source_location)
                         {
                             entered.store(true);
                             entered.notify_one();
                             release.wait(false);
                             ++*owned;
                         });
    std::jthread worker([this] { m_logger.info("in flight"); });
    entered.wait(false);
    m_logger.SetCallback({});
    EXPECT_FALSE(lifetime.expired());
    release.store(true);
    release.notify_one();
    worker.join();
    EXPECT_TRUE(lifetime.expired());

    int observed = 0;
    m_logger.SetCallback([owned = std::make_unique<int>(1), &observed](LogLevel, std::string_view,
                                                                    std::source_location) { observed = *owned; });
    m_logger.info("move-only capture");
    EXPECT_EQ(observed, 1);
}

TEST_F(LoggerTest, MultipleInstancesWriteWholeStderrRecords)
{
    constexpr int RECORD_COUNT = 64;
    Logger first;
    Logger second;
    const auto location = std::source_location::current();
    ::testing::internal::CaptureStderr();
    {
        std::jthread left([&]
                          {
                              for (int index = 0; index < RECORD_COUNT; ++index)
                              {
                                  first.info(utils::LogLocation("left {}", location), index);
                              }
                          });
        std::jthread right([&]
                           {
                               for (int index = 0; index < RECORD_COUNT; ++index)
                               {
                                   second.info(utils::LogLocation("right {}", location), index);
                               }
                           });
    }
    const auto output = ::testing::internal::GetCapturedStderr();
    EXPECT_EQ(std::ranges::count(output, '\n'), RECORD_COUNT * 2);
    for (int index = 0; index < RECORD_COUNT; ++index)
    {
        for (const auto* name : {"left", "right"})
        {
            const auto expected = std::format("[info] {} {} [{}:{} {}]\n", name, index, location.file_name(),
                                              location.line(), location.function_name());
            EXPECT_NE(output.find(expected), std::string::npos);
        }
    }
}
}  // namespace
}  // namespace ui::tests