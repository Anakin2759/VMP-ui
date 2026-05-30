/**
 * ************************************************************************
 *
 * @file TextRenderer.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-30
 * @version 0.1
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once
#include "interface/IRenderer.hpp"
#include "singleton/Registry.hpp"
#include "common/components/Data.hpp"
#include "common/components/Interaction.hpp"
#include "common/components/Layout.hpp"
#include "common/Tags.hpp"
#include "managers/TextTextureCache.hpp"
#include "managers/FontManager.hpp"
#include "managers/BatchManager.hpp"
#include "interface/IBackendRenderer.hpp"
#include "common/CustomizationPoints.hpp"
#include "core/TextUtils.hpp"
#include "api/Utils.hpp"
#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ui::renderers
{

/**
 *
 * - 按钮文本
 * - 标签文本
 * - 文本输入框文本及光标
 */
class TextRenderer : public core::IRenderer
{
public:
    explicit TextRenderer(Registry& reg) : m_reg(&reg) {}

    /**
     * @param entity 要检查的实体
     * @return true 如果实体可以由文本渲染器处理
     * @return false 如果实体不能由文本渲染器处理
     */
    [[nodiscard]] bool canHandle(entt::entity entity) const override
    {
        return m_reg->any_of<components::TextTag, components::ButtonTag, components::LabelTag, components::TextEditTag>(
            entity);
    }

    void collect(entt::entity entity, core::RenderContext& context) override
    {
        if (context.fontManager == nullptr || context.batchManager == nullptr)
        {
            return;
        }

        if (m_reg->any_of<components::TextTag, components::ButtonTag, components::LabelTag>(entity))
        {
            const auto* textComp = m_reg->try_get<components::Text>(entity);
            if (textComp != nullptr && !textComp->content.empty())
            {
                renderText(entity, *textComp, context);
            }
        }

        if (m_reg->any_of<components::TextEditTag>(entity))
        {
            const auto* textComp = m_reg->try_get<components::Text>(entity);
            const auto* textEdit = m_reg->try_get<components::TextEdit>(entity);
            if (textComp != nullptr && textEdit != nullptr)
            {
                renderTextEdit(entity, *textComp, *textEdit, context);
            }
        }
    }

    int getPriority() const override { return 10; }

private:
    struct FallbackTextBitmap
    {
        std::vector<uint8_t> pixels;
        uint32_t rasterWidth = 0;
        uint32_t rasterHeight = 0;
        float logicalWidth = 0.0F;
        float logicalHeight = 0.0F;
    };

    struct TextEditCursorLineInfo
    {
        int lineIndex = 0;
        float lineOffsetX = 0.0F;
    };

    struct TextEditSingleLineView
    {
        std::string visibleText;
        float cursorOffsetInVisible = 0.0F;
    };

    struct TextEditRenderArgs
    {
        entt::entity entity = entt::null;
        const components::Text* textComp = nullptr;
        const components::TextEdit* textEdit = nullptr;
        const std::string* displayText = nullptr;
        Eigen::Vector2f textPos = Eigen::Vector2f::Zero();
        Eigen::Vector2f textSize = Eigen::Vector2f::Zero();
        Eigen::Vector4f color = Eigen::Vector4f::Zero();
        float lineHeight = 0.0F;
        float fontSize = 0.0F;
        core::RenderContext* context = nullptr;
        core::RenderContext* textEditContext = nullptr;

        [[nodiscard]] const components::Text& text() const { return *textComp; }
        [[nodiscard]] const components::TextEdit& edit() const { return *textEdit; }
        [[nodiscard]] const std::string& textValue() const { return *displayText; }
        [[nodiscard]] core::RenderContext& renderContext() const { return *context; }
        [[nodiscard]] core::RenderContext& clippedContext() const { return *textEditContext; }
    };

    struct WrappedTextRenderArgs
    {
        const std::string* text = nullptr;
        Eigen::Vector2f pos = Eigen::Vector2f::Zero();
        Eigen::Vector2f size = Eigen::Vector2f::Zero();
        Eigen::Vector4f color = Eigen::Vector4f::Zero();
        policies::Alignment alignment = policies::Alignment::NONE;
        policies::TextWrap wrapMode = policies::TextWrap::NONE;
        float wrapWidth = 0.0F;
        float opacity = 0.0F;
        float fontSize = 0.0F;
        core::RenderContext* context = nullptr;

