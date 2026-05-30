#include "Icon.hpp"
#include "Scale.hpp"
#include <string>
#include <cstdint>
#include <unordered_set>
#include "Utils.hpp"
#include "core/RuntimeFacade.hpp"
#include "common/Policies.hpp"
#include "entt/entity/fwd.hpp"
#include "common/components/Data.hpp"
namespace ui::icon
{
namespace
{
[[nodiscard]] entt::registry& CurrentRegistry()
{
    return RuntimeFacade::current().enttRegistry();
}
} // namespace

void SetIcon(
    entt::entity entity, const std::string& textureId, policies::IconFlag iconflag, float iconSize, float spacing)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    (void)iconflag;

    auto& icon = reg.get_or_emplace<components::Icon>(entity);
    icon.type |= policies::IconFlag::TEXTURE;
    icon.textureId = textureId;
    icon.fontHandle = nullptr;
    icon.codepoint = 0;

    icon.size = {scale::Metric(iconSize), scale::Metric(iconSize)};
    icon.spacing = scale::Metric(spacing);

    ui::utils::MarkLayoutAndVisualChanged(entity);
}

void SetIcon(entt::entity entity,
             const std::string& fontName,
             uint32_t codepoint,
             policies::IconFlag iconflag,
             float iconSize,
             float spacing)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    (void)iconflag;

    auto& icon = reg.get_or_emplace<components::Icon>(entity);
    icon.type |= ~policies::IconFlag::TEXTURE;
    // 暂时将字体名转换为 const char* 存储在 fontHandle 中，IconRenderer 会读取它
    // 理想情况下应该重构 Icon 组件，但为了保持兼容性暂且如此
    static std::unordered_set<std::string> fontNamePool;
    auto [fontIterator, wasInserted] = fontNamePool.insert(fontName);
    (void)wasInserted;
    icon.fontHandle = fontIterator->c_str();

    icon.codepoint = codepoint;
    icon.textureId = "";
    icon.size = {scale::Metric(iconSize), scale::Metric(iconSize)};
    icon.spacing = scale::Metric(spacing);

    ui::utils::MarkLayoutAndVisualChanged(entity);
}

void RemoveIcon(entt::entity entity)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    if (reg.any_of<components::Icon>(entity))
    {
        reg.remove<components::Icon>(entity);
        ui::utils::MarkLayoutAndVisualChanged(entity);
    }
}
} // namespace ui::icon