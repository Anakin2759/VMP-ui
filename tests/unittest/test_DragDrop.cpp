#include <gtest/gtest.h>

#include <memory>
#include <ui.hpp>

#include "common/components/Interaction.hpp"
#include "common/components/Layout.hpp"
#include "entt/entity/entity.hpp"
#include "src/api/Controls.hpp"
#include "src/api/Hierarchy.hpp"
#include "src/common/Events.hpp"
#include "src/common/Tags.hpp"
#include "src/core/UiRuntime.hpp"
#include "src/singleton/Dispatcher.hpp"
#include "src/singleton/Registry.hpp"
#include "src/systems/ActionSystem.hpp"

namespace ui::tests
{
namespace
{

class DragDropTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        m_scope = std::make_unique<UiRuntimeScope>(m_runtime);
        m_actionSystem.registerHandlers();
    }

    void TearDown() override
    {
        m_actionSystem.unregisterHandlers();
        m_scope.reset();
    }

    systems::ActionSystem m_actionSystem;

private:
    UiRuntime m_runtime;
    std::unique_ptr<UiRuntimeScope> m_scope;
};

} // namespace

// ===================== SetDraggable DSL =====================

TEST_F(DragDropTest, SetDraggableAddsComponent)
{
    const auto entity = factory::CreateLabel("D", "drag_entity_1");

    controls::SetDraggable(entity, true);

    const auto* comp = Registry::TryGet<components::Draggable>(entity);
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->enabled, policies::Feature::ENABLED);
}

TEST_F(DragDropTest, SetDraggableFalseDisablesComponent)
{
    const auto entity = factory::CreateLabel("D", "drag_entity_2");

    controls::SetDraggable(entity, false);

    const auto* comp = Registry::TryGet<components::Draggable>(entity);
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->enabled, policies::Feature::DISABLED);
}

TEST_F(DragDropTest, SetDragLockAxisPreservesFlags)
{
    const auto entity = factory::CreateLabel("D", "drag_lock_1");

    controls::SetDragLockAxis(entity, true, false);

    const auto* comp = Registry::TryGet<components::Draggable>(entity);
    ASSERT_NE(comp, nullptr);
    EXPECT_TRUE(comp->lockX);
    EXPECT_FALSE(comp->lockY);
}

TEST_F(DragDropTest, SetOnDragStartStoresCallback)
{
    const auto entity = factory::CreateLabel("D", "drag_cb_1");
    bool called = false;

    controls::SetOnDragStart(entity, [&called] { called = true; });

    auto* comp = Registry::TryGet<components::Draggable>(entity);
    ASSERT_NE(comp, nullptr);
    ASSERT_TRUE(static_cast<bool>(comp->onDragStart));
    comp->onDragStart();
    EXPECT_TRUE(called);
}

TEST_F(DragDropTest, SetOnDragEndStoresCallback)
{
    const auto entity = factory::CreateLabel("D", "drag_cb_2");
    bool called = false;

    controls::SetOnDragEnd(entity, [&called] { called = true; });

    auto* comp = Registry::TryGet<components::Draggable>(entity);
    ASSERT_NE(comp, nullptr);
    ASSERT_TRUE(static_cast<bool>(comp->onDragEnd));
    comp->onDragEnd();
    EXPECT_TRUE(called);
}

TEST_F(DragDropTest, SetOnDragMoveStoresCallback)
{
    const auto entity = factory::CreateLabel("D", "drag_cb_3");
    Vec2 received{0.0F, 0.0F};

    controls::SetOnDragMove(entity, [&received](Vec2 delta) { received = delta; });

    auto* comp = Registry::TryGet<components::Draggable>(entity);
    ASSERT_NE(comp, nullptr);
    ASSERT_TRUE(static_cast<bool>(comp->onDragMove));
    comp->onDragMove(Vec2{3.0F, 5.0F});
    EXPECT_FLOAT_EQ(received.x(), 3.0F);
    EXPECT_FLOAT_EQ(received.y(), 5.0F);
}

// ===================== SetDroppable DSL =====================

TEST_F(DragDropTest, SetDroppableAddsComponent)
{
    const auto entity = factory::CreateVBoxLayout("drop_entity_1");

    controls::SetDroppable(entity, true);

    const auto* comp = Registry::TryGet<components::Droppable>(entity);
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->enabled, policies::Feature::ENABLED);
}

TEST_F(DragDropTest, SetDroppableFalseDisablesComponent)
{
    const auto entity = factory::CreateVBoxLayout("drop_entity_2");

    controls::SetDroppable(entity, false);

    const auto* comp = Registry::TryGet<components::Droppable>(entity);
    ASSERT_NE(comp, nullptr);
    EXPECT_EQ(comp->enabled, policies::Feature::DISABLED);
}

// ===================== DragDroppedEvent → AddChild（默认处理路径）=====================

TEST_F(DragDropTest, DragDroppedReparentsSourceUnderTarget)
{
    const auto source = factory::CreateLabel("SRC", "dd_source_1");
    const auto target = factory::CreateVBoxLayout("dd_target_1");

    // target 必须有 Droppable（enabled）
    controls::SetDroppable(target, true);

    Dispatcher::Trigger<events::DragDroppedEvent>({.source = source, .target = target});

    const auto* cHier = Registry::TryGet<components::Hierarchy>(source);
    ASSERT_NE(cHier, nullptr);
    EXPECT_EQ(cHier->parent, target);
}

