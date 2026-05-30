#include "Text.hpp"
#include "core/RuntimeFacade.hpp"
#include "common/Tags.hpp"
#include "api/Utils.hpp"
#include "entt/entity/fwd.hpp"
#include <string>
#include "common/components/Data.hpp"
#include "common/Policies.hpp"
#include "common/Types.hpp"
#include <algorithm>
#include "common/components/Interaction.hpp"
#include <utility>
namespace ui::text
{
namespace
{
[[nodiscard]] entt::registry& CurrentRegistry()
{
    return RuntimeFacade::current().enttRegistry();
}
} // namespace

void SetText(::entt::entity entity, const std::string& content)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    if (reg.any_of<components::Text>(entity))
    {
        auto& text = reg.get_or_emplace<components::Text>(entity);
        text.content = content;
        utils::MarkLayoutDirty(entity);
    }
}

void SetButtonEnabled(::entt::entity entity, bool enabled)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    if (enabled)
    {
        reg.remove<components::DisabledTag>(entity);
    }
    else
    {
        reg.emplace_or_replace<components::DisabledTag>(entity);
    }
}

void SetTextContent(::entt::entity entity, const std::string& content)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    auto& text = reg.get_or_emplace<components::Text>(entity);
    text.content = content;
    utils::MarkLayoutDirty(entity);
}

void SetTextWordWrap(::entt::entity entity, policies::TextWrap mode)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    auto& text = reg.get_or_emplace<components::Text>(entity);
    text.wordWrap = mode;
    utils::MarkLayoutDirty(entity);
}

void SetTextAlignment(::entt::entity entity, policies::Alignment alignment)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    auto& text = reg.get_or_emplace<components::Text>(entity);
    text.alignment = alignment;
    utils::MarkLayoutDirty(entity);
}

void SetTextColor(::entt::entity entity, const Color& color)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    if (auto* textComp = reg.try_get<components::Text>(entity)) textComp->color = color;
    if (auto* textEdit = reg.try_get<components::TextEdit>(entity)) textEdit->textColor = color;
}

std::string GetTextEditContent(::entt::entity entity)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return "";
    if (auto* textEdit = reg.try_get<components::TextEdit>(entity)) return textEdit->buffer;
    return "";
}
/**
 * @brief 设置文本编辑框内容
 * @param entity {comment}
 * @param content {comment}
 */
void SetTextEditContent(::entt::entity entity, const std::string& content)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    if (auto* textEdit = reg.try_get<components::TextEdit>(entity))
    {
        textEdit->buffer = content;
        textEdit->cursorPosition = std::min(textEdit->cursorPosition, content.size());
        textEdit->hasSelection = false;
        textEdit->selectionStart = 0;
        textEdit->selectionEnd = 0;
    }
}

void SetPasswordMode(::entt::entity entity, policies::TextFlag enabled)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    if (auto* textEdit = reg.try_get<components::TextEdit>(entity)) textEdit->inputMode |= enabled;
}

void SetClickCallback(::entt::entity entity, components::on_event<> callback)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    auto& clickable = reg.get_or_emplace<components::Clickable>(entity);
    clickable.onClick = std::move(callback);
    clickable.enabled = policies::Feature::ENABLED;
}

void SetOnSubmit(::entt::entity entity, components::on_event<> callback)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    auto* textEdit = reg.try_get<components::TextEdit>(entity);
    if (textEdit != nullptr)
    {
        textEdit->onSubmit = std::move(callback);
    }
}

void SetOnTextChanged(::entt::entity entity, components::on_event<const std::string&> callback)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    auto* textEdit = reg.try_get<components::TextEdit>(entity);
    if (textEdit != nullptr)
    {
        textEdit->onTextChanged = std::move(callback);
    }
}

void SetLineHeight(::entt::entity entity, float height)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    auto& text = reg.get_or_emplace<components::Text>(entity);
    text.lineHeight = height;
    utils::MarkLayoutDirty(entity);
}

void SetCharacterSpacing(::entt::entity entity, float spacing)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    auto& text = reg.get_or_emplace<components::Text>(entity);
    text.letterSpacing = spacing;
    utils::MarkLayoutDirty(entity);
}

void SetTextWrapWidth(::entt::entity entity, float width)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    auto& text = reg.get_or_emplace<components::Text>(entity);
    text.wrapWidth = width;
    utils::MarkLayoutDirty(entity);
}

void SetFontSize(::entt::entity entity, float size)
{
    auto& reg = CurrentRegistry();
    if (!reg.valid(entity)) return;
    auto& text = reg.get_or_emplace<components::Text>(entity);
    text.fontSize = size;
    utils::MarkLayoutDirty(entity);
}

} // namespace ui::text
