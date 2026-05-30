#include "Animation.hpp"
#include "core/RuntimeFacade.hpp"
#include "entt/entity/fwd.hpp"
#include "common/components/Animation.hpp"
#include "common/Policies.hpp"
#include "common/Tags.hpp"
#include "common/Types.hpp"
#include <optional>
#include "common/components/Visual.hpp"

namespace ui::animation
{
namespace
{

[[nodiscard]] entt::registry& CurrentRegistry()
{
    return RuntimeFacade::current().enttRegistry();
}

void ConfigureTiming(::entt::entity entity, const TweenOptions& options)
{
    auto& reg = CurrentRegistry();
    auto& time = reg.get_or_emplace<components::AnimationTime>(entity);
    time.duration = options.duration;
    time.elapsed = 0.0F;
    time.easing = options.easing;
    time.mode = options.mode;
    time.state = policies::AnimationState::PLAYING;
    time.autoCleanup = options.autoCleanup;

    reg.emplace_or_replace<components::AnimatingTag>(entity);
}

} // namespace

void StartPositionAnimation(::entt::entity entity,
                            const Vec2& startPos,
                            const Vec2& endPos,
                            const TweenOptions& options)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;

    auto& posAnim = reg.get_or_emplace<components::AnimationPosition>(entity);
    posAnim.from = startPos;
    posAnim.to = endPos;

    ConfigureTiming(entity, options);
}

void StartAlphaAnimation(::entt::entity entity, float startAlpha, float endAlpha, const TweenOptions& options)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;

    auto& alphaAnim = reg.get_or_emplace<components::AnimationAlpha>(entity);
    alphaAnim.from = startAlpha;
    alphaAnim.to = endAlpha;

    ConfigureTiming(entity, options);
}

void StartScaleAnimation(::entt::entity entity,
                         const Vec2& startScale,
                         const Vec2& endScale,
                         const TweenOptions& options)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;

    auto& scaleAnim = reg.get_or_emplace<components::AnimationScale>(entity);
    scaleAnim.from = startScale;
    scaleAnim.to = endScale;

    ConfigureTiming(entity, options);
}

void StartRenderOffsetAnimation(::entt::entity entity,
                                const Vec2& startOffset,
                                const Vec2& endOffset,
                                const TweenOptions& options)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;

    auto& offsetAnim = reg.get_or_emplace<components::AnimationRenderOffset>(entity);
    offsetAnim.from = startOffset;
    offsetAnim.to = endOffset;

    ConfigureTiming(entity, options);
}

void StartColorAnimation(::entt::entity entity,
                         const Color& startColor,
                         const Color& endColor,
                         const TweenOptions& options)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;

    auto& colorAnim = reg.get_or_emplace<components::AnimationColor>(entity);
    colorAnim.from = startColor;
    colorAnim.to = endColor;

    ConfigureTiming(entity, options);
}

void StartTransformAnimation(::entt::entity entity,
                             const std::optional<Vec2>& targetScale,
                             const std::optional<Vec2>& targetOffset,
                             const TweenOptions& options,
                             const Vec2& defaultScale,
                             const Vec2& defaultOffset)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;

    bool changed = false;

    if (targetScale.has_value())
    {
        auto& scaleAnim = reg.get_or_emplace<components::AnimationScale>(entity);
        const auto* currentScale = reg.try_get<components::Scale>(entity);
        scaleAnim.from = currentScale != nullptr ? currentScale->value : defaultScale;
        scaleAnim.to = *targetScale;
        changed = true;
    }

    if (targetOffset.has_value())
    {
        auto& offsetAnim = reg.get_or_emplace<components::AnimationRenderOffset>(entity);
        const auto* currentOffset = reg.try_get<components::RenderOffset>(entity);
        offsetAnim.from = currentOffset != nullptr ? currentOffset->value : defaultOffset;
        offsetAnim.to = *targetOffset;
        changed = true;
    }

    if (changed)
    {
        ConfigureTiming(entity, options);
    }
}

void StopAnimation(::entt::entity entity)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    reg.remove<components::AnimatingTag>(entity);
    reg.remove<components::AnimationTime>(entity);
    reg.remove<components::AnimationPosition>(entity);
    reg.remove<components::AnimationAlpha>(entity);
    reg.remove<components::AnimationScale>(entity);
    reg.remove<components::AnimationRenderOffset>(entity);
    reg.remove<components::AnimationColor>(entity);
}

} // namespace ui::animation