        [[nodiscard]] const std::string& textValue() const { return *text; }
        [[nodiscard]] core::RenderContext& renderContext() const { return *context; }
    };

    static uint8_t quantizeColor(float value)
    {
        return static_cast<uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
    }

    [[nodiscard]] bool isUsingFallbackBackend(const core::RenderContext& context) const
    {
        return context.backendRenderer != nullptr
            && context.backendRenderer->getType() == interface::BackendType::FALLBACK;
    }

    static std::string
        buildFallbackTextCacheKey(const std::string& text, const Eigen::Vector4f& color, float fontSize, float scale)
    {
        return text + "_" + std::to_string(static_cast<int>(fontSize * 10.0F)) + "_"
             + std::to_string(static_cast<int>(scale * 100.0F)) + "_" + std::to_string(quantizeColor(color.x())) + "_"
             + std::to_string(quantizeColor(color.y())) + "_" + std::to_string(quantizeColor(color.z())) + "_"
             + std::to_string(quantizeColor(color.w()));
    }

    const FallbackTextBitmap* getOrCreateFallbackTextBitmap(const std::string& text,
                                                            const Eigen::Vector4f& color,
                                                            float fontSize,
                                                            managers::FontManager& fontManager)
    {
        const float oversampleScale = fontManager.getOversampleScale();
        const std::string cacheKey = buildFallbackTextCacheKey(text, color, fontSize, oversampleScale);
        if (auto cacheIterator = m_fallbackTextCache.find(cacheKey); cacheIterator != m_fallbackTextCache.end())
        {
            return &cacheIterator->second;
        }

        int bitmapWidth = 0;
        int bitmapHeight = 0;
        std::vector<uint8_t> alphaMask =
            fontManager.renderTextAlphaMask(text, quantizeColor(color.w()), bitmapWidth, bitmapHeight, fontSize);
        if (alphaMask.empty() || bitmapWidth <= 0 || bitmapHeight <= 0)
        {
            return nullptr;
        }

        std::vector<uint8_t> bitmap(static_cast<size_t>(bitmapWidth) * static_cast<size_t>(bitmapHeight) * 4ULL, 0);
        const uint8_t red = quantizeColor(color.x());
        const uint8_t green = quantizeColor(color.y());
        const uint8_t blue = quantizeColor(color.z());
        const uint8_t alpha = quantizeColor(color.w());

        for (size_t pixelIndex = 0; pixelIndex < alphaMask.size(); ++pixelIndex)
        {
            const uint8_t coverage = alphaMask[pixelIndex];
            const auto finalAlpha =
                static_cast<uint8_t>((static_cast<uint16_t>(coverage) * static_cast<uint16_t>(alpha)) / 255U);
            const size_t rgbaOffset = pixelIndex * 4ULL;
            bitmap[rgbaOffset] = red;
            bitmap[rgbaOffset + 1] = green;
            bitmap[rgbaOffset + 2] = blue;
            bitmap[rgbaOffset + 3] = finalAlpha;
        }

        FallbackTextBitmap newBitmap{};
        newBitmap.pixels = std::move(bitmap);
        newBitmap.rasterWidth = static_cast<uint32_t>(bitmapWidth);
        newBitmap.rasterHeight = static_cast<uint32_t>(bitmapHeight);
        newBitmap.logicalWidth = static_cast<float>(bitmapWidth) / oversampleScale;
        newBitmap.logicalHeight = static_cast<float>(bitmapHeight) / oversampleScale;

        auto [cacheIterator, inserted] = m_fallbackTextCache.emplace(cacheKey, std::move(newBitmap));
        static_cast<void>(inserted);
        return &cacheIterator->second;
    }

