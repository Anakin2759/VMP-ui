/**
 * ************************************************************************
 *
 * @file test_BatchManager.cpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-03-16
 * @version 0.1
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#include <gtest/gtest.h>
#include <optional>

#include "common/RenderTypes.hpp"
#include "Eigen/Core"
#include "SDL3/SDL_rect.h"
#include "src/managers/BatchManager.hpp"

namespace ui::tests
{
namespace
{

ui::render::UiPushConstants MakePushConstants(float rectWidth = 100.0F,
                                              float rectHeight = 40.0F,
                                              float opacity = 1.0F,
                                              float strokeWidth = 0.0F)
{
    ui::render::UiPushConstants pushConstants{};
    pushConstants.screen_size[0] = 1280.0F;
    pushConstants.screen_size[1] = 720.0F;
    pushConstants.rect_size[0] = rectWidth;
    pushConstants.rect_size[1] = rectHeight;
    pushConstants.opacity = opacity;
    pushConstants.stroke_width = strokeWidth;
    pushConstants.draw_mode = 0.0F;
    return pushConstants;
}

Eigen::Vector4f White()
{
    return {1.0F, 1.0F, 1.0F, 1.0F};
}

} // namespace

TEST(BatchManagerTest, AddRectProducesSingleBatchAfterOptimize)
{
    ui::managers::BatchManager batchManager;

    batchManager.beginBatch(nullptr, std::nullopt, MakePushConstants());
    batchManager.addRect({10.0F, 20.0F}, {120.0F, 30.0F}, White());
    batchManager.optimize();

    ASSERT_EQ(batchManager.getBatchCount(), 1U);
    EXPECT_EQ(batchManager.getTotalVertexCount(), 4U);

    const auto& batches = batchManager.getBatches();
    ASSERT_EQ(batches.size(), 1U);
    EXPECT_EQ(batches.at(0).vertices.size(), 4U);
    EXPECT_EQ(batches.at(0).indices.size(), 6U);
    EXPECT_EQ(batches.at(0).texture, nullptr);
    EXPECT_FALSE(batches.at(0).scissorRect.has_value());
}

TEST(BatchManagerTest, DifferentScissorProducesDifferentBatches)
{
    ui::managers::BatchManager batchManager;

    const SDL_Rect firstScissor{.x = 0, .y = 0, .w = 100, .h = 100};
    const SDL_Rect secondScissor{.x = 10, .y = 10, .w = 100, .h = 100};

    batchManager.beginBatch(nullptr, firstScissor, MakePushConstants());
    batchManager.addRect({0.0F, 0.0F}, {10.0F, 10.0F}, White());
    batchManager.beginBatch(nullptr, secondScissor, MakePushConstants());
    batchManager.addRect({20.0F, 20.0F}, {10.0F, 10.0F}, White());
    batchManager.optimize();

    ASSERT_EQ(batchManager.getBatchCount(), 2U);
    const auto& batches = batchManager.getBatches();
    ASSERT_EQ(batches.size(), 2U);
    ASSERT_TRUE(batches.at(0).scissorRect.has_value());
    ASSERT_TRUE(batches.at(1).scissorRect.has_value());
    const auto firstScissorRect = batches.at(0).scissorRect.value_or(SDL_Rect{});
    const auto secondScissorRect = batches.at(1).scissorRect.value_or(SDL_Rect{});
    EXPECT_EQ(firstScissorRect.x, 0);
    EXPECT_EQ(secondScissorRect.x, 10);
}

TEST(BatchManagerTest, DifferentOpacityMergesIntoBatchWithPerVertexParams)
{
    ui::managers::BatchManager batchManager;

    batchManager.beginBatch(nullptr, std::nullopt, MakePushConstants(100.0F, 40.0F, 1.0F));
    batchManager.addRect({0.0F, 0.0F}, {10.0F, 10.0F}, White());
    batchManager.beginBatch(nullptr, std::nullopt, MakePushConstants(100.0F, 40.0F, 0.5F));
    batchManager.addRect({20.0F, 20.0F}, {10.0F, 10.0F}, White());
    batchManager.optimize();

    ASSERT_EQ(batchManager.getBatchCount(), 1U);
    EXPECT_EQ(batchManager.getTotalVertexCount(), 8U);

    const auto& batches = batchManager.getBatches();
    EXPECT_FLOAT_EQ(batches.at(0).vertices.at(0).shadow_params[3], 1.0F);
    EXPECT_FLOAT_EQ(batches.at(0).vertices.at(1).shadow_params[3], 1.0F);
    EXPECT_FLOAT_EQ(batches.at(0).vertices.at(2).shadow_params[3], 1.0F);
    EXPECT_FLOAT_EQ(batches.at(0).vertices.at(3).shadow_params[3], 1.0F);
    EXPECT_FLOAT_EQ(batches.at(0).vertices.at(4).shadow_params[3], 0.5F);
    EXPECT_FLOAT_EQ(batches.at(0).vertices.at(5).shadow_params[3], 0.5F);
    EXPECT_FLOAT_EQ(batches.at(0).vertices.at(6).shadow_params[3], 0.5F);
    EXPECT_FLOAT_EQ(batches.at(0).vertices.at(7).shadow_params[3], 0.5F);
}

TEST(BatchManagerTest, ClearResetsAllBatchesAndSupportsReuse)
{
    ui::managers::BatchManager batchManager;

    batchManager.beginBatch(nullptr, std::nullopt, MakePushConstants());
    batchManager.addRect({0.0F, 0.0F}, {10.0F, 10.0F}, White());
    batchManager.optimize();
    ASSERT_EQ(batchManager.getBatchCount(), 1U);

    batchManager.clear();

    EXPECT_EQ(batchManager.getBatchCount(), 0U);
    EXPECT_EQ(batchManager.getTotalVertexCount(), 0U);
    EXPECT_TRUE(batchManager.getBatches().empty());

    batchManager.beginBatch(nullptr, std::nullopt, MakePushConstants(80.0F, 20.0F, 0.75F));
    batchManager.addRect({5.0F, 5.0F}, {8.0F, 8.0F}, White());
    batchManager.optimize();

    ASSERT_EQ(batchManager.getBatchCount(), 1U);
    EXPECT_FLOAT_EQ(batchManager.getBatches().at(0).pushConstants.opacity, 0.75F);
}

} // namespace ui::tests
