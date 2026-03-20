#include <gtest/gtest.h>

#include <ui.hpp>

#include "src/ui/common/GlobalContext.hpp"
#include "src/ui/singleton/Dispatcher.hpp"
#include "src/ui/singleton/Registry.hpp"
#include "src/ui/systems/ActionSystem.hpp"
#include "src/ui/systems/TweenSystem.hpp"

namespace ui::tests
{
namespace
{

class UiTweenSystemTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        Registry::Clear();

        if (auto* frameContext = Registry::ctx().find<globalcontext::FrameContext>())
        {
            frameContext->intervalMs = 16;
        }
        else
        {
            Registry::ctx().emplace<globalcontext::FrameContext>().intervalMs = 16;
        }
    }

    void TearDown() override { Registry::Clear(); }
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

} // namespace
} // namespace ui::tests