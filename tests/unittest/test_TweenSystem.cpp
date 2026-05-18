#include <gtest/gtest.h>

#include <ui.hpp>

#include "src/common/GlobalContext.hpp"
#include "src/core/RuntimeFacade.hpp"
#include "src/core/UiRuntime.hpp"
#include "src/singleton/Dispatcher.hpp"
#include "src/singleton/Registry.hpp"
#include "src/systems/ActionSystem.hpp"
#include "src/systems/TweenSystem.hpp"

namespace ui::tests
{
namespace
{

class UiTweenSystemTest : public ::testing::Test
{
protected:
    UiRuntime m_runtime;
    std::unique_ptr<UiRuntimeScope> m_scope;

    void SetUp() override
    {
        m_scope = std::make_unique<UiRuntimeScope>(m_runtime);
        RuntimeFacade::current().ensureContext<globalcontext::FrameContext>().intervalMs = 16;
    }

    void TearDown() override { m_scope.reset(); }
};

TEST_F(UiTweenSystemTest, PositionTweenCompletesAndCleansUp)
{
    systems::TweenSystem tweenSystem;
    tweenSystem.registerHandlers();

    const auto entity = factory::CreateLabel("Tween", "tween_label");
    auto& position = Registry::Get<components::Position>(entity);
    position.value = {4.0F, 6.0F};

    animation::TweenOptions options;
    options.duration = 16.0F;

    animation::StartPositionAnimation(entity, position.value, {24.0F, 36.0F}, options);

    ASSERT_TRUE(Registry::AllOf<components::AnimatingTag>(entity));
    ASSERT_NE(Registry::TryGet<components::AnimationTime>(entity), nullptr);

    Dispatcher::Trigger<events::UpdateEvent>({});

    EXPECT_FLOAT_EQ(position.value.x(), 24.0F);
    EXPECT_FLOAT_EQ(position.value.y(), 36.0F);
    EXPECT_FALSE(Registry::AllOf<components::AnimatingTag>(entity));
    EXPECT_EQ(Registry::TryGet<components::AnimationTime>(entity), nullptr);
    EXPECT_EQ(Registry::TryGet<components::AnimationPosition>(entity), nullptr);

    tweenSystem.unregisterHandlers();
}

TEST_F(UiTweenSystemTest, InteractiveAnimationFlowsThroughTweenPipeline)
{
    systems::ActionSystem actionSystem;
    systems::TweenSystem tweenSystem;
    actionSystem.registerHandlers();
    tweenSystem.registerHandlers();

    const auto entity = factory::CreateButton("Hover", "hover_btn");
    auto& interaction = Registry::Emplace<components::InteractiveAnimation>(entity);
    interaction.hoverScale = Vec2{1.15F, 1.15F};
    interaction.hoverOffset = Vec2{0.0F, -3.0F};
    interaction.hoverDuration = 16.0F;
    interaction.normalScale = Vec2{1.0F, 1.0F};
    interaction.normalOffset = Vec2{0.0F, 0.0F};

    Dispatcher::Trigger<events::HoverEvent>({entity});

    ASSERT_TRUE(Registry::AllOf<components::AnimatingTag>(entity));
    ASSERT_NE(Registry::TryGet<components::AnimationTime>(entity), nullptr);
    ASSERT_NE(Registry::TryGet<components::AnimationScale>(entity), nullptr);
    ASSERT_NE(Registry::TryGet<components::AnimationRenderOffset>(entity), nullptr);

    Dispatcher::Trigger<events::UpdateEvent>({});

    const auto* scale = Registry::TryGet<components::Scale>(entity);
    const auto* offset = Registry::TryGet<components::RenderOffset>(entity);
    ASSERT_NE(scale, nullptr);
    ASSERT_NE(offset, nullptr);
    EXPECT_FLOAT_EQ(scale->value.x(), 1.15F);
    EXPECT_FLOAT_EQ(scale->value.y(), 1.15F);
    EXPECT_FLOAT_EQ(offset->value.x(), 0.0F);
    EXPECT_FLOAT_EQ(offset->value.y(), -3.0F);
    EXPECT_EQ(Registry::TryGet<components::AnimationTime>(entity), nullptr);

    Dispatcher::Trigger<events::UnhoverEvent>({entity});
    ASSERT_TRUE(Registry::AllOf<components::AnimatingTag>(entity));

    Dispatcher::Trigger<events::UpdateEvent>({});

    EXPECT_FLOAT_EQ(Registry::Get<components::Scale>(entity).value.x(), 1.0F);
    EXPECT_FLOAT_EQ(Registry::Get<components::Scale>(entity).value.y(), 1.0F);
    EXPECT_FLOAT_EQ(Registry::Get<components::RenderOffset>(entity).value.x(), 0.0F);
    EXPECT_FLOAT_EQ(Registry::Get<components::RenderOffset>(entity).value.y(), 0.0F);

    tweenSystem.unregisterHandlers();
    actionSystem.unregisterHandlers();
}

// Phase 2 architecture-protection test: Tween updates against the active runtime registry
// only, and do not leak across UiRuntimeScope boundaries.
TEST(UiTweenSystemRuntimeIsolationTest, TweenStaysWithinActiveRuntimeScope)
{
    UiRuntime defaultRuntime;
    UiRuntime alternateRuntime;

    {
        UiRuntimeScope defaultScope(defaultRuntime);

        // ---- Setup entity & animation in the default runtime ----
        RuntimeFacade::current().ensureContext<globalcontext::FrameContext>().intervalMs = 16;

        systems::TweenSystem defaultTween;
        defaultTween.registerHandlers();

        const auto defaultEntity = factory::CreateLabel("Tween-Default", "tween_default");
        auto& defaultPos = Registry::Get<components::Position>(defaultEntity);
        defaultPos.value = {0.0F, 0.0F};

        animation::TweenOptions opts;
        opts.duration = 16.0F;
        animation::StartPositionAnimation(defaultEntity, defaultPos.value, {10.0F, 20.0F}, opts);

        ASSERT_TRUE(Registry::AllOf<components::AnimatingTag>(defaultEntity));

        // ---- Switch to alternate runtime: it must not see the default-runtime entity ----
        {
            UiRuntimeScope altScope(alternateRuntime);

            // The alternate runtime is empty: facade-routed entt::registry must report so.
            EXPECT_FALSE(RuntimeFacade::current().enttRegistry().valid(defaultEntity));

            // Triggering UpdateEvent in the alternate runtime must NOT advance the default
            // runtime animation — the handler is bound to default's dispatcher.
            RuntimeFacade::current().ensureContext<globalcontext::FrameContext>().intervalMs = 16;
            Dispatcher::Trigger<events::UpdateEvent>({});
        }

        // Default runtime entity should still be in mid-animation (nothing advanced it).
        ASSERT_TRUE(Registry::AllOf<components::AnimatingTag>(defaultEntity));
        EXPECT_FLOAT_EQ(defaultPos.value.x(), 0.0F);
        EXPECT_FLOAT_EQ(defaultPos.value.y(), 0.0F);

        // Now advance the default runtime explicitly — animation should complete.
        Dispatcher::Trigger<events::UpdateEvent>({});

        EXPECT_FLOAT_EQ(defaultPos.value.x(), 10.0F);
        EXPECT_FLOAT_EQ(defaultPos.value.y(), 20.0F);
        EXPECT_FALSE(Registry::AllOf<components::AnimatingTag>(defaultEntity));

        defaultTween.unregisterHandlers();
    }
}

} // namespace
} // namespace ui::tests