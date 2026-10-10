#include <gtest/gtest.h>

#include <cmath>
#include <memory>
#include <limits>

#include "common/Events.hpp"
#include "common/GlobalContext.hpp"
#include "common/components/Layout.hpp"
#include "src/core/UiRuntime.hpp"
#include "src/core/UiRuntimeScope.hpp"
#include "src/core/SystemManager.hpp"
#include "src/helper/Helper.hpp"
#include "ui/api/Hierarchy.hpp"
#include "ui/api/Factory.hpp"
#include "ui/api/Layout.hpp"

namespace ui::tests
{
namespace
{

class AnchorsTest : public ::testing::Test
{
   protected:
    void SetUp() override
    {
        m_scope = std::make_unique<UiRuntimeScope>(m_runtime);
        m_systems = std::make_unique<SystemManager>(&m_runtime);
        m_systems->registerAllHandlers();
    }

    void TearDown() override
    {
        m_systems.reset();
        m_scope.reset();
    }

    void calculate()
    {
        auto& frame = m_runtime.ensureContext<globalcontext::FrameContext>();
        frame.stage = globalcontext::FrameStage::LAYOUT;
        frame.layoutUpdateCount = 0;
        m_runtime.dispatcher().trigger(events::UpdateLayout{});
        frame.stage = globalcontext::FrameStage::IDLE;
    }

    ui::entity makeRoot(float width = 400.0F, float height = 300.0F)
    {
        const auto root = factory::CreateVBoxLayout("anchors_root");
        auto& registry = m_runtime.registry();
        registry.emplace<components::RootTag>(root);
        auto& size = registry.get<components::Size>(root);
        size.size = Vec2{width, height};
        size.sizePolicy = policies::Size::FIXED;
        registry.get<components::Position>(root).positionPolicy = policies::Position::FIXED;
        return root;
    }

    ui::entity makeFixedChild(float width = 80.0F, float height = 40.0F)
    {
        const auto child = factory::CreateLabel("child", "anchors_child");
        auto& size = m_runtime.registry().get<components::Size>(child);
        size.size = Vec2{width, height};
        size.sizePolicy = policies::Size::FIXED;
        return child;
    }

    void attach(ui::entity root, ui::entity child)
    {
        hierarchy::AddChild(root, child);
    }

    Registry& registry() { return m_runtime.registry(); }