TEST_F(DragDropTest, DragDroppedIsBlockedWhenTargetNotDroppable)
{
    const auto source = factory::CreateLabel("SRC", "dd_source_2");
    const auto target = factory::CreateVBoxLayout("dd_target_2");

    // target 没有 Droppable 组件
    Dispatcher::Trigger<events::DragDroppedEvent>({.source = source, .target = target});

    // source 不应被移到 target 下
    const auto* cHier = Registry::TryGet<components::Hierarchy>(source);
    if (cHier != nullptr)
    {
        EXPECT_NE(cHier->parent, target);
    }
}

TEST_F(DragDropTest, DragDroppedIsBlockedWhenTargetDroppableDisabled)
{
    const auto source = factory::CreateLabel("SRC", "dd_source_3");
    const auto target = factory::CreateVBoxLayout("dd_target_3");

    controls::SetDroppable(target, false); // 存在但禁用

    Dispatcher::Trigger<events::DragDroppedEvent>({.source = source, .target = target});

    const auto* cHier = Registry::TryGet<components::Hierarchy>(source);
    if (cHier != nullptr)
    {
        EXPECT_NE(cHier->parent, target);
    }
}

TEST_F(DragDropTest, DragDroppedIsBlockedByDroppingOnSelf)
{
    const auto entity = factory::CreateLabel("SELF", "dd_self_1");
    controls::SetDroppable(entity, true);

    // 把自己拖到自己上
    Dispatcher::Trigger<events::DragDroppedEvent>({.source = entity, .target = entity});

    // 不能成为自己的父节点
    const auto* cHier = Registry::TryGet<components::Hierarchy>(entity);
    if (cHier != nullptr)
    {
        EXPECT_NE(cHier->parent, entity);
    }
}

// ===================== 循环检测 =====================

TEST_F(DragDropTest, DragDroppedIsBlockedByDirectCycle)
{
    //  parent → child，再尝试把 parent 拖到 child 下 → 循环
    const auto parent = factory::CreateVBoxLayout("dd_cycle_p1");
    const auto child = factory::CreateLabel("C", "dd_cycle_c1");

    hierarchy::AddChild(parent, child);
    controls::SetDroppable(child, true);

    Dispatcher::Trigger<events::DragDroppedEvent>({.source = parent, .target = child});

    // parent 不应成为 child 的子节点（循环）
    const auto* pHier = Registry::TryGet<components::Hierarchy>(parent);
    if (pHier != nullptr)
    {
        EXPECT_NE(pHier->parent, child);
    }
}

TEST_F(DragDropTest, DragDroppedIsBlockedByIndirectCycle)
{
    //  grandparent → parent → child；尝试 grandparent 拖到 child 下
    const auto grandparent = factory::CreateVBoxLayout("dd_cycle_gp");
    const auto parent = factory::CreateVBoxLayout("dd_cycle_p2");
    const auto child = factory::CreateLabel("C", "dd_cycle_c2");

    hierarchy::AddChild(grandparent, parent);
    hierarchy::AddChild(parent, child);
    controls::SetDroppable(child, true);

    Dispatcher::Trigger<events::DragDroppedEvent>({.source = grandparent, .target = child});

    const auto* gpHier = Registry::TryGet<components::Hierarchy>(grandparent);
    if (gpHier != nullptr)
    {
        EXPECT_NE(gpHier->parent, child);
    }
}

TEST_F(DragDropTest, DragDroppedAllowsValidDropOnUnrelatedTarget)
{
    const auto source = factory::CreateLabel("SRC", "dd_valid_src");
    const auto unrelated = factory::CreateVBoxLayout("dd_valid_tgt");

    controls::SetDroppable(unrelated, true);

    Dispatcher::Trigger<events::DragDroppedEvent>({.source = source, .target = unrelated});

    const auto* cHier = Registry::TryGet<components::Hierarchy>(source);
    ASSERT_NE(cHier, nullptr);
    EXPECT_EQ(cHier->parent, unrelated);
}

// ===================== DroppableTag（标签路径）=====================

TEST_F(DragDropTest, DroppableTagAloneEnablesDrop)
{
    const auto source = factory::CreateLabel("SRC", "dd_tag_src");
    const auto target = factory::CreateVBoxLayout("dd_tag_tgt");

    // 只打 DroppableTag，不用 SetDroppable
    Registry::Emplace<components::DroppableTag>(target);

    Dispatcher::Trigger<events::DragDroppedEvent>({.source = source, .target = target});

    const auto* cHier = Registry::TryGet<components::Hierarchy>(source);
    ASSERT_NE(cHier, nullptr);
    EXPECT_EQ(cHier->parent, target);
}

// ===================== DragDroppedEvent 对无效实体是 no-op =====================

TEST_F(DragDropTest, DragDroppedWithInvalidSourceIsNoOp)
{
    const auto target = factory::CreateVBoxLayout("dd_null_tgt");
    controls::SetDroppable(target, true);

    EXPECT_NO_FATAL_FAILURE(Dispatcher::Trigger<events::DragDroppedEvent>({.source = entt::null, .target = target}));
}

TEST_F(DragDropTest, DragDroppedWithInvalidTargetIsNoOp)
{
    const auto source = factory::CreateLabel("SRC", "dd_null_src");

    EXPECT_NO_FATAL_FAILURE(Dispatcher::Trigger<events::DragDroppedEvent>({.source = source, .target = entt::null}));
}

} // namespace ui::tests
