#include <gtest/gtest.h>

#include <entt/entt.hpp>

#include "src/common/Components.hpp"
#include "src/common/Events.hpp"
#include "src/common/GlobalContext.hpp"
#include "src/core/RuntimeFacade.hpp"
#include "src/core/UiRuntime.hpp"
#include "src/singleton/Dispatcher.hpp"
#include "src/singleton/Registry.hpp"

namespace ui::tests
{
namespace
{

struct UpdateFlagHandler
{
    bool* triggered;

    void onUpdate(const events::UpdateEvent& event) const
    {
        (void)event;
        *triggered = true;
    }
};

TEST(UiRuntimeTest, NestedRuntimeScopesSwitchRegistryAndDispatcherIndependently)
{
    UiRuntime firstRuntime;
    UiRuntime secondRuntime;
    bool firstTriggered = false;
    bool secondTriggered = false;

    {
        UiRuntimeScope firstScope(firstRuntime);
        Registry::ctx().emplace<globalcontext::FrameContext>().intervalMs = 11;

        UpdateFlagHandler firstHandler{&firstTriggered};
        auto firstConnection = entt::scoped_connection{
            Dispatcher::Sink<events::UpdateEvent>().template connect<&UpdateFlagHandler::onUpdate>(firstHandler)};

        {
            UiRuntimeScope secondScope(secondRuntime);

            EXPECT_EQ(Registry::ctx().find<globalcontext::FrameContext>(), nullptr);

            UpdateFlagHandler secondHandler{&secondTriggered};
            auto secondConnection = entt::scoped_connection{
                Dispatcher::Sink<events::UpdateEvent>().template connect<&UpdateFlagHandler::onUpdate>(secondHandler)};

            Dispatcher::Enqueue<events::UpdateEvent>({});
            Dispatcher::Update();

            EXPECT_FALSE(firstTriggered);
            EXPECT_TRUE(secondTriggered);

            Registry::ctx().emplace<globalcontext::FrameContext>().intervalMs = 22;
            EXPECT_EQ(Registry::ctx().get<globalcontext::FrameContext>().intervalMs, 22U);
        }

        ASSERT_NE(Registry::ctx().find<globalcontext::FrameContext>(), nullptr);
        EXPECT_EQ(Registry::ctx().get<globalcontext::FrameContext>().intervalMs, 11U);

        Dispatcher::Enqueue<events::UpdateEvent>({});
        Dispatcher::Update();

        EXPECT_TRUE(firstTriggered);
    }
}

TEST(UiRuntimeTest, RuntimeFacadeFollowsActiveRuntimeScope)
{
    UiRuntime firstRuntime;
    UiRuntime secondRuntime;

    {
        UiRuntimeScope firstScope(firstRuntime);
        RuntimeFacade::current().ensureContext<globalcontext::FrameContext>().intervalMs = 33;

        EXPECT_EQ(RuntimeFacade::current().frame().intervalMs, 33U);
        EXPECT_NE(RuntimeFacade::current().tryFrame(), nullptr);

        {
            UiRuntimeScope secondScope(secondRuntime);

            EXPECT_EQ(RuntimeFacade::current().tryFrame(), nullptr);

            RuntimeFacade::current().ensureContext<globalcontext::FrameContext>().intervalMs = 44;
            EXPECT_EQ(RuntimeFacade::current().frame().intervalMs, 44U);
        }

        EXPECT_EQ(RuntimeFacade::current().frame().intervalMs, 33U);
    }
}

TEST(UiRuntimeTest, WindowLookupCacheIsolatedPerRuntime)
{
    UiRuntime firstRuntime;
    UiRuntime secondRuntime;
    entt::entity firstWindow = entt::null;

    {
        UiRuntimeScope firstScope(firstRuntime);
        firstWindow = Registry::Create();
        Registry::Emplace<components::Window>(firstWindow).windowID = 101;
        RuntimeFacade::current().windowLookup().remember(firstWindow);

        EXPECT_EQ(RuntimeFacade::current().windowLookup().findById(101), firstWindow);

        {
            UiRuntimeScope secondScope(secondRuntime);

            EXPECT_FALSE(Registry::Valid(RuntimeFacade::current().windowLookup().findById(101)));

            const auto secondWindow = Registry::Create();
            Registry::Emplace<components::Window>(secondWindow).windowID = 101;
            RuntimeFacade::current().windowLookup().remember(secondWindow);

            EXPECT_EQ(RuntimeFacade::current().windowLookup().findById(101), secondWindow);
        }

        EXPECT_EQ(RuntimeFacade::current().windowLookup().findById(101), firstWindow);
    }
}

TEST(UiRuntimeTest, WindowLookupCacheRecoversFromDestroyedEntity)
{
    UiRuntime runtime;
    {
        UiRuntimeScope scope(runtime);

        const auto firstWindow = Registry::Create();
        Registry::Emplace<components::Window>(firstWindow).windowID = 202;
        RuntimeFacade::current().windowLookup().remember(firstWindow);

        EXPECT_EQ(RuntimeFacade::current().windowLookup().findById(202), firstWindow);

        Registry::Destroy(firstWindow);

        const auto secondWindow = Registry::Create();
        Registry::Emplace<components::Window>(secondWindow).windowID = 202;

        EXPECT_EQ(RuntimeFacade::current().windowLookup().findById(202), secondWindow);
    }
}

} // namespace
} // namespace ui::tests