    void renderText(entt::entity entity, const components::Text& textComp, core::RenderContext& context)
    {
        Eigen::Vector4f color(textComp.color.red, textComp.color.green, textComp.color.blue, textComp.color.alpha);

        float fontSize = textComp.fontSize;
        policies::TextWrap wrapMode = textComp.wordWrap;
        float wrapWidth = textComp.wrapWidth;

        if (wrapMode == policies::TextWrap::NONE)
        {
            float inferredWidth = getAncestorScrollAreaTextWidth(entity);
            if (inferredWidth > 0.0F)
            {
                wrapMode = policies::TextWrap::WORD;
                wrapWidth = inferredWidth;
            }
        }

        if (wrapMode != policies::TextWrap::NONE && wrapWidth <= 0.0F)
        {
            wrapWidth = context.size.x();
        }

        auto measureFunc = [fontManager = context.fontManager, fontSize](const std::string& str)
        { return ui::cpo::measure_text_width(*fontManager, str, fontSize); };

        if (wrapMode != policies::TextWrap::NONE && wrapWidth > 0.0F)
        {
            if (auto* sizeComp = m_reg->try_get<components::Size>(entity))
            {
                if (policies::HasFlag(sizeComp->sizePolicy, policies::Size::V_AUTO))
                {
                    const auto lineHeight = static_cast<float>(context.fontManager->getFontHeight(fontSize));
                    if (lineHeight > 0.0F)
                    {
                        const auto lines = ui::utils::WrapTextLines(
                            textComp.content, static_cast<int>(wrapWidth), wrapMode, measureFunc);
                        const auto desiredHeight = static_cast<float>(lines.size()) * lineHeight;
                        if (std::abs(sizeComp->size.y() - desiredHeight) > 0.5F)
                        {
                            sizeComp->size.y() = desiredHeight;
                            m_reg->emplace_or_replace<components::LayoutDirtyTag>(entity);
                        }
                    }
                }
            }

            addWrappedText(WrappedTextRenderArgs{&textComp.content,
                                                 context.position,
                                                 context.size,
                                                 color,
                                                 textComp.alignment,
                                                 wrapMode,
                                                 wrapWidth,
                                                 context.alpha,
                                                 fontSize,
                                                 &context});
        }
        else
        {
            addText(textComp.content,
                    context.position,
                    context.size,
                    color,
                    textComp.alignment,
                    context.alpha,
                    fontSize,
                    context);
        }
    }

    void renderTextEdit(entt::entity entity,
                        const components::Text& textComp,
                        const components::TextEdit& textEdit,
                        core::RenderContext& context)
    {
        const float fontSize = textComp.fontSize;
        Eigen::Vector2f textPos = context.position;
        Eigen::Vector2f textSize = context.size;
        core::RenderContext textEditContext = context;
        prepareTextEditLayout(entity, textPos, textSize, textEditContext);

        std::string displayText = textEdit.buffer;
        const Eigen::Vector4f color = resolveTextEditColor(entity, textEdit, context.alpha, displayText);
        const auto lineHeight = static_cast<float>(context.fontManager->getFontHeight(fontSize));
        TextEditRenderArgs textEditArgs{entity,
                                        &textComp,
                                        &textEdit,
                                        &displayText,
                                        textPos,
                                        textSize,
                                        color,
                                        lineHeight,
                                        fontSize,
                                        &context,
                                        &textEditContext};

        if (isMultilineTextEdit(textEdit))
        {
            renderMultilineTextEdit(textEditArgs);
        }
        else
        {
            renderSingleLineTextEdit(textEditArgs);
        }

        textEditContext.popScissor();
    }

    void prepareTextEditLayout(entt::entity entity,
                               Eigen::Vector2f& textPos,
                               Eigen::Vector2f& textSize,
                               core::RenderContext& textEditContext) const
    {
        if (const auto* padding = m_reg->try_get<components::Padding>(entity))
        {
            textPos.x() += padding->values.w();
            textPos.y() += padding->values.x();
            textSize.x() = std::max(0.0F, textSize.x() - padding->values.y() - padding->values.w());
            textSize.y() = std::max(0.0F, textSize.y() - padding->values.x() - padding->values.z());
        }

        SDL_Rect currentScissor{};
        currentScissor.x = static_cast<int>(textPos.x());
        currentScissor.y = static_cast<int>(textPos.y());
        currentScissor.w = static_cast<int>(textSize.x());
        currentScissor.h = static_cast<int>(textSize.y());
        textEditContext.pushScissor(currentScissor);
    }

