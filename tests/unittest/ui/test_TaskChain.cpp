#include <gtest/gtest.h>

#include <entt/entt.hpp>

#include <string>
#include <vector>

#include "src/ui/common/Events.hpp"
#include "src/ui/common/GlobalContext.hpp"
#include "src/ui/core/TaskChain.hpp"
#include "src/ui/core/RuntimeFacade.hpp"
#include "src/ui/core/UiRuntime.hpp"
#include "src/ui/singleton/Dispatcher.hpp"
#include "src/ui/singleton/Registry.hpp"

namespace ui::tests
{
namespace
{

struct CountingTask
{
    using is_task_tag = void;

    int* callCount;
    int* lastSum;

    void operator()(int left, int right) const
    {
        ++(*callCount);
        *lastSum = left + right;
    }
};

struct SummingTask
{
    using is_task_tag = void;

    int* callCount;

    int operator()(int left, int right) const
    {
        ++(*callCount);
        return left + right;
    }
};

struct EventRecorder
{
    std::vector<std::string>* events;

    void onLayout(const events::UpdateLayout& event) const
    {
        (void)event;
        events->push_back("layout");
    }

    void onRender(const events::UpdateRendering& event) const
    {
        (void)event;
        events->push_back("render");
    }

    void onEndFrame(const events::EndFrame& event) const
    {
        (void)event;
        events->push_back("end_frame");
    }

    void onUpdate(const events::UpdateEvent& event) const
    {
        (void)event;
        events->push_back("update");
    }

    void onTimer(const events::UpdateTimer& event) const
    {
        (void)event;
        events->push_back("timer");
    }
};

class TaskChainTest : public ::testing::Test
{
protected:
    UiRuntime m_runtime;
    std::unique_ptr<UiRuntimeScope> m_scope;

    void SetUp() override
    {
        m_scope = std::make_unique<UiRuntimeScope>(m_runtime);
        auto& frame = RuntimeFacade::current().ensureContext<globalcontext::FrameContext>();
        frame.intervalMs = 0;
        frame.frameSlot = 0;
    }

    void TearDown() override
    {
        Dispatcher::Update();
        m_scope.reset();
    }
};

TEST_F(TaskChainTest, CombinedBroadcastsArgumentsAndReturnsSecondResult)
{
    int firstCallCount = 0;
    int secondCallCount = 0;
    int lastFirstSum = 0;

    auto combined = tasks::PIPE_COMPOSER(CountingTask{&firstCallCount, &lastFirstSum}, SummingTask{&secondCallCount});

    const int result = combined(7, 5);

    EXPECT_EQ(firstCallCount, 1);
    EXPECT_EQ(secondCallCount, 1);
    EXPECT_EQ(lastFirstSum, 12);
    EXPECT_EQ(result, 12);
}

TEST_F(TaskChainTest, WrapArgsBindsArgumentsBeforeExecutingTask)
{
    int observedValue = 0;

    auto task = [&observedValue](int left, int right)
    {
        observedValue = left * right;
        return observedValue;
    };

    auto context = tasks::WrapArgs(6, 7);
    auto bound = context | task;

    EXPECT_EQ(bound(), 42);
    EXPECT_EQ(observedValue, 42);
}

TEST_F(TaskChainTest, QueuedTaskUpdatesFrameContextBeforeDispatchingQueuedEvents)
{
    std::vector<std::string> observedEvents;
    EventRecorder recorder{&observedEvents};
    auto updateConnection = entt::scoped_connection{
        Dispatcher::Sink<events::UpdateEvent>().template connect<&EventRecorder::onUpdate>(recorder)};
    auto timerConnection = entt::scoped_connection{
        Dispatcher::Sink<events::UpdateTimer>().template connect<&EventRecorder::onTimer>(recorder)};

    Dispatcher::Enqueue<events::UpdateEvent>({});

    tasks::QueuedTask queuedTask;
    queuedTask(33);

    const auto& frameContext = Registry::ctx().get<globalcontext::FrameContext>();
    EXPECT_EQ(frameContext.intervalMs, 33U);
    EXPECT_EQ(frameContext.frameSlot, 1U);
    ASSERT_EQ(observedEvents.size(), 2U);
    EXPECT_EQ(observedEvents[0], "timer");
    EXPECT_EQ(observedEvents[1], "update");
}

TEST_F(TaskChainTest, RenderTaskTriggersFrameStagesInFixedOrderAndHonorsDelay)
{
    std::vector<std::string> observedEvents;
    EventRecorder recorder{&observedEvents};
    auto layoutConnection = entt::scoped_connection{
        Dispatcher::Sink<events::UpdateLayout>().template connect<&EventRecorder::onLayout>(recorder)};
    auto renderConnection = entt::scoped_connection{
        Dispatcher::Sink<events::UpdateRendering>().template connect<&EventRecorder::onRender>(recorder)};
    auto endFrameConnection = entt::scoped_connection{
        Dispatcher::Sink<events::EndFrame>().template connect<&EventRecorder::onEndFrame>(recorder)};

    tasks::RenderTask renderTask;

    renderTask(16);

    ASSERT_EQ(observedEvents.size(), 3U);
    EXPECT_EQ(observedEvents[0], "layout");
    EXPECT_EQ(observedEvents[1], "render");
    EXPECT_EQ(observedEvents[2], "end_frame");
    EXPECT_EQ(renderTask.remainingTime, 16U);

    observedEvents.clear();
    renderTask(15);
    EXPECT_TRUE(observedEvents.empty());
    EXPECT_EQ(renderTask.remainingTime, 1U);

    renderTask(1);
    ASSERT_EQ(observedEvents.size(), 3U);
    EXPECT_EQ(observedEvents[0], "layout");
    EXPECT_EQ(observedEvents[1], "render");
    EXPECT_EQ(observedEvents[2], "end_frame");
    EXPECT_EQ(renderTask.remainingTime, 16U);
}

} // namespace
} // namespace ui::tests