#include "src/renderers/FallbackBackendRenderer.hpp"
#include "src/common/CustomizationPoints.hpp"
#include "src/renderers/CanvasRenderer.hpp"
#include "src/renderers/ImageRenderer.hpp"
#include "src/renderers/ShapeRenderer.hpp"
#include "src/renderers/TextRenderer.hpp"
#include "src/systems/render/WindowRenderState.hpp"
#include "src/managers/DeviceManager.hpp"
#include "src/core/UiRuntimeScope.hpp"

#include <array>
#include <filesystem>
#include <fstream>

#include <gtest/gtest.h>

namespace ui::tests
{
namespace
{

utils::Logger& TestLogger()
{
    static utils::Logger logger;
    return logger;
}

class CapabilityOverrideBackend final : public interface::IBackendRenderer
{
   public:
    ui::Result<void> initialize(SDL_Window* window) override
    {
        static_cast<void>(window);
        return ui::Ok();
    }
    void cleanup() override {}
    ui::Result<void> beginFrame(const SDL_FColor& clearColor) override
    {
        static_cast<void>(clearColor);
        return ui::Ok();
    }
    ui::Result<void> drawBatch(const render::RenderBatch& batch, SDL_GPUTexture* whiteTextureTag) override
    {
        static_cast<void>(batch);
        static_cast<void>(whiteTextureTag);
        return ui::Ok();
    }
    ui::Result<void> endFrame() override { return ui::Ok(); }
    ui::Result<void> drawCachedBitmap(std::string_view cacheKey, std::span<const std::uint8_t> rgbaPixels,
                                      int bitmapWidth, int bitmapHeight, const SDL_FRect& destinationRect,
                                      const std::optional<SDL_Rect>& scissorRect, std::uint8_t alphaMod) override
    {
        static_cast<void>(cacheKey);
        static_cast<void>(rgbaPixels);
        static_cast<void>(bitmapWidth);
        static_cast<void>(bitmapHeight);
        static_cast<void>(destinationRect);
        static_cast<void>(scissorRect);
        static_cast<void>(alphaMod);
        ++bitmapDrawCount;
        return bitmapDrawResult;
    }
    [[nodiscard]] interface::BackendType getType() const override { return interface::BackendType::FALLBACK; }
    [[nodiscard]] interface::BackendCapabilityStatus capabilityStatus(
        interface::BackendCapability capability) const override
    {
        static_cast<void>(capability);
        return status;
    }

    interface::BackendCapabilityStatus status = interface::BackendCapabilityStatus::SUPPORTED;
    ui::Result<void> bitmapDrawResult = ui::Ok();
    std::size_t bitmapDrawCount = 0;
};

class RecordingDiagnostics final : public core::IRenderCapabilityDiagnostics
{
   public:
    void report(interface::BackendCapability capability, interface::BackendCapabilityStatus status,
                std::string_view feature, std::string_view fallbackAction) override
    {
        static_cast<void>(feature);
        static_cast<void>(fallbackAction);
        reports.emplace_back(capability, status);
    }