    Eigen::Vector4f resolveTextEditColor(entt::entity entity,
                                         const components::TextEdit& textEdit,
                                         float contextAlpha,
                                         std::string& displayText) const
    {
        const auto& effectiveColor = textEdit.textColor;
        Eigen::Vector4f color(effectiveColor.red, effectiveColor.green, effectiveColor.blue, effectiveColor.alpha);

        if (displayText.empty() && !textEdit.placeholder.empty() && !m_reg->any_of<components::FocusedTag>(entity))
        {
            displayText = textEdit.placeholder;
            color = Eigen::Vector4f(0.5F, 0.5F, 0.5F, contextAlpha);
        }

        return color;
    }

    static bool isMultilineTextEdit(const components::TextEdit& textEdit)
    {
        const auto modeVal = static_cast<uint8_t>(textEdit.inputMode);
        const auto multiFlag = static_cast<uint8_t>(policies::TextFlag::MULTILINE);
        return (modeVal & multiFlag) != 0;
    }

    bool shouldRenderTextEditCaret(entt::entity entity, const core::RenderContext& context) const
    {
        if (!m_reg->any_of<components::FocusedTag>(entity))
        {
            return false;
        }

        const auto* caret = m_reg->try_get<components::Caret>(entity);
        return context.sdlWindow != nullptr && caret != nullptr && caret->visible;
    }

    void drawTextEditCaret(const Eigen::Vector2f& cursorPos,
                           float lineHeight,
                           const core::RenderContext& context,
                           const core::RenderContext& textEditContext) const
    {
        render::UiPushConstants pushConstants{};
        pushConstants.screen_size[0] = context.screenWidth;
        pushConstants.screen_size[1] = context.screenHeight;
        pushConstants.rect_size[0] = 2.0F;
        pushConstants.rect_size[1] = lineHeight;
        pushConstants.opacity = context.alpha;

        context.batchManager->beginBatch(context.whiteTexture, textEditContext.currentScissor, pushConstants);
        context.batchManager->addRect(cursorPos, {2.0F, lineHeight}, {1.0F, 1.0F, 1.0F, 1.0F});

        SDL_Rect rect{};
        rect.x = static_cast<int>(cursorPos.x());
        rect.y = static_cast<int>(cursorPos.y());
        rect.w = 2;
        rect.h = static_cast<int>(lineHeight);
        SDL_SetTextInputArea(context.sdlWindow, &rect, 0);
    }

    TextEditCursorLineInfo calculateCursorLineInfo(const std::vector<std::string>& lines,
                                                   const components::TextEdit& textEdit,
                                                   const std::string& displayText,
                                                   float fontSize,
                                                   managers::FontManager& fontManager) const
    {
        TextEditCursorLineInfo cursorInfo{};
        cursorInfo.lineIndex = static_cast<int>(lines.size()) - 1;
        cursorInfo.lineOffsetX =
            lines.empty() ? 0.0F : static_cast<float>(ui::cpo::measure_text_width(fontManager, lines.back(), fontSize));

        size_t byteOffset = 0;
        for (int lineIdx = 0; lineIdx < static_cast<int>(lines.size()); ++lineIdx)
        {
            const size_t lineByteEnd = byteOffset + lines[lineIdx].size();
            if (textEdit.cursorPosition <= lineByteEnd)
            {
                cursorInfo.lineIndex = lineIdx;
                const size_t offsetInLine = textEdit.cursorPosition - byteOffset;
                cursorInfo.lineOffsetX = static_cast<float>(
                    ui::cpo::measure_text_width(fontManager, lines[lineIdx].substr(0, offsetInLine), fontSize));
                break;
            }

            byteOffset = lineByteEnd;
            if (byteOffset < displayText.size() && displayText[byteOffset] == '\n')
            {
                byteOffset++;
            }
        }

        return cursorInfo;
    }

