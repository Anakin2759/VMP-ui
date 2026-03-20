/**
 * ************************************************************************
 *
 * @file TextRenderer.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-01-30
 * @version 0.1
 * @brief 文本渲染器 - 处理所有文本的渲染
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */

#pragma once
#include "../interface/IRenderer.hpp"
#include "../singleton/Registry.hpp"
#include "../common/Components.hpp"
#include "../common/Tags.hpp"
#include "../managers/TextTextureCache.hpp"
#include "../managers/FontManager.hpp"
#include "../managers/BatchManager.hpp"
#include "../interface/IBackendRenderer.hpp"
#include "../core/TextUtils.hpp"
#include "../api/Utils.hpp"
#include <cstdint>
#include <unordered_map>
#include <vector>

namespace ui::renderers
{

/**
 * @brief 文本渲染器
 *
 * 负责渲染：
 * - 普通文本
 * - 按钮文本
 * - 标签文本
 * - 文本输入框文本及光标
 */
class TextRenderer : public core::IRenderer
{
public:
    TextRenderer() = default;

    [[nodiscard]] bool canHandle(entt::entity entity) const override
    {
        return Registry::
            AnyOf<components::TextTag, components::ButtonTag, components::LabelTag, components::TextEditTag>(entity);
    }

    void collect(entt::entity entity, core::RenderContext& context) override
    {
        if (context.fontManager == nullptr || context.batchManager == nullptr)
        {
            return;
        }

        // 文本渲染 (普通文本、按钮、标签)
        if (Registry::AnyOf<components::TextTag, components::ButtonTag, components::LabelTag>(entity))
        {
            const auto* textComp = Registry::TryGet<components::Text>(entity);
            if (textComp != nullptr && !textComp->content.empty())
            {
                renderText(entity, *textComp, context);
            }
        }

        // 文本输入框渲染
        if (Registry::AnyOf<components::TextEditTag>(entity))
        {
            const auto* textComp = Registry::TryGet<components::Text>(entity);
            const auto* textEdit = Registry::TryGet<components::TextEdit>(entity);
            if (textComp && textEdit)
            {
                renderTextEdit(entity, *textComp, *textEdit, context);
            }
        }
    }

    int getPriority() const override
    {
        return 10; // 文本在背景之后渲染
    }

private:
    struct FallbackTextBitmap
    {
        std::vector<uint8_t> pixels;
        uint32_t rasterWidth = 0;
        uint32_t rasterHeight = 0;
        float logicalWidth = 0.0F;
        float logicalHeight = 0.0F;
    };

    static uint8_t quantizeColor(float value)
    {
        return static_cast<uint8_t>(std::lround(std::clamp(value, 0.0F, 1.0F) * 255.0F));
    }

    [[nodiscard]] bool isUsingFallbackBackend(const core::RenderContext& context) const
    {
        return context.backendRenderer != nullptr &&
               context.backendRenderer->getType() == interface::BackendType::FALLBACK;
    }

    static std::string buildFallbackTextCacheKey(const std::string& text, const Eigen::Vector4f& color, float fontSize)
    {
        return text + "_" + std::to_string(static_cast<int>(fontSize * 10.0F)) + "_" +
               std::to_string(quantizeColor(color.x())) + "_" + std::to_string(quantizeColor(color.y())) + "_" +
               std::to_string(quantizeColor(color.z())) + "_" + std::to_string(quantizeColor(color.w()));
    }

    const FallbackTextBitmap* getOrCreateFallbackTextBitmap(const std::string& text,
                                                            const Eigen::Vector4f& color,
                                                            float fontSize,
                                                            managers::FontManager& fontManager)
    {
        const std::string cacheKey = buildFallbackTextCacheKey(text, color, fontSize);
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
            const uint8_t finalAlpha =
                static_cast<uint8_t>((static_cast<uint16_t>(coverage) * static_cast<uint16_t>(alpha)) / 255U);
            const size_t rgbaOffset = pixelIndex * 4ULL;
            bitmap[rgbaOffset] = red;
            bitmap[rgbaOffset + 1] = green;
            bitmap[rgbaOffset + 2] = blue;
            bitmap[rgbaOffset + 3] = finalAlpha;
        }

        const float oversampleScale = fontManager.getOversampleScale();

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

        // 获取字体大小（0 表示使用默认值）
        float fontSize = textComp.fontSize;

