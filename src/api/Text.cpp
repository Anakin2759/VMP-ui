#include "Text.hpp"
#include "../singleton/Registry.hpp"
#include "../common/Components.hpp"
#include "../common/Tags.hpp"
#include "../api/Utils.hpp"
namespace ui::text
{
void SetText(::entt::entity entity, const std::string& content)
{
    if (!Registry::Valid(entity)) return;
    if (Registry::AnyOf<components::Text>(entity))
    {
        auto& text = Registry::GetOrEmplace<components::Text>(entity);
        text.content = content;
        utils::MarkLayoutDirty(entity);
    }
}

void SetButtonEnabled(::entt::entity entity, bool enabled)
{
    if (!Registry::Valid(entity)) return;
    if (enabled)
    {
        Registry::Remove<components::DisabledTag>(entity);
    }
    else
    {
        Registry::EmplaceOrReplace<components::DisabledTag>(entity);
    }
}

void SetTextContent(::entt::entity entity, const std::string& content)
{
    if (!Registry::Valid(entity)) return;
    auto& text = Registry::GetOrEmplace<components::Text>(entity);
    text.content = content;
    utils::MarkLayoutDirty(entity);
}

void SetTextWordWrap(::entt::entity entity, policies::TextWrap mode)
{
    if (!Registry::Valid(entity)) return;
    auto& text = Registry::GetOrEmplace<components::Text>(entity);
    text.wordWrap = mode;
    utils::MarkLayoutDirty(entity);
}

void SetTextAlignment(::entt::entity entity, policies::Alignment alignment)
{
    if (!Registry::Valid(entity)) return;
    auto& text = Registry::GetOrEmplace<components::Text>(entity);
    text.alignment = alignment;
    utils::MarkLayoutDirty(entity);
}

void SetTextColor(::entt::entity entity, const Color& color)
{
    if (!Registry::Valid(entity)) return;
    if (auto* textComp = Registry::TryGet<components::Text>(entity)) textComp->color = color;
    if (auto* textEdit = Registry::TryGet<components::TextEdit>(entity)) textEdit->textColor = color;
}

std::string GetTextEditContent(::entt::entity entity)
{
    if (!Registry::Valid(entity)) return "";
    if (auto* textEdit = Registry::TryGet<components::TextEdit>(entity)) return textEdit->buffer;
    return "";
}
/**
 * @brief 设置文本编辑框内容
 * @param entity {comment}
 * @param content {comment}
 */
void SetTextEditContent(::entt::entity entity, const std::string& content)
{
    if (!Registry::Valid(entity)) return;
    if (auto* textEdit = Registry::TryGet<components::TextEdit>(entity))
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
    if (!Registry::Valid(entity)) return;
    if (auto* textEdit = Registry::TryGet<components::TextEdit>(entity)) textEdit->inputMode |= enabled;
}

void SetClickCallback(::entt::entity entity, components::on_event<> callback)
{
    if (!Registry::Valid(entity)) return;
    auto& clickable = Registry::GetOrEmplace<components::Clickable>(entity);
    clickable.onClick = std::move(callback);
    clickable.enabled = policies::Feature::Enabled;
}

void SetOnSubmit(::entt::entity entity, components::on_event<> callback)
{
    if (!Registry::Valid(entity)) return;
    auto* textEdit = Registry::TryGet<components::TextEdit>(entity);
    if (textEdit != nullptr)
    {
        textEdit->onSubmit = std::move(callback);
    }
}

void SetOnTextChanged(::entt::entity entity, components::on_event<const std::string&> callback)
{
    if (!Registry::Valid(entity)) return;
    auto* textEdit = Registry::TryGet<components::TextEdit>(entity);
    if (textEdit != nullptr)
    {
        textEdit->onTextChanged = std::move(callback);
    }
}

void SetLineHeight(::entt::entity entity, float height)
{
    if (!Registry::Valid(entity)) return;
    auto& text = Registry::GetOrEmplace<components::Text>(entity);
    text.lineHeight = height;
    utils::MarkLayoutDirty(entity);
}

void SetCharacterSpacing(::entt::entity entity, float spacing)
{
    if (!Registry::Valid(entity)) return;
    auto& text = Registry::GetOrEmplace<components::Text>(entity);
    text.letterSpacing = spacing;
    utils::MarkLayoutDirty(entity);
}

void SetTextWrapWidth(::entt::entity entity, float width)
{
    if (!Registry::Valid(entity)) return;
    auto& text = Registry::GetOrEmplace<components::Text>(entity);
    text.wrapWidth = width;
    utils::MarkLayoutDirty(entity);
}

void SetFontSize(::entt::entity entity, float size)
{
    if (!Registry::Valid(entity)) return;
    auto& text = Registry::GetOrEmplace<components::Text>(entity);
    text.fontSize = size;
    utils::MarkLayoutDirty(entity);
}

} // namespace ui::text
