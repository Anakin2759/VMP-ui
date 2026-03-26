#include <gtest/gtest.h>

#include <entt/entt.hpp>

#include "src/ui/common/Events.hpp"
#include "src/ui/common/GlobalContext.hpp"
#include "src/ui/core/UiRuntime.hpp"
#include "src/ui/singleton/Dispatcher.hpp"
#include "src/ui/singleton/Registry.hpp"

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
    Registry::Clear();
    Dispatcher::Update();

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

    Registry::Clear();
    Dispatcher::Update();
}

} // namespace
} // namespace ui::tests