        policies::TextWrap wrapMode = textComp.wordWrap;
        float wrapWidth = textComp.wrapWidth;

        if (wrapMode == policies::TextWrap::NONE)
        {
            // 如果在 ScrollArea 内，默认启用换行
            float inferredWidth = getAncestorScrollAreaTextWidth(entity);
            if (inferredWidth > 0.0F)
            {
                wrapMode = policies::TextWrap::Word;
                wrapWidth = inferredWidth;
            }
        }

        if (wrapMode != policies::TextWrap::NONE && wrapWidth <= 0.0F)
        {
            wrapWidth = context.size.x();
        }

        auto measureFunc = [fontManager = context.fontManager, fontSize](const std::string& str)
        { return fontManager->measureTextWidth(str, fontSize); };

        if (wrapMode != policies::TextWrap::NONE && wrapWidth > 0.0F)
        {
            // 根据换行结果动态修正自动高度，避免滚动区内容高度不匹配
            if (auto* sizeComp = Registry::TryGet<components::Size>(entity))
            {
                if (policies::HasFlag(sizeComp->sizePolicy, policies::Size::VAuto))
                {
                    const float lineHeight = static_cast<float>(context.fontManager->getFontHeight(fontSize));
                    if (lineHeight > 0.0F)
                    {
                        const auto lines = ui::utils::WrapTextLines(
                            textComp.content, static_cast<int>(wrapWidth), wrapMode, measureFunc);
                        const float desiredHeight = static_cast<float>(lines.size()) * lineHeight;
                        if (std::abs(sizeComp->size.y() - desiredHeight) > 0.5F)
                        {
                            sizeComp->size.y() = desiredHeight;
                            Registry::EmplaceOrReplace<components::LayoutDirtyTag>(entity);
                        }
                    }
                }
            }

            addWrappedText(textComp.content,
                           context.position,
                           context.size,
                           color,
                           textComp.alignment,
                           wrapMode,
                           wrapWidth,
                           context.alpha,
                           fontSize,
                           context);
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
        // 获取字体大小
        float fontSize = textComp.fontSize;

        // 计算文本区域（考虑内边距）
        Eigen::Vector2f textPos = context.position;
        Eigen::Vector2f textSize = context.size;
        if (const auto* padding = Registry::TryGet<components::Padding>(entity))
        {
            textPos.x() += padding->values.w();
            textPos.y() += padding->values.x();
            textSize.x() = std::max(0.0F, textSize.x() - padding->values.y() - padding->values.w());
            textSize.y() = std::max(0.0F, textSize.y() - padding->values.x() - padding->values.z());
        }

        // 在输入框内部裁剪
        core::RenderContext textEditContext = context;
        SDL_Rect currentScissor;
        currentScissor.x = static_cast<int>(textPos.x());
        currentScissor.y = static_cast<int>(textPos.y());
        currentScissor.w = static_cast<int>(textSize.x());
        currentScissor.h = static_cast<int>(textSize.y());
        textEditContext.pushScissor(currentScissor);

        std::string displayText = textEdit.buffer;
        // 优先使用 TextEdit 自身的 textColor，保持与 SetTextColor API 的一致性
        const auto& effectiveColor = textEdit.textColor;
        Eigen::Vector4f color(effectiveColor.red, effectiveColor.green, effectiveColor.blue, effectiveColor.alpha);

        // 如果没有内容且有 placeholder，显示 placeholder（灰色）
        // 在获得焦点（点击）时不再显示
        if (displayText.empty() && !textEdit.placeholder.empty() && !Registry::AnyOf<components::FocusedTag>(entity))
        {
            displayText = textEdit.placeholder;
            color = Eigen::Vector4f(0.5F, 0.5F, 0.5F, context.alpha);
        }

        const float lineHeight = static_cast<float>(context.fontManager->getFontHeight(fontSize));

        const auto modeVal = static_cast<uint8_t>(textEdit.inputMode);
        const auto multiFlag = static_cast<uint8_t>(policies::TextFlag::Multiline);

        auto measureFunc = [fontManager = context.fontManager, fontSize](const std::string& str)
        { return fontManager->measureTextWidth(str, fontSize); };

        if ((modeVal & multiFlag) == 0)
        {
            // 单行：确保光标所在的区域可见
            std::string leftOfCursor = displayText.substr(0, textEdit.cursorPosition);
            float cursorOffsetInVisible = 0.0F;
            std::string visibleLeft = ui::utils::GetTailThatFits(
                leftOfCursor, static_cast<int>(textSize.x()), measureFunc, cursorOffsetInVisible);

            std::string rightOfCursor = displayText.substr(textEdit.cursorPosition);
            size_t rightCharsFit = 0;
            context.fontManager->measureString(rightOfCursor.c_str(),
                                               rightOfCursor.size(),
                                               static_cast<int>(textSize.x() - cursorOffsetInVisible),
                                               &rightCharsFit,
                                               fontSize);
            std::string visibleRight = rightOfCursor.substr(0, rightCharsFit);

            std::string visibleText = visibleLeft + visibleRight;

            const policies::Alignment align = policies::Alignment::LEFT | policies::Alignment::VCENTER;
            if (!visibleText.empty())
            {
                addText(visibleText, textPos, textSize, color, align, context.alpha, fontSize, textEditContext);
            }

            // 绘制光标 (仅当获焦时)
            if (Registry::AnyOf<components::FocusedTag>(entity))
            {
                const auto* caret = Registry::TryGet<components::Caret>(entity);
                if (context.sdlWindow && caret != nullptr && caret->visible)
                {
                    const float cursorX = textPos.x() + cursorOffsetInVisible;
                    const float cursorY = textPos.y() + (textSize.y() - lineHeight) * 0.5F;

                    render::UiPushConstants pushConstants{};
                    pushConstants.screen_size[0] = context.screenWidth;
                    pushConstants.screen_size[1] = context.screenHeight;
                    pushConstants.rect_size[0] = 2.0F;
                    pushConstants.rect_size[1] = lineHeight;
                    pushConstants.opacity = context.alpha;

                    context.batchManager->beginBatch(
                        context.whiteTexture, textEditContext.currentScissor, pushConstants);
                    context.batchManager->addRect({cursorX, cursorY}, {2.0F, lineHeight}, {1.0F, 1.0F, 1.0F, 1.0F});

                    SDL_Rect rect;
                    rect.x = static_cast<int>(cursorX);
                    rect.y = static_cast<int>(cursorY);
                    rect.w = 2;
                    rect.h = static_cast<int>(lineHeight);
                    SDL_SetTextInputArea(context.sdlWindow, &rect, 0);
                }
            }
        }
        else
        {
            // 多行：自动换行 + 支持滚动
            policies::TextWrap wrapMode =
                textComp.wordWrap != policies::TextWrap::NONE ? textComp.wordWrap : policies::TextWrap::Word;
            std::vector<std::string> lines =
                ui::utils::WrapTextLines(displayText, static_cast<int>(textSize.x()), wrapMode, measureFunc);

            // 计算文本总高度并更新 ScrollArea contentSize
            float totalTextHeight = lines.size() * lineHeight;
            if (auto* scrollArea = Registry::TryGet<components::ScrollArea>(entity))
            {
                // 视口高度即为当前的 textSize.y() (已去除 Padding)
                float viewportHeight = textSize.y();

                if (scrollArea->contentSize.y() != totalTextHeight)
                {
                    float oldHeight = scrollArea->contentSize.y();
                    float newHeight = totalTextHeight;

                    scrollArea->contentSize.x() = textSize.x();
                    scrollArea->contentSize.y() = totalTextHeight;

                    // 应用锚定策略
                    if (scrollArea->anchor == policies::ScrollAnchor::Bottom)
                    {
                        // 锚定底部：Offset 随高度差增加，保持距离底部不变
                        scrollArea->scrollOffset.y() += (newHeight - oldHeight);
                    }
                    else if (scrollArea->anchor == policies::ScrollAnchor::Smart)
                    {
                        // 智能模式：如果之前在底部，则保持在底部
                        float oldMaxScroll = std::max(0.0F, oldHeight - viewportHeight);
                        // 给予 2.0 像素的容差
                        bool wasAtBottom = (scrollArea->scrollOffset.y() >= oldMaxScroll - 2.0F);

                        if (wasAtBottom)
                        {
                            float newMaxScroll = std::max(0.0F, newHeight - viewportHeight);
                            scrollArea->scrollOffset.y() = newMaxScroll;
                        }
                    }
                }

                // 始终更新宽度记录
                scrollArea->contentSize.x() = textSize.x();

                // 始终执行 Clamping，防止视口变化(Resize)导致的越界
                // 注意：这里使用当前的 totalTextHeight 和 viewportHeight 进行计算
                float maxScroll = std::max(0.0F, totalTextHeight - viewportHeight);
                scrollArea->scrollOffset.y() = std::clamp(scrollArea->scrollOffset.y(), 0.0F, maxScroll);

                // 使用滚动偏移来确定起始行
                const int maxVisibleLines = lineHeight > 0.0F ? static_cast<int>(textSize.y() / lineHeight) : 0;
                const int scrollOffsetLines =
                    lineHeight > 0.0F ? static_cast<int>(scrollArea->scrollOffset.y() / lineHeight) : 0;
                const size_t startIndex =
                    std::min(static_cast<size_t>(scrollOffsetLines), lines.size() > 0 ? lines.size() - 1 : 0);
                const size_t endIndex = std::min(startIndex + static_cast<size_t>(maxVisibleLines) + 1, lines.size());

                float y = textPos.y() - (scrollArea->scrollOffset.y() - scrollOffsetLines * lineHeight);
                for (size_t i = startIndex; i < endIndex; ++i)
                {
                    const std::string& line = lines[i];
                    if (!line.empty())
                    {
                        addText(line,
                                {textPos.x(), y},
                                {textSize.x(), lineHeight},
                                color,
                                policies::Alignment::LEFT,
                                context.alpha,
                                fontSize,
                                textEditContext);
                    }
                    y += lineHeight;
                }

                // 绘制光标 (仅当获焦时) - ScrollArea 分支
                if (Registry::AnyOf<components::FocusedTag>(entity))
                {
                    const auto* caret = Registry::TryGet<components::Caret>(entity);
                    if (context.sdlWindow && caret != nullptr && caret->visible)
                    {
                        // 根据 cursorPosition (字节索引) 定位光标所在行和列偏移
                        int cursorLineIdx = static_cast<int>(lines.size()) - 1;
                        float cursorLineX =
                            lines.empty() ? 0.0F : context.fontManager->measureTextWidth(lines.back(), fontSize);
                        {
                            size_t byteOffset = 0;
                            for (int lineIdx = 0; lineIdx < static_cast<int>(lines.size()); ++lineIdx)
                            {
                                const size_t lineByteEnd = byteOffset + lines[lineIdx].size();
                                if (textEdit.cursorPosition <= lineByteEnd)
                                {
                                    cursorLineIdx = lineIdx;
                                    const size_t offsetInLine = textEdit.cursorPosition - byteOffset;
                                    cursorLineX = context.fontManager->measureTextWidth(
                                        lines[lineIdx].substr(0, offsetInLine), fontSize);
                                    break;
                                }
                                byteOffset = lineByteEnd;
                                // 跳过硬换行符
                                if (byteOffset < displayText.size() && displayText[byteOffset] == '\n') byteOffset++;
                            }
                        }

                        float cursorX = -1000.0F;
                        float cursorY = -1000.0F;
                        if (cursorLineIdx >= scrollOffsetLines && cursorLineIdx < scrollOffsetLines + maxVisibleLines)
                        {
                            const int visibleLineIndex = cursorLineIdx - scrollOffsetLines;
                            cursorY =
                                textPos.y() + static_cast<float>(visibleLineIndex) * lineHeight -
                                (scrollArea->scrollOffset.y() - static_cast<float>(scrollOffsetLines) * lineHeight);
                            cursorX = textPos.x() + cursorLineX;
                        }

                        if (cursorX >= 0.0F && cursorY >= 0.0F)
                        {
                            render::UiPushConstants pushConstants{};
                            pushConstants.screen_size[0] = context.screenWidth;
                            pushConstants.screen_size[1] = context.screenHeight;
                            pushConstants.rect_size[0] = 2.0F;
                            pushConstants.rect_size[1] = lineHeight;
                            pushConstants.opacity = context.alpha;

                            context.batchManager->beginBatch(
                                context.whiteTexture, textEditContext.currentScissor, pushConstants);
                            context.batchManager->addRect(
                                {cursorX, cursorY}, {2.0F, lineHeight}, {1.0F, 1.0F, 1.0F, 1.0F});

                            SDL_Rect rect;
                            rect.x = static_cast<int>(cursorX);
                            rect.y = static_cast<int>(cursorY);
                            rect.w = 2;
                            rect.h = static_cast<int>(lineHeight);
                            SDL_SetTextInputArea(context.sdlWindow, &rect, 0);
                        }
                    }
                }
            }
            else
            {
                // 没有 ScrollArea 的情况：显示末尾内容（保持原有行为）
                const int maxLines = lineHeight > 0.0F ? static_cast<int>(textSize.y() / lineHeight) : 0;
                const size_t startIndex = (maxLines > 0 && lines.size() > static_cast<size_t>(maxLines))
                                              ? (lines.size() - static_cast<size_t>(maxLines))
                                              : 0;

                float y = textPos.y();
                for (size_t i = startIndex; i < lines.size(); ++i)
                {
                    const std::string& line = lines[i];
                    if (!line.empty())
                    {
                        addText(line,
                                {textPos.x(), y},
                                {textSize.x(), lineHeight},
                                color,
                                policies::Alignment::LEFT,
                                context.alpha,
                                fontSize,
                                textEditContext);
                    }
                    y += lineHeight;
                }

                // 绘制光标 (仅当获焦时)
                if (Registry::AnyOf<components::FocusedTag>(entity))
                {
                    const auto* caret = Registry::TryGet<components::Caret>(entity);
                    if (context.sdlWindow && caret != nullptr && caret->visible)
                    {
                        // 根据 cursorPosition (字节索引) 定位光标所在行和列偏移
                        int cursorLineIdx = static_cast<int>(lines.size()) - 1;
                        float cursorLineX =
                            lines.empty() ? 0.0F : context.fontManager->measureTextWidth(lines.back(), fontSize);
                        {
                            size_t byteOffset = 0;
                            for (int lineIdx = 0; lineIdx < static_cast<int>(lines.size()); ++lineIdx)
                            {
                                const size_t lineByteEnd = byteOffset + lines[lineIdx].size();
                                if (textEdit.cursorPosition <= lineByteEnd)
                                {
                                    cursorLineIdx = lineIdx;
                                    const size_t offsetInLine = textEdit.cursorPosition - byteOffset;
                                    cursorLineX = context.fontManager->measureTextWidth(
                                        lines[lineIdx].substr(0, offsetInLine), fontSize);
                                    break;
                                }
                                byteOffset = lineByteEnd;
                                // 跳过硬换行符
                                if (byteOffset < displayText.size() && displayText[byteOffset] == '\n') byteOffset++;
                            }
                        }

                        // 仅在可见行范围内绘制光标
                        float cursorX = -1000.0F;
                        float cursorY = -1000.0F;
                        if (cursorLineIdx >= static_cast<int>(startIndex))
                        {
                            const int relLine = cursorLineIdx - static_cast<int>(startIndex);
                            cursorX = textPos.x() + cursorLineX;
                            cursorY = (lines.size() == 1) ? textPos.y() + (textSize.y() - lineHeight) * 0.5F
                                                          : textPos.y() + static_cast<float>(relLine) * lineHeight;
                        }

                        if (cursorX >= 0.0F && cursorY >= 0.0F)
                        {
                            render::UiPushConstants pushConstants{};
                            pushConstants.screen_size[0] = context.screenWidth;
                            pushConstants.screen_size[1] = context.screenHeight;
                            pushConstants.rect_size[0] = 2.0F;
                            pushConstants.rect_size[1] = lineHeight;
                            pushConstants.opacity = context.alpha;

                            context.batchManager->beginBatch(
                                context.whiteTexture, textEditContext.currentScissor, pushConstants);
                            context.batchManager->addRect(
                                {cursorX, cursorY}, {2.0F, lineHeight}, {1.0F, 1.0F, 1.0F, 1.0F});

                            SDL_Rect rect;
                            rect.x = static_cast<int>(cursorX);
                            rect.y = static_cast<int>(cursorY);
                            rect.w = 2;
                            rect.h = static_cast<int>(lineHeight);
                            SDL_SetTextInputArea(context.sdlWindow, &rect, 0);
                        }
                    }
                }
            }
        }
        textEditContext.popScissor();
    }

