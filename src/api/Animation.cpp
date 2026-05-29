#include "Animation.hpp"
#include "singleton/Registry.hpp"
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

void ConfigureTiming(::entt::entity entity, const TweenOptions& options)
{
    auto& time = Registry::GetOrEmplace<components::AnimationTime>(entity);
    time.duration = options.duration;
    time.elapsed = 0.0F;
    time.easing = options.easing;
    time.mode = options.mode;
    time.state = policies::AnimationState::PLAYING;
    time.autoCleanup = options.autoCleanup;

    Registry::EmplaceOrReplace<components::AnimatingTag>(entity);
}

} // namespace

void StartPositionAnimation(::entt::entity entity,
                            const Vec2& startPos,
                            const Vec2& endPos,
                            const TweenOptions& options)
{
    if (!Registry::Valid(entity)) return;

    auto& posAnim = Registry::GetOrEmplace<components::AnimationPosition>(entity);
    posAnim.from = startPos;
    posAnim.to = endPos;

    ConfigureTiming(entity, options);
}

void StartAlphaAnimation(::entt::entity entity, float startAlpha, float endAlpha, const TweenOptions& options)
{
    if (!Registry::Valid(entity)) return;

    auto& alphaAnim = Registry::GetOrEmplace<components::AnimationAlpha>(entity);
    alphaAnim.from = startAlpha;
    alphaAnim.to = endAlpha;

    ConfigureTiming(entity, options);
}

void StartScaleAnimation(::entt::entity entity,
                         const Vec2& startScale,
                         const Vec2& endScale,
                         const TweenOptions& options)
{
    if (!Registry::Valid(entity)) return;

    auto& scaleAnim = Registry::GetOrEmplace<components::AnimationScale>(entity);
    scaleAnim.from = startScale;
    scaleAnim.to = endScale;

    ConfigureTiming(entity, options);
}

void StartRenderOffsetAnimation(::entt::entity entity,
                                const Vec2& startOffset,
                                const Vec2& endOffset,
                                const TweenOptions& options)
{
    if (!Registry::Valid(entity)) return;

    auto& offsetAnim = Registry::GetOrEmplace<components::AnimationRenderOffset>(entity);
    offsetAnim.from = startOffset;
    offsetAnim.to = endOffset;

    ConfigureTiming(entity, options);
}

void StartColorAnimation(::entt::entity entity,
                         const Color& startColor,
                         const Color& endColor,
                         const TweenOptions& options)
{
    if (!Registry::Valid(entity)) return;

    auto& colorAnim = Registry::GetOrEmplace<components::AnimationColor>(entity);
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
    if (!Registry::Valid(entity)) return;

    bool changed = false;

    if (targetScale.has_value())
    {
        auto& scaleAnim = Registry::GetOrEmplace<components::AnimationScale>(entity);
        const auto* currentScale = Registry::TryGet<components::Scale>(entity);
        scaleAnim.from = currentScale != nullptr ? currentScale->value : defaultScale;
        scaleAnim.to = *targetScale;
        changed = true;
    }

    if (targetOffset.has_value())
    {
        auto& offsetAnim = Registry::GetOrEmplace<components::AnimationRenderOffset>(entity);
        const auto* currentOffset = Registry::TryGet<components::RenderOffset>(entity);
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
    if (!Registry::Valid(entity)) return;
    Registry::Remove<components::AnimatingTag>(entity);
    Registry::Remove<components::AnimationTime>(entity);
    Registry::Remove<components::AnimationPosition>(entity);
    Registry::Remove<components::AnimationAlpha>(entity);
    Registry::Remove<components::AnimationScale>(entity);
    Registry::Remove<components::AnimationRenderOffset>(entity);
    Registry::Remove<components::AnimationColor>(entity);
}

} // namespace ui::animation