    TextEditSingleLineView buildSingleLineTextView(const components::TextEdit& textEdit,
                                                   const std::string& displayText,
                                                   const Eigen::Vector2f& textSize,
                                                   float fontSize,
                                                   core::RenderContext& context) const
    {
        auto measureFunc = [fontManager = context.fontManager, fontSize](const std::string& str)
        { return ui::cpo::measure_text_width(*fontManager, str, fontSize); };

        TextEditSingleLineView textView{};
        const std::string leftOfCursor = displayText.substr(0, textEdit.cursorPosition);
        const std::string visibleLeft = ui::utils::GetTailThatFits(
            leftOfCursor, static_cast<int>(textSize.x()), measureFunc, textView.cursorOffsetInVisible);

        const std::string rightOfCursor = displayText.substr(textEdit.cursorPosition);
        size_t rightCharsFit = 0;
        context.fontManager->measureString(rightOfCursor.c_str(),
                                           rightOfCursor.size(),
                                           static_cast<int>(textSize.x() - textView.cursorOffsetInVisible),
                                           &rightCharsFit,
                                           fontSize);
        textView.visibleText = visibleLeft + rightOfCursor.substr(0, rightCharsFit);
        return textView;
    }

    void renderTextEditLines(const TextEditRenderArgs& args,
                             const std::vector<std::string>& lines,
                             size_t startIndex,
                             size_t endIndex,
                             float startDrawY)
    {
        float drawY = startDrawY;
        for (size_t lineIndex = startIndex; lineIndex < endIndex; ++lineIndex)
        {
            if (!lines[lineIndex].empty())
            {
                addText(lines[lineIndex],
                        {args.textPos.x(), drawY},
                        {args.textSize.x(), args.lineHeight},
                        args.color,
                        policies::Alignment::LEFT,
                        args.renderContext().alpha,
                        args.fontSize,
                        args.clippedContext());
            }
            drawY += args.lineHeight;
        }
    }

    void renderScrollAreaTextEdit(const TextEditRenderArgs& args,
                                  components::ScrollArea& scrollArea,
                                  const std::vector<std::string>& lines,
                                  float totalTextHeight)
    {
        updateTextEditScrollArea(scrollArea, totalTextHeight, args.textSize.y(), args.textSize.x());

        const int maxVisibleLines = args.lineHeight > 0.0F ? static_cast<int>(args.textSize.y() / args.lineHeight) : 0;
        const int scrollOffsetLines =
            args.lineHeight > 0.0F ? static_cast<int>(scrollArea.scrollOffset.y() / args.lineHeight) : 0;
        const size_t startIndex =
            std::min(static_cast<size_t>(scrollOffsetLines), lines.empty() ? size_t{0} : lines.size() - size_t{1});
        const size_t endIndex = std::min(startIndex + static_cast<size_t>(maxVisibleLines) + size_t{1}, lines.size());
        const float startDrawY =
            args.textPos.y()
            - (scrollArea.scrollOffset.y() - (static_cast<float>(scrollOffsetLines) * args.lineHeight));

        renderTextEditLines(args, lines, startIndex, endIndex, startDrawY);

        if (!shouldRenderTextEditCaret(args.entity, args.renderContext()))
        {
            return;
        }

        const auto cursorInfo = calculateCursorLineInfo(
            lines, args.edit(), args.textValue(), args.fontSize, *args.renderContext().fontManager);
        if (cursorInfo.lineIndex < scrollOffsetLines || cursorInfo.lineIndex >= scrollOffsetLines + maxVisibleLines)
        {
            return;
        }

        const int visibleLineIndex = cursorInfo.lineIndex - scrollOffsetLines;
        drawTextEditCaret(
            {args.textPos.x() + cursorInfo.lineOffsetX,
             args.textPos.y() + (static_cast<float>(visibleLineIndex) * args.lineHeight)
                 - (scrollArea.scrollOffset.y() - (static_cast<float>(scrollOffsetLines) * args.lineHeight))},
            args.lineHeight,
            args.renderContext(),
            args.clippedContext());
    }