    float getAncestorScrollAreaTextWidth(entt::entity entity) const
    {
        entt::entity current = entity;
        while (current != entt::null)
        {
            const auto* hierarchy = Registry::TryGet<components::Hierarchy>(current);
            if (hierarchy == nullptr) break;

            current = hierarchy->parent;
            if (current == entt::null) break;

            if (Registry::AnyOf<components::ScrollArea>(current))
            {
                const auto* size = Registry::TryGet<components::Size>(current);
                if (size == nullptr) return 0.0F;

                float width = size->size.x();
                if (const auto* padding = Registry::TryGet<components::Padding>(current))
                {
                    width -= (padding->values.y() + padding->values.z());
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
            fallbackCacheKey = buildFallbackTextCacheKey(text, color, fontSize);
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

        // 对齐到整数像素，避免亚像素偏移导致线性滤波在预乘 Alpha 纹理上产生暗边
        drawX = std::round(drawX);
        drawY = std::round(drawY);

        if (useFallbackText)
        {
            const SDL_FRect destinationRect = {drawX, drawY, textSize.x(), textSize.y()};
            const uint8_t alphaMod = static_cast<uint8_t>(std::lround(std::clamp(opacity, 0.0F, 1.0F) * 255.0F));
            context.backendRenderer->drawCachedBitmap(fallbackCacheKey,
                                                      fallbackBitmap->pixels,
                                                      static_cast<int>(fallbackBitmap->rasterWidth),
                                                      static_cast<int>(fallbackBitmap->rasterHeight),
                                                      destinationRect,
                                                      context.currentScissor,
                                                      alphaMod);
            return;
        }

        render::UiPushConstants pushConstants{};
        pushConstants.screen_size[0] = context.screenWidth;
        pushConstants.screen_size[1] = context.screenHeight;
        pushConstants.rect_size[0] = textSize.x();
        pushConstants.rect_size[1] = textSize.y();
        pushConstants.opacity = opacity;
        pushConstants.padding = 2.0F; // 标记纹理为 alpha mask，由 shader 负责着色

        context.batchManager->beginBatch(textTexture, context.currentScissor, pushConstants);
        context.batchManager->addRect({drawX, drawY}, textSize, color);
    }

    void addWrappedText(const std::string& text,
                        const Eigen::Vector2f& pos,
                        const Eigen::Vector2f& size,
                        const Eigen::Vector4f& color,
                        policies::Alignment alignment,
                        policies::TextWrap wrapMode,
                        float wrapWidth,
                        float opacity,
                        float fontSize,
                        core::RenderContext& context)
    {
        if (!context.fontManager->isLoaded() || text.empty() || wrapWidth <= 0.0F) return;

        const float lineHeight = static_cast<float>(context.fontManager->getFontHeight(fontSize));
        if (lineHeight <= 0.0F) return;

        auto measureFunc = [fontManager = context.fontManager, fontSize](const std::string& str)
        { return fontManager->measureTextWidth(str, fontSize); };

        std::vector<std::string> lines =
            ui::utils::WrapTextLines(text, static_cast<int>(wrapWidth), wrapMode, measureFunc);
        const float totalHeight = static_cast<float>(lines.size()) * lineHeight;

        float startY = pos.y();
        if (ui::utils::HasAlignment(alignment, policies::Alignment::VCENTER))
        {
            startY += (size.y() - totalHeight) * 0.5F;
        }
        else if (ui::utils::HasAlignment(alignment, policies::Alignment::BOTTOM))
        {
            startY += size.y() - totalHeight;
        }

        const uint8_t horizontalMask = static_cast<uint8_t>(policies::Alignment::LEFT) |
                                       static_cast<uint8_t>(policies::Alignment::HCENTER) |
                                       static_cast<uint8_t>(policies::Alignment::RIGHT);
        const uint8_t alignValue = static_cast<uint8_t>(alignment);
        policies::Alignment horizontalAlign = static_cast<policies::Alignment>(alignValue & horizontalMask);
        if (horizontalAlign == policies::Alignment::NONE)
        {
            horizontalAlign = policies::Alignment::LEFT;
        }

        float y = startY;
        for (const auto& line : lines)
        {
            if (!line.empty())
            {
                addText(
                    line, {pos.x(), y}, {wrapWidth, lineHeight}, color, horizontalAlign, opacity, fontSize, context);
            }
            y += lineHeight;
        }
    }

    std::unordered_map<std::string, FallbackTextBitmap> m_fallbackTextCache;
};

} // namespace ui::renderers