    UiRuntime m_runtime;
    std::unique_ptr<UiRuntimeScope> m_scope;
    std::unique_ptr<SystemManager> m_systems;
};

TEST_F(AnchorsTest, FillUsesAllParentEdges)
{
    const auto root = makeRoot();
    const auto child = makeFixedChild(20.0F, 30.0F);
    attach(root, child);

    ASSERT_TRUE(layout::SetAnchors(child, ParentAnchors::Fill()).has_value());
    calculate();

    const auto& position = registry().get<components::Position>(child);
    const auto& size = registry().get<components::Size>(child);
    EXPECT_FLOAT_EQ(position.value.x(), 0.0F);
    EXPECT_FLOAT_EQ(position.value.y(), 0.0F);
    EXPECT_FLOAT_EQ(size.size.x(), 400.0F);
    EXPECT_FLOAT_EQ(size.size.y(), 300.0F);
}

TEST_F(AnchorsTest, CenterUsesMeasuredSizeAndOffsets)
{
    const auto root = makeRoot();
    const auto child = makeFixedChild();
    attach(root, child);

    ASSERT_TRUE(layout::SetAnchors(child, ParentAnchors::CenterIn(10.0F, -5.0F)).has_value());
    calculate();

    const auto& position = registry().get<components::Position>(child);
    EXPECT_NEAR(position.value.x(), 170.0F, 0.01F);
    EXPECT_NEAR(position.value.y(), 125.0F, 0.01F);
}

TEST_F(AnchorsTest, EdgeAnchorsRespectInsets)
{
    const auto root = makeRoot();
    const auto child = makeFixedChild();
    attach(root, child);

    ParentAnchors anchors;
    anchors.left = 12.0F;
    anchors.top = 18.0F;
    ASSERT_TRUE(layout::SetAnchors(child, anchors).has_value());
    calculate();

    const auto& position = registry().get<components::Position>(child);
    EXPECT_FLOAT_EQ(position.value.x(), 12.0F);
    EXPECT_FLOAT_EQ(position.value.y(), 18.0F);
}

TEST_F(AnchorsTest, ResizeRecalculatesFillAndCenter)
{
    const auto root = makeRoot();
    const auto child = makeFixedChild();
    attach(root, child);
    ASSERT_TRUE(layout::SetAnchors(child, ParentAnchors::CenterIn()).has_value());
    calculate();
    EXPECT_NEAR(registry().get<components::Position>(child).value.x(), 160.0F, 0.01F);

    registry().get<components::Size>(root).size = Vec2{600.0F, 500.0F};
    utils::MarkLayoutDirty(m_runtime, root);
    calculate();
    EXPECT_NEAR(registry().get<components::Position>(child).value.x(), 260.0F, 0.01F);
    EXPECT_NEAR(registry().get<components::Position>(child).value.y(), 230.0F, 0.01F);
}

TEST_F(AnchorsTest, ClearRestoresFlexLayout)
{
    const auto root = makeRoot();
    const auto child = makeFixedChild();
    attach(root, child);
    ASSERT_TRUE(layout::SetAnchors(child, ParentAnchors::Fill()).has_value());
    calculate();
    ASSERT_TRUE(layout::ClearAnchors(child).has_value());
    calculate();

    EXPECT_FALSE(registry().all_of<components::Anchors>(child));
    EXPECT_FLOAT_EQ(registry().get<components::Size>(child).size.x(), 400.0F);
    EXPECT_FLOAT_EQ(registry().get<components::Size>(child).size.y(), 300.0F);
}

TEST_F(AnchorsTest, ClearAnchorsKeepsCurrentDeclaredSize)
{
    const auto root = makeRoot();
    const auto child = makeFixedChild(20.0F, 20.0F);
    attach(root, child);
    ASSERT_TRUE(layout::SetAnchorFill(child).has_value());
    calculate();
    registry().get<components::Size>(child).size = Vec2{55.0F, 35.0F};
    ASSERT_TRUE(layout::ClearAnchors(child).has_value());
    EXPECT_FLOAT_EQ(registry().get<components::Size>(child).size.x(), 55.0F);
    EXPECT_FLOAT_EQ(registry().get<components::Size>(child).size.y(), 35.0F);
}

TEST_F(AnchorsTest, ClearFillRestoresDeclaredSize)
{
    const auto root = makeRoot();
    const auto child = makeFixedChild(80.0F, 40.0F);
    attach(root, child);
    auto& declared = registry().get<components::Size>(child);
    declared.declaredSize = declared.size;
    declared.hasDeclaredWidth = true;
    declared.hasDeclaredHeight = true;
    ASSERT_TRUE(layout::SetAnchorFill(child).has_value());
    calculate();
    ASSERT_TRUE(layout::ClearAnchors(child).has_value());
    calculate();
    EXPECT_FLOAT_EQ(registry().get<components::Size>(child).size.x(), 80.0F);
    EXPECT_FLOAT_EQ(registry().get<components::Size>(child).size.y(), 40.0F);
}

TEST_F(AnchorsTest, ExplicitZeroSizeIsNotReplacedByImplicitSize)
{
    const auto root = makeRoot();
    const auto child = makeFixedChild(0.0F, 0.0F);
    attach(root, child);
    auto& size = registry().get<components::Size>(child);
    size.sizePolicy = policies::Size::FIXED;
    size.declaredSize = Vec2{0.0F, 0.0F};
    size.hasDeclaredWidth = true;
    size.hasDeclaredHeight = true;
    auto& implicit = registry().emplace<components::ImplicitSize>(child);
    implicit.value = Vec2{80.0F, 30.0F};
    implicit.hasWidth = true;
    implicit.hasHeight = true;
    ASSERT_TRUE(layout::SetAnchorCenterIn(child).has_value());
    calculate();
    EXPECT_FLOAT_EQ(registry().get<components::Size>(child).size.x(), 0.0F);
    EXPECT_FLOAT_EQ(registry().get<components::Size>(child).size.y(), 0.0F);
}

TEST_F(AnchorsTest, ImplicitSizeIsSeparateFromDeclaredSize)
{
    const auto root = makeRoot();
    const auto child = makeFixedChild(0.0F, 0.0F);
    attach(root, child);
    registry().emplace<components::ImplicitSize>(child).value = Vec2{80.0F, 30.0F};
    auto& implicit = registry().get<components::ImplicitSize>(child);
    implicit.hasWidth = true;
    implicit.hasHeight = true;
    ASSERT_TRUE(layout::SetAnchorCenterIn(child).has_value());
    calculate();
    EXPECT_FLOAT_EQ(registry().get<components::Size>(child).size.x(), 0.0F);
    EXPECT_FLOAT_EQ(registry().get<components::Size>(child).size.y(), 0.0F);
}

TEST_F(AnchorsTest, RejectsConflictingNonFiniteAndInvalidAnchors)
{
    const auto root = makeRoot();
    const auto child = makeFixedChild();
    attach(root, child);

    // 同轴 center 与任一 edge 冲突（Qt Quick 语义）。
    AnchorSet centerAndLeft;
    centerAndLeft.set(AnchorLine::HorizontalCenter, AnchorTarget::Parent(), 0.0F);
    centerAndLeft.set(AnchorLine::Left, AnchorTarget::Parent(), 1.0F);
    EXPECT_FALSE(layout::SetAnchors(child, centerAndLeft).has_value());

    AnchorSet centerAndBottom;
    centerAndBottom.set(AnchorLine::VerticalCenter, AnchorTarget::Parent(), 0.0F);
    centerAndBottom.set(AnchorLine::Bottom, AnchorTarget::Parent(), 2.0F);
    EXPECT_FALSE(layout::SetAnchors(child, centerAndBottom).has_value());

    // baseline 与垂直方向 edge 互斥。
    AnchorSet baselineAndTop;
    baselineAndTop.set(AnchorLine::Baseline, AnchorTarget::Parent(), 0.0F);
    baselineAndTop.set(AnchorLine::Top, AnchorTarget::Parent(), 3.0F);
    EXPECT_FALSE(layout::SetAnchors(child, baselineAndTop).has_value());

    // 校验失败不得写入任何状态。
    EXPECT_FALSE(registry().all_of<components::Anchors>(child));

    ParentAnchors nonFinite;
    nonFinite.top = std::numeric_limits<float>::infinity();
    EXPECT_FALSE(layout::SetAnchors(child, nonFinite).has_value());

    EXPECT_FALSE(layout::SetAnchors(ui::null_entity, ParentAnchors::Fill()).has_value());
}

TEST_F(AnchorsTest, NewApiSupportsLineAndZeroCenterOffset)
{
    const auto root = makeRoot();
    const auto child = makeFixedChild();
    attach(root, child);

    ASSERT_TRUE(layout::SetAnchor(child, AnchorLine::Left, AnchorTarget::Parent(), 12.0F).has_value());
    ASSERT_TRUE(layout::SetAnchor(child, AnchorLine::Top, AnchorTarget::Parent(), 18.0F).has_value());
    calculate();

    EXPECT_FLOAT_EQ(registry().get<components::Position>(child).value.x(), 12.0F);
    EXPECT_FLOAT_EQ(registry().get<components::Position>(child).value.y(), 18.0F);

    ASSERT_TRUE(layout::SetAnchorCenterIn(child).has_value());
    calculate();
    EXPECT_NEAR(registry().get<components::Position>(child).value.x(), 160.0F, 0.01F);
    EXPECT_NEAR(registry().get<components::Position>(child).value.y(), 130.0F, 0.01F);
}

TEST_F(AnchorsTest, SiblingTargetUsesParentLocalGeometry)
{
    const auto root = makeRoot();
    const auto first = makeFixedChild(40.0F, 20.0F);
    const auto second = makeFixedChild(30.0F, 20.0F);
    attach(root, first);
    attach(root, second);

    ASSERT_TRUE(layout::SetAnchorFill(first).has_value());
    ASSERT_TRUE(layout::SetAnchor(second, AnchorLine::Left, AnchorTarget::Sibling(first), AnchorLine::Right, 5.0F)
                    .has_value());
    ASSERT_TRUE(layout::SetAnchor(second, AnchorLine::Top, AnchorTarget::Sibling(first), 0.0F).has_value());
    calculate();

    EXPECT_NEAR(registry().get<components::Position>(second).value.x(), 405.0F, 0.01F);
    EXPECT_NEAR(registry().get<components::Position>(second).value.y(), 0.0F, 0.01F);
}

TEST_F(AnchorsTest, ClearLineAndAllDoNotRestoreSizeSnapshot)
{
    const auto root = makeRoot();
    const auto child = makeFixedChild(80.0F, 40.0F);
    attach(root, child);

    ASSERT_TRUE(layout::SetAnchorFill(child, AnchorTarget::Parent(), 10.0F).has_value());
    registry().get<components::Size>(child).size = Vec2{25.0F, 15.0F};
    ASSERT_TRUE(layout::ClearAnchor(child, AnchorLine::Left).has_value());
    ASSERT_TRUE(layout::ClearAllAnchors(child).has_value());

    EXPECT_FALSE(registry().all_of<components::Anchors>(child));
    EXPECT_FLOAT_EQ(registry().get<components::Size>(child).size.x(), 25.0F);
    EXPECT_FLOAT_EQ(registry().get<components::Size>(child).size.y(), 15.0F);
}

TEST_F(AnchorsTest, SupportsNegativeAndExplicitMarginOverrides)
{
    const auto root = makeRoot();
    const auto child = makeFixedChild(20.0F, 20.0F);
    attach(root, child);

    AnchorSet anchors;
    anchors.set(AnchorLine::Left, AnchorTarget::Parent(), 1.0F);
    anchors.set(AnchorLine::Top, AnchorTarget::Parent(), 2.0F);
    anchors.leftMargin = -10.0F;
    anchors.topMargin = 7.0F;
    ASSERT_TRUE(layout::SetAnchors(child, anchors).has_value());
    calculate();

    EXPECT_FLOAT_EQ(registry().get<components::Position>(child).value.x(), -10.0F);
    EXPECT_FLOAT_EQ(registry().get<components::Position>(child).value.y(), 7.0F);
}

TEST_F(AnchorsTest, RejectsLongSiblingCycle)
{
    const auto root = makeRoot();
    const auto first = makeFixedChild();
    const auto second = makeFixedChild();
    const auto third = makeFixedChild();
    attach(root, first);
    attach(root, second);
    attach(root, third);

    ASSERT_TRUE(layout::SetAnchor(first, AnchorLine::Left, AnchorTarget::Sibling(second), AnchorLine::Right, 0.0F).has_value());
    ASSERT_TRUE(layout::SetAnchor(second, AnchorLine::Left, AnchorTarget::Sibling(third), AnchorLine::Right, 0.0F).has_value());
    EXPECT_FALSE(layout::SetAnchor(third, AnchorLine::Left, AnchorTarget::Sibling(first), AnchorLine::Right, 0.0F).has_value());
}

TEST_F(AnchorsTest, BaselineUsesExplicitOffsetAndNonTextDefaultsToZero)
{
    const auto root = makeRoot();
    const auto first = makeFixedChild(40.0F, 20.0F);
    const auto second = makeFixedChild(40.0F, 20.0F);
    attach(root, first);
    attach(root, second);

    AnchorSet firstAnchors;
    firstAnchors.baselineOffset = 12.0F;
    firstAnchors.set(AnchorLine::Top, AnchorTarget::Parent(), 30.0F);
    ASSERT_TRUE(layout::SetAnchors(first, firstAnchors).has_value());
    AnchorSet secondAnchors;
    secondAnchors.set(AnchorLine::Baseline, AnchorTarget::Sibling(first), AnchorLine::Baseline, 0.0F);
    ASSERT_TRUE(layout::SetAnchors(second, secondAnchors).has_value());
    calculate();

    EXPECT_FLOAT_EQ(registry().get<components::Position>(first).value.y(), 30.0F);
    EXPECT_FLOAT_EQ(registry().get<components::Position>(second).value.y(), 42.0F);
}

TEST_F(AnchorsTest, ClampsDoubleEdgeSizeToMaxSize)
{
    const auto root = makeRoot();
    const auto child = makeFixedChild();
    attach(root, child);
    auto& size = registry().get<components::Size>(child);
    size.minSize = Vec2{40.0F, 30.0F};
    size.maxSize = Vec2{260.0F, 200.0F};

    ASSERT_TRUE(layout::SetAnchorFill(child).has_value());
    calculate();

    // 双边约束派生 400x300，超过 maxSize 时统一裁剪到上限。
    EXPECT_FLOAT_EQ(registry().get<components::Size>(child).size.x(), 260.0F);
    EXPECT_FLOAT_EQ(registry().get<components::Size>(child).size.y(), 200.0F);
}

TEST_F(AnchorsTest, ClampsDoubleEdgeSizeToMinSize)
{
    const auto root = makeRoot();
    const auto first = makeFixedChild(30.0F, 20.0F);
    const auto second = makeFixedChild(10.0F, 10.0F);
    attach(root, first);
    attach(root, second);
    registry().get<components::Size>(second).minSize = Vec2{150.0F, 0.0F};

    ASSERT_TRUE(layout::SetAnchor(second, AnchorLine::Left, AnchorTarget::Sibling(first), AnchorLine::Left, 0.0F)
                    .has_value());
    ASSERT_TRUE(layout::SetAnchor(second, AnchorLine::Right, AnchorTarget::Sibling(first), AnchorLine::Right, 0.0F)
                    .has_value());
    calculate();

    // 双边派生宽度仅 30，低于 minSize 时抬升到 150。
    EXPECT_FLOAT_EQ(registry().get<components::Size>(second).size.x(), 150.0F);
}

TEST_F(AnchorsTest, DestroyedSiblingTargetClearsBindingAndReportsDiagnostic)
{
    const auto root = makeRoot();
    const auto first = makeFixedChild(40.0F, 20.0F);
    const auto second = makeFixedChild(30.0F, 20.0F);
    attach(root, first);
    attach(root, second);

    ASSERT_TRUE(layout::SetAnchor(second, AnchorLine::Left, AnchorTarget::Sibling(first), AnchorLine::Right, 0.0F)
                    .has_value());
    calculate();
    ASSERT_TRUE(registry().all_of<components::Anchors>(second));
    ASSERT_TRUE(static_cast<bool>(registry().get<components::Anchors>(second).value.get(AnchorLine::Left)));

    registry().destroy(ui::detail::ToInternal(first));
    calculate();

    const auto& anchors = registry().get<components::Anchors>(second);
    EXPECT_FALSE(static_cast<bool>(anchors.value.get(AnchorLine::Left)));
    EXPECT_EQ(anchors.lastDiagnostic, AnchorDiagnostic::TargetLost);
}

TEST_F(AnchorsTest, ReparentInvalidatesSiblingBindingWithDiagnostic)
{
    const auto root = makeRoot();
    const auto otherRoot = makeRoot(200.0F, 100.0F);
    const auto first = makeFixedChild(40.0F, 20.0F);
    const auto second = makeFixedChild(30.0F, 20.0F);
    attach(root, first);
    attach(root, second);
    ASSERT_TRUE(layout::SetAnchor(second, AnchorLine::Left, AnchorTarget::Sibling(first), AnchorLine::Right, 0.0F)
                    .has_value());

    hierarchy::AddChild(otherRoot, second);

    const auto& anchors = registry().get<components::Anchors>(second);
    EXPECT_FALSE(static_cast<bool>(anchors.value.get(AnchorLine::Left)));
    EXPECT_EQ(anchors.lastDiagnostic, AnchorDiagnostic::TargetLost);
    EXPECT_EQ(registry().get<components::Hierarchy>(second).parent, ui::detail::ToInternal(otherRoot));
}

}  // namespace
}  // namespace ui::tests