    std::vector<std::pair<interface::BackendCapability, interface::BackendCapabilityStatus>> reports;
};

[[nodiscard]] std::filesystem::path WriteTestBmp()
{
    constexpr std::array<std::uint8_t, 62> BMP_BYTES = {
        0x42, 0x4D, 0x3E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
        0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x08, 0x00, 0x00, 0x00, 0x13, 0x0B, 0x00, 0x00, 0x13, 0x0B, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0x00, 0xFF, 0x00, 0x00, 0x00};
    const auto path = std::filesystem::path(::testing::TempDir()) / "vmp_ui_fallback_renderer.bmp";
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(BMP_BYTES.data()), static_cast<std::streamsize>(BMP_BYTES.size()));
    return path;
}

TEST(FallbackRendererGeometryTest, DetectsAxisAlignedQuad)
{
    const std::array<SDL_FPoint, 4> points = {
        SDL_FPoint{10.0F, 20.0F}, SDL_FPoint{30.0F, 20.0F}, SDL_FPoint{30.0F, 40.0F},
        SDL_FPoint{10.0F, 40.0F}};

    EXPECT_TRUE(renderers::detail::isAxisAlignedQuad(points));
}

TEST(FallbackRendererGeometryTest, RejectsRotatedQuad)
{
    const std::array<SDL_FPoint, 4> points = {
        SDL_FPoint{20.0F, 10.0F}, SDL_FPoint{40.0F, 20.0F}, SDL_FPoint{30.0F, 40.0F},
        SDL_FPoint{10.0F, 30.0F}};

    EXPECT_FALSE(renderers::detail::isAxisAlignedQuad(points));
}

TEST(BackendCapabilityMatrixTest, GpuSupportsEveryDeclaredCapability)
{
    using enum interface::BackendCapability;
    constexpr std::array capabilities = {SOLID_RECT, TRANSFORMED_SOLID_QUAD, ROUNDED_RECT, BORDER, SHADOW,
                                         CACHED_BITMAP, BITMAP_COLOR_MODULATION, BITMAP_UV_CROP, FILLED_CIRCLE,
                                         CIRCLE_OUTLINE, CAPSULE};
    for (const auto capability : capabilities)
    {
        EXPECT_EQ(interface::GetBackendCapabilityStatus(interface::BackendType::GPU, capability),
                  interface::BackendCapabilityStatus::SUPPORTED);
    }
}

TEST(BackendCapabilityMatrixTest, FallbackDistinguishesSupportedDegradedAndUnsupportedFeatures)
{
    using enum interface::BackendCapability;
    using enum interface::BackendCapabilityStatus;
    constexpr std::array supported = {SOLID_RECT, CACHED_BITMAP};
    constexpr std::array degraded = {TRANSFORMED_SOLID_QUAD, ROUNDED_RECT, FILLED_CIRCLE, CIRCLE_OUTLINE};
    constexpr std::array unsupported = {BORDER, SHADOW, BITMAP_COLOR_MODULATION, BITMAP_UV_CROP, CAPSULE};

    for (const auto capability : supported)
    {
        EXPECT_EQ(interface::GetBackendCapabilityStatus(interface::BackendType::FALLBACK, capability), SUPPORTED);
    }
    for (const auto capability : degraded)
    {
        EXPECT_EQ(interface::GetBackendCapabilityStatus(interface::BackendType::FALLBACK, capability), DEGRADED);
    }
    for (const auto capability : unsupported)
    {
        EXPECT_EQ(interface::GetBackendCapabilityStatus(interface::BackendType::FALLBACK, capability), UNSUPPORTED);
    }
}

TEST(BackendCapabilityMatrixTest, CapabilityCpoUsesVirtualOverrideAfterTypeErasure)
{
    CapabilityOverrideBackend backend;
    const interface::IBackendRenderer& erasedBackend = backend;
    backend.status = interface::BackendCapabilityStatus::DEGRADED;
    EXPECT_EQ(cpo::backend_capability_level(erasedBackend, interface::BackendCapability::ROUNDED_RECT),
              interface::BackendCapabilityStatus::DEGRADED);
    EXPECT_TRUE(cpo::backend_supports(erasedBackend, interface::BackendCapability::ROUNDED_RECT));

    backend.status = interface::BackendCapabilityStatus::UNSUPPORTED;
    EXPECT_FALSE(cpo::backend_supports(erasedBackend, interface::BackendCapability::ROUNDED_RECT));
}

TEST(RenderCapabilityDiagnosticsTest, ReportsEachCapabilityAndStatusOnlyOncePerWindowState)
{
    std::size_t messageCount = 0;
    utils::Logger logger([&messageCount](utils::LogLevel, std::string_view, std::source_location) { ++messageCount; });
    systems::render_detail::WindowCapabilityDiagnostics diagnostics;
    diagnostics.SetLogger(logger);

    diagnostics.report(interface::BackendCapability::SHADOW, interface::BackendCapabilityStatus::UNSUPPORTED,
                       "shadow", "omitted");
    diagnostics.report(interface::BackendCapability::SHADOW, interface::BackendCapabilityStatus::UNSUPPORTED,
                       "shadow", "omitted");
    diagnostics.report(interface::BackendCapability::ROUNDED_RECT, interface::BackendCapabilityStatus::DEGRADED,
                       "rounded rect", "approximated");
    EXPECT_EQ(messageCount, 2U);
}

TEST(CanvasFallbackCapabilityTest, UnsupportedCapsuleFallsBackToTransformedSolidQuad)
{
    Registry registry;
    const auto entity = registry.create();
    registry.emplace<components::CanvasTag>(entity);
    auto& drawList = registry.emplace<components::CanvasDrawList>(entity);
    components::CanvasDrawCommand line;
    line.type = components::CanvasDrawType::LINE;
    line.p1 = {4.0F, 4.0F};
    line.p2 = {28.0F, 20.0F};
    line.lineWidth = 3.0F;
    drawList.commands.push_back(line);

    managers::BatchManager batchManager;
    renderers::FallbackBackendRenderer backend{TestLogger()};
    RecordingDiagnostics diagnostics;
    core::RenderContext context;
    context.batchManager = &batchManager;
    context.backendRenderer = &backend;
    context.capabilityDiagnostics = &diagnostics;
    context.whiteTexture = reinterpret_cast<SDL_GPUTexture*>(static_cast<std::uintptr_t>(1));
    context.screenWidth = 64.0F;
    context.screenHeight = 64.0F;

    renderers::CanvasRenderer renderer(registry);
    renderer.collect(entity, context);
    batchManager.optimize();

    ASSERT_EQ(batchManager.getBatchCount(), 1U);
    const auto& batch = batchManager.getBatches().front();
    ASSERT_EQ(batch.vertices.size(), 4U);
    EXPECT_FLOAT_EQ(batch.vertices.front().mode_params[2], 0.0F);
    EXPECT_FALSE(renderers::detail::isAxisAlignedQuad({
        SDL_FPoint{batch.vertices[0].position[0], batch.vertices[0].position[1]},
        SDL_FPoint{batch.vertices[1].position[0], batch.vertices[1].position[1]},
        SDL_FPoint{batch.vertices[2].position[0], batch.vertices[2].position[1]},
        SDL_FPoint{batch.vertices[3].position[0], batch.vertices[3].position[1]}}));
    EXPECT_TRUE(std::ranges::any_of(diagnostics.reports, [](const auto& report) {
        return report.first == interface::BackendCapability::CAPSULE &&
               report.second == interface::BackendCapabilityStatus::UNSUPPORTED;
    }));
}

TEST(CanvasFallbackCapabilityTest, FilledCircleKeepsBatchAndReportsDegradedGeometry)
{
    Registry registry;
    const auto entity = registry.create();
    registry.emplace<components::CanvasTag>(entity);
    auto& drawList = registry.emplace<components::CanvasDrawList>(entity);
    components::CanvasDrawCommand circle;
    circle.type = components::CanvasDrawType::FILLED_CIRCLE;
    circle.p1 = {20.0F, 20.0F};
    circle.p2 = {8.0F, 0.0F};
    drawList.commands.push_back(circle);

    managers::BatchManager batchManager;
    renderers::FallbackBackendRenderer backend{TestLogger()};
    RecordingDiagnostics diagnostics;
    core::RenderContext context;
    context.batchManager = &batchManager;
    context.backendRenderer = &backend;
    context.capabilityDiagnostics = &diagnostics;
    context.whiteTexture = reinterpret_cast<SDL_GPUTexture*>(static_cast<std::uintptr_t>(1));  // NOLINT

    renderers::CanvasRenderer renderer(registry);
    renderer.collect(entity, context);
    batchManager.optimize();

    ASSERT_EQ(batchManager.getBatchCount(), 1U);
    const auto& batch = batchManager.getBatches().front();
    ASSERT_EQ(batch.vertices.size(), 4U);
    EXPECT_GT(batch.vertices.front().radius[0], 0.0F);
    EXPECT_TRUE(std::ranges::any_of(diagnostics.reports, [](const auto& report) {
        return report.first == interface::BackendCapability::FILLED_CIRCLE &&
               report.second == interface::BackendCapabilityStatus::DEGRADED;
    }));
}

TEST(ShapeFallbackCapabilityTest, UnsupportedShadowAndBorderPreserveBackgroundBatch)
{
    Registry registry;
    const auto entity = registry.create();
    auto& background = registry.emplace<components::Background>(entity);
    background.enabled = policies::Feature::ENABLED;
    background.color = Color{1.0F, 0.0F, 0.0F, 1.0F};
    background.borderRadius = Vec4{4.0F, 4.0F, 4.0F, 4.0F};
    auto& shadow = registry.emplace<components::Shadow>(entity);
    shadow.enabled = policies::Feature::ENABLED;
    shadow.softness = 6.0F;
    auto& border = registry.emplace<components::Border>(entity);
    border.enabled = policies::Feature::ENABLED;
    border.thickness = 2.0F;

    managers::BatchManager batchManager;
    managers::DeviceManager deviceManager{TestLogger()};
    renderers::FallbackBackendRenderer backend{TestLogger()};
    RecordingDiagnostics diagnostics;
    core::RenderContext context;
    context.batchManager = &batchManager;
    context.deviceManager = &deviceManager;
    context.backendRenderer = &backend;
    context.capabilityDiagnostics = &diagnostics;
    context.whiteTexture = reinterpret_cast<SDL_GPUTexture*>(static_cast<std::uintptr_t>(1));  // NOLINT
    context.size = {32.0F, 20.0F};

    renderers::ShapeRenderer renderer(registry);
    renderer.collect(entity, context);
    batchManager.optimize();

    ASSERT_EQ(batchManager.getBatchCount(), 1U) << "unsupported border must not remove the background";
    const auto& batch = batchManager.getBatches().front();
    ASSERT_EQ(batch.vertices.size(), 4U);
    EXPECT_FLOAT_EQ(batch.vertices.front().shadow_params[0], 0.0F);
    EXPECT_TRUE(std::ranges::any_of(diagnostics.reports, [](const auto& report) {
        return report.first == interface::BackendCapability::SHADOW &&
               report.second == interface::BackendCapabilityStatus::UNSUPPORTED;
    }));
    EXPECT_TRUE(std::ranges::any_of(diagnostics.reports, [](const auto& report) {
        return report.first == interface::BackendCapability::BORDER &&
               report.second == interface::BackendCapabilityStatus::UNSUPPORTED;
    }));
    EXPECT_TRUE(std::ranges::any_of(diagnostics.reports, [](const auto& report) {
        return report.first == interface::BackendCapability::ROUNDED_RECT &&
               report.second == interface::BackendCapabilityStatus::DEGRADED;
    }));
}

TEST(BackendRenderContractTest, ShapeKeepsBaseGeometryAcrossGpuAndFallbackCapabilities)
{
    Registry registry;
    const auto entity = registry.create();
    auto& background = registry.emplace<components::Background>(entity);
    background.enabled = policies::Feature::ENABLED;
    background.color = Color{0.2F, 0.4F, 0.8F, 0.75F};
    background.borderRadius = Vec4{6.0F, 6.0F, 6.0F, 6.0F};
    auto& shadow = registry.emplace<components::Shadow>(entity);
    shadow.enabled = policies::Feature::ENABLED;
    shadow.softness = 5.0F;
    auto& border = registry.emplace<components::Border>(entity);
    border.enabled = policies::Feature::ENABLED;
    border.thickness = 2.0F;

    managers::DeviceManager deviceManager{TestLogger()};
    const auto collectSnapshot = [&](interface::IBackendRenderer* backend) {
        managers::BatchManager batchManager;
        RecordingDiagnostics diagnostics;
        core::RenderContext context;
        context.position = {11.0F, 13.0F};
        context.size = {48.0F, 24.0F};
        context.alpha = 0.8F;
        context.batchManager = &batchManager;
        context.deviceManager = &deviceManager;
        context.backendRenderer = backend;
        context.capabilityDiagnostics = &diagnostics;
        context.whiteTexture = reinterpret_cast<SDL_GPUTexture*>(static_cast<std::uintptr_t>(1));  // NOLINT

        renderers::ShapeRenderer renderer(registry);
        renderer.collect(entity, context);
        batchManager.optimize();
        EXPECT_EQ(batchManager.getBatchCount(), 1U);
        return std::pair{batchManager.getBatches().front(), std::move(diagnostics.reports)};
    };

    renderers::FallbackBackendRenderer fallbackBackend{TestLogger()};
    auto [gpuBatch, gpuDiagnostics] = collectSnapshot(nullptr);
    auto [fallbackBatch, fallbackDiagnostics] = collectSnapshot(&fallbackBackend);

    ASSERT_EQ(gpuBatch.vertices.size(), 8U);
    ASSERT_EQ(fallbackBatch.vertices.size(), 4U);
    for (std::size_t index = 0; index < fallbackBatch.vertices.size(); ++index)
    {
        EXPECT_FLOAT_EQ(fallbackBatch.vertices[index].position[0], gpuBatch.vertices[index].position[0]);
        EXPECT_FLOAT_EQ(fallbackBatch.vertices[index].position[1], gpuBatch.vertices[index].position[1]);
        EXPECT_FLOAT_EQ(fallbackBatch.vertices[index].color[0], gpuBatch.vertices[index].color[0]);
        EXPECT_FLOAT_EQ(fallbackBatch.vertices[index].color[1], gpuBatch.vertices[index].color[1]);
        EXPECT_FLOAT_EQ(fallbackBatch.vertices[index].color[2], gpuBatch.vertices[index].color[2]);
        EXPECT_FLOAT_EQ(fallbackBatch.vertices[index].color[3], gpuBatch.vertices[index].color[3]);
    }
    EXPECT_TRUE(gpuDiagnostics.empty());
    EXPECT_TRUE(std::ranges::any_of(fallbackDiagnostics, [](const auto& report) {
        return report.first == interface::BackendCapability::SHADOW &&
               report.second == interface::BackendCapabilityStatus::UNSUPPORTED;
    }));
    EXPECT_TRUE(std::ranges::any_of(fallbackDiagnostics, [](const auto& report) {
        return report.first == interface::BackendCapability::BORDER &&
               report.second == interface::BackendCapabilityStatus::UNSUPPORTED;
    }));
}

TEST(ImageFallbackCapabilityTest, UnsupportedCachedBitmapIsDiagnosedWithoutFailingFrame)
{
    Registry registry;
    const auto entity = registry.create();
    registry.emplace<components::ImageTag>(entity);
    registry.emplace<components::ImageSource>(entity).path = "unused.bmp";

    CapabilityOverrideBackend backend;
    backend.status = interface::BackendCapabilityStatus::UNSUPPORTED;
    RecordingDiagnostics diagnostics;
    core::RenderContext context;
    context.backendRenderer = &backend;
    context.capabilityDiagnostics = &diagnostics;

    renderers::ImageRenderer renderer(registry);
    const auto result = renderer.collectChecked(entity, context);

    EXPECT_TRUE(result.has_value());
    ASSERT_EQ(diagnostics.reports.size(), 1U);
    EXPECT_EQ(diagnostics.reports.front().first, interface::BackendCapability::CACHED_BITMAP);
    EXPECT_EQ(diagnostics.reports.front().second, interface::BackendCapabilityStatus::UNSUPPORTED);
}

TEST(ImageFallbackCapabilityTest, BitmapDrawFailurePropagatesFromCollectChecked)
{
    Registry registry;
    const auto entity = registry.create();
    registry.emplace<components::ImageTag>(entity);
    const auto imagePath = WriteTestBmp();
    registry.emplace<components::ImageSource>(entity).path = imagePath.string();

    CapabilityOverrideBackend backend;
    backend.status = interface::BackendCapabilityStatus::SUPPORTED;
    backend.bitmapDrawResult = ui::Err(UiErrc::ASSET_UPLOAD_FAILED, "injected bitmap draw failure");
    utils::Logger logger;
    managers::ImageManager imageManager{nullptr, logger};
    core::RenderContext context;
    context.backendRenderer = &backend;
    context.imageManager = &imageManager;

    renderers::ImageRenderer renderer(registry);
    const auto result = renderer.collectChecked(entity, context);

    ASSERT_FALSE(result.has_value());
    EXPECT_EQ(result.error(), UiErrc::ASSET_UPLOAD_FAILED);
    EXPECT_EQ(backend.bitmapDrawCount, 1U);
    std::filesystem::remove(imagePath);
}

TEST(TextFallbackCapabilityTest, UnsupportedCachedBitmapIsDiagnosedBeforeFontRasterization)
{
    UiRuntime runtime;
    UiRuntimeScope scope(runtime);
    Registry registry;
    const auto entity = registry.create();
    registry.emplace<components::TextTag>(entity);
    registry.emplace<components::Text>(entity).content = "fallback text";

    CapabilityOverrideBackend backend;
    backend.status = interface::BackendCapabilityStatus::UNSUPPORTED;
    RecordingDiagnostics diagnostics;
    managers::BatchManager batchManager;
    managers::FontManager fontManager{TestLogger()};
    core::RenderContext context;
    context.backendRenderer = &backend;
    context.capabilityDiagnostics = &diagnostics;
    context.batchManager = &batchManager;
    context.fontManager = &fontManager;

    renderers::TextRenderer renderer(registry);
    const auto result = renderer.collectChecked(entity, context);

    EXPECT_TRUE(result.has_value());
    EXPECT_EQ(backend.bitmapDrawCount, 0U);
    ASSERT_EQ(diagnostics.reports.size(), 1U);
    EXPECT_EQ(diagnostics.reports.front().first, interface::BackendCapability::CACHED_BITMAP);
    EXPECT_EQ(diagnostics.reports.front().second, interface::BackendCapabilityStatus::UNSUPPORTED);
}

TEST(TextRenderContextContractTest, MissingRequiredManagersReturnExplicitErrors)
{
    Registry registry;
    const auto entity = registry.create();
    registry.emplace<components::TextTag>(entity);
    registry.emplace<components::Text>(entity).content = "text";

    renderers::TextRenderer renderer(registry);
    core::RenderContext context;

    const auto missingBatch = renderer.collectChecked(entity, context);
    ASSERT_FALSE(missingBatch.has_value());
    EXPECT_EQ(missingBatch.error(), UiErrc::DEVICE_UNAVAILABLE);

    managers::BatchManager batchManager;
    context.batchManager = &batchManager;
    const auto missingFont = renderer.collectChecked(entity, context);
    ASSERT_FALSE(missingFont.has_value());
    EXPECT_EQ(missingFont.error(), UiErrc::DEVICE_UNAVAILABLE);
}

}  // namespace
}  // namespace ui::tests