    void renderPlainMultilineTextEdit(const TextEditRenderArgs& args, const std::vector<std::string>& lines)
    {
        const int maxLines = args.lineHeight > 0.0F ? static_cast<int>(args.textSize.y() / args.lineHeight) : 0;
        const size_t startIndex = (maxLines > 0 && lines.size() > static_cast<size_t>(maxLines))
                                    ? (lines.size() - static_cast<size_t>(maxLines))
                                    : 0ULL;

        renderTextEditLines(args, lines, startIndex, lines.size(), args.textPos.y());

        if (!shouldRenderTextEditCaret(args.entity, args.renderContext()))
        {
            return;
        }

        const auto cursorInfo = calculateCursorLineInfo(
            lines, args.edit(), args.textValue(), args.fontSize, *args.renderContext().fontManager);
        if (cursorInfo.lineIndex < static_cast<int>(startIndex))
        {
            return;
        }

        const int relativeLine = cursorInfo.lineIndex - static_cast<int>(startIndex);
        const float cursorY = args.textPos.y() + (static_cast<float>(relativeLine) * args.lineHeight);
        drawTextEditCaret({args.textPos.x() + cursorInfo.lineOffsetX, cursorY},
                          args.lineHeight,
                          args.renderContext(),
                          args.clippedContext());
    }

    void renderSingleLineTextEdit(const TextEditRenderArgs& args)
    {
        const auto textView =
            buildSingleLineTextView(args.edit(), args.textValue(), args.textSize, args.fontSize, args.renderContext());

        const policies::Alignment align = policies::Alignment::LEFT | policies::Alignment::VCENTER;
        if (!textView.visibleText.empty())
        {
            addText(textView.visibleText,
                    args.textPos,
                    args.textSize,
                    args.color,
                    align,
                    args.renderContext().alpha,
                    args.fontSize,
                    args.clippedContext());
        }

        if (!shouldRenderTextEditCaret(args.entity, args.renderContext()))
        {
            return;
        }

        drawTextEditCaret({args.textPos.x() + textView.cursorOffsetInVisible,
                           args.textPos.y() + ((args.textSize.y() - args.lineHeight) * 0.5F)},
                          args.lineHeight,
                          args.renderContext(),
                          args.clippedContext());
    }

    void updateTextEditScrollArea(components::ScrollArea& scrollArea,
                                  float totalTextHeight,
                                  float viewportHeight,
                                  float contentWidth) const
    {
        if (scrollArea.contentSize.y() != totalTextHeight)
        {
            const float oldHeight = scrollArea.contentSize.y();
            const float newHeight = totalTextHeight;

            scrollArea.contentSize.x() = contentWidth;
            scrollArea.contentSize.y() = totalTextHeight;

            if (scrollArea.anchor == policies::ScrollAnchor::BOTTOM)
            {
                scrollArea.scrollOffset.y() += (newHeight - oldHeight);
            }
            else if (scrollArea.anchor == policies::ScrollAnchor::SMART)
            {
                const float oldMaxScroll = std::max(0.0F, oldHeight - viewportHeight);
                const bool wasAtBottom = (scrollArea.scrollOffset.y() >= oldMaxScroll - 2.0F);

                if (wasAtBottom)
                {
                    const float newMaxScroll = std::max(0.0F, newHeight - viewportHeight);
                    scrollArea.scrollOffset.y() = newMaxScroll;
                }
            }
        }

        scrollArea.contentSize.x() = contentWidth;
        const float maxScroll = std::max(0.0F, totalTextHeight - viewportHeight);
        scrollArea.scrollOffset.y() = std::clamp(scrollArea.scrollOffset.y(), 0.0F, maxScroll);
    }

