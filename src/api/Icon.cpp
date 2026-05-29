#include "Icon.hpp"
#include <string>
#include <cstdint>
#include <unordered_set>
#include "Utils.hpp"
#include "singleton/Registry.hpp"
#include "common/Policies.hpp"
#include "entt/entity/fwd.hpp"
#include "common/components/Data.hpp"
namespace ui::icon
{
void SetIcon(
    entt::entity entity, const std::string& textureId, policies::IconFlag iconflag, float iconSize, float spacing)
{
    if (!Registry::Valid(entity)) return;
    (void)iconflag;

    auto& icon = Registry::GetOrEmplace<components::Icon>(entity);
    icon.type |= policies::IconFlag::TEXTURE;
    icon.textureId = textureId;
    icon.fontHandle = nullptr;
    icon.codepoint = 0;

    icon.size = {iconSize, iconSize};
    icon.spacing = spacing;

    ui::utils::MarkLayoutAndVisualChanged(entity);
}

void SetIcon(entt::entity entity,
             const std::string& fontName,
             uint32_t codepoint,
             policies::IconFlag iconflag,
             float iconSize,
             float spacing)
{
    if (!Registry::Valid(entity)) return;
    (void)iconflag;

    auto& icon = Registry::GetOrEmplace<components::Icon>(entity);
    icon.type |= ~policies::IconFlag::TEXTURE;
    // 暂时将字体名转换为 const char* 存储在 fontHandle 中，IconRenderer 会读取它
    // 理想情况下应该重构 Icon 组件，但为了保持兼容性暂且如此
    static std::unordered_set<std::string> fontNamePool;
    auto [fontIterator, wasInserted] = fontNamePool.insert(fontName);
    (void)wasInserted;
    icon.fontHandle = fontIterator->c_str();

    icon.codepoint = codepoint;
    icon.textureId = "";
    icon.size = {iconSize, iconSize};
    icon.spacing = spacing;

    ui::utils::MarkLayoutAndVisualChanged(entity);
}

void RemoveIcon(entt::entity entity)
{
    if (!Registry::Valid(entity)) return;
    if (Registry::AnyOf<components::Icon>(entity))
    {
        Registry::Remove<components::Icon>(entity);
        ui::utils::MarkLayoutAndVisualChanged(entity);
    }
}
} // namespace ui::icon