    void renderMultilineTextEdit(const TextEditRenderArgs& args)
    {
        policies::TextWrap wrapMode =
            args.text().wordWrap != policies::TextWrap::NONE ? args.text().wordWrap : policies::TextWrap::WORD;
        auto measureFunc =
            [fontManager = args.renderContext().fontManager, fontSize = args.fontSize](const std::string& str)
        { return ui::cpo::measure_text_width(*fontManager, str, fontSize); };
        const std::vector<std::string> lines =
            ui::utils::WrapTextLines(args.textValue(), static_cast<int>(args.textSize.x()), wrapMode, measureFunc);
        const float totalTextHeight = static_cast<float>(lines.size()) * args.lineHeight;

        if (auto* scrollArea = m_reg->try_get<components::ScrollArea>(args.entity))
        {
            renderScrollAreaTextEdit(args, *scrollArea, lines, totalTextHeight);
            return;
        }

        renderPlainMultilineTextEdit(args, lines);
    }
    /**
     * @brief 获取
     * @param entity {comment}
     * @return float {comment}
     */
    [[nodiscard]] float getAncestorScrollAreaTextWidth(entt::entity entity) const
    {
        entt::entity current = entity;
        while (current != entt::null)
        {
            const auto* hierarchy = m_reg->try_get<components::Hierarchy>(current);
            if (hierarchy == nullptr) break;

            current = hierarchy->parent;
            if (current == entt::null) break;

            if (m_reg->any_of<components::ScrollArea>(current))
            {
                const auto* size = m_reg->try_get<components::Size>(current);
                if (size == nullptr) return 0.0F;

                float width = size->size.x();
                if (const auto* padding = m_reg->try_get<components::Padding>(current))
                {
                    width -= (padding->values.y() + padding->values.w());
                }
                return std::max(0.0F, width);
            }
        }
        return 0.0F;
    }

    void addText(const std::string& text,
                 const Eigen::Vector2f& pos,
                 const Eigen::Vector2f& size,
                 const Eigen::Vector4f& color,
                 policies::Alignment alignment,
                 float opacity,
                 float fontSize,
                 core::RenderContext& context)
    {
        if (!context.fontManager->isLoaded() || text.empty()) return;

        const bool useFallbackText = isUsingFallbackBackend(context);

        uint32_t textWidth = 0;
        uint32_t textHeight = 0;

        SDL_GPUTexture* textTexture = nullptr;
        Eigen::Vector2f textSize(0.0F, 0.0F);
        std::string fallbackCacheKey;
        const FallbackTextBitmap* fallbackBitmap = nullptr;

        if (useFallbackText)
        {
            fallbackCacheKey =
                buildFallbackTextCacheKey(text, color, fontSize, context.fontManager->getOversampleScale());
            fallbackBitmap = getOrCreateFallbackTextBitmap(text, color, fontSize, *context.fontManager);
            if (fallbackBitmap == nullptr)
            {
                return;
            }

            textWidth = fallbackBitmap->rasterWidth;
            textHeight = fallbackBitmap->rasterHeight;
            textSize = {fallbackBitmap->logicalWidth, fallbackBitmap->logicalHeight};
        }
        else
        {
            if (context.textTextureCache == nullptr)
            {
                return;
            }

            textTexture = context.textTextureCache->getOrUpload(text, color, textWidth, textHeight, fontSize);
            if (textTexture == nullptr)
            {
                return;
            }

            const float scale = context.fontManager->getOversampleScale();
            textSize = {static_cast<float>(textWidth) / scale, static_cast<float>(textHeight) / scale};
        }

        float drawX = pos.x();
        float drawY = pos.y();

        // 水平对齐
        if (ui::utils::HasAlignment(alignment, policies::Alignment::HCENTER))
        {
            drawX += (size.x() - textSize.x()) * 0.5F;
        }
        else if (ui::utils::HasAlignment(alignment, policies::Alignment::RIGHT))
        {
            drawX += size.x() - textSize.x();
        }

        // 垂直对齐
        if (ui::utils::HasAlignment(alignment, policies::Alignment::VCENTER))
        {
            drawY += (size.y() - textSize.y()) * 0.5F;
        }
        else if (ui::utils::HasAlignment(alignment, policies::Alignment::BOTTOM))
        {
            drawY += size.y() - textSize.y();
        }

        drawX = std::round(drawX);
        drawY = std::round(drawY);

        if (useFallbackText)
        {
            const SDL_FRect destinationRect = {drawX, drawY, textSize.x(), textSize.y()};
            const uint8_t alphaMod = static_cast<uint8_t>(std::lround(std::clamp(opacity, 0.0F, 1.0F) * 255.0F));
            if (!ui::cpo::backend_supports(*context.backendRenderer, interface::BackendCapability::CACHED_BITMAP))
            {
                return;
            }

            if (!context.backendRenderer->drawCachedBitmap(fallbackCacheKey,
                                                           fallbackBitmap->pixels,
                                                           static_cast<int>(fallbackBitmap->rasterWidth),
                                                           static_cast<int>(fallbackBitmap->rasterHeight),
                                                           destinationRect,
                                                           context.currentScissor,
                                                           alphaMod))
            {
                return;
            }
            return;
        }

        render::UiPushConstants pushConstants{};
        pushConstants.screen_size[0] = context.screenWidth;
        pushConstants.screen_size[1] = context.screenHeight;
        pushConstants.rect_size[0] = textSize.x();
        pushConstants.rect_size[1] = textSize.y();
        pushConstants.opacity = opacity;
        pushConstants.padding = 2.0F;

        context.batchManager->beginBatch(textTexture, context.currentScissor, pushConstants);
        context.batchManager->addRect({drawX, drawY}, textSize, color);
    }

    [[nodiscard]] float resolveWrappedTextStartY(const WrappedTextRenderArgs& args, float totalHeight) const
    {
        float startY = args.pos.y();
        if (ui::utils::HasAlignment(args.alignment, policies::Alignment::VCENTER))
        {
            startY += (args.size.y() - totalHeight) * 0.5F;
        }
        else if (ui::utils::HasAlignment(args.alignment, policies::Alignment::BOTTOM))
        {
            startY += args.size.y() - totalHeight;
        }

        return startY;
    }

    [[nodiscard]] static policies::Alignment resolveWrappedTextHorizontalAlignment(policies::Alignment alignment)
    {
        const uint8_t horizontalMask = static_cast<uint8_t>(policies::Alignment::LEFT)
                                     | static_cast<uint8_t>(policies::Alignment::HCENTER)
                                     | static_cast<uint8_t>(policies::Alignment::RIGHT);
        const auto alignValue = static_cast<uint8_t>(alignment);
        auto horizontalAlign = static_cast<policies::Alignment>(alignValue & horizontalMask);
        if (horizontalAlign == policies::Alignment::NONE)
        {
            horizontalAlign = policies::Alignment::LEFT;
        }

        return horizontalAlign;
    }

    void renderWrappedTextLines(const WrappedTextRenderArgs& args,
                                const std::vector<std::string>& lines,
                                float lineHeight,
                                float startY,
                                policies::Alignment horizontalAlign)
    {
        float drawY = startY;
        for (const auto& line : lines)
        {
            if (!line.empty())
            {
                addText(line,
                        {args.pos.x(), drawY},
                        {args.wrapWidth, lineHeight},
                        args.color,
                        horizontalAlign,
                        args.opacity,
                        args.fontSize,
                        args.renderContext());
            }
            drawY += lineHeight;
        }
    }

    void addWrappedText(const WrappedTextRenderArgs& args)
    {
        if (!args.renderContext().fontManager->isLoaded() || args.textValue().empty() || args.wrapWidth <= 0.0F) return;

        const auto lineHeight = static_cast<float>(args.renderContext().fontManager->getFontHeight(args.fontSize));
        if (lineHeight <= 0.0F) return;

        auto measureFunc =
            [fontManager = args.renderContext().fontManager, fontSize = args.fontSize](const std::string& str)
        { return ui::cpo::measure_text_width(*fontManager, str, fontSize); };

        std::vector<std::string> lines =
            ui::utils::WrapTextLines(args.textValue(), static_cast<int>(args.wrapWidth), args.wrapMode, measureFunc);
        const float totalHeight = static_cast<float>(lines.size()) * lineHeight;

        const float startY = resolveWrappedTextStartY(args, totalHeight);
        const auto horizontalAlign = resolveWrappedTextHorizontalAlignment(args.alignment);
        renderWrappedTextLines(args, lines, lineHeight, startY, horizontalAlign);
    }

    Registry* m_reg = nullptr;
    std::unordered_map<std::string, FallbackTextBitmap> m_fallbackTextCache;
};

} // namespace ui::renderers
