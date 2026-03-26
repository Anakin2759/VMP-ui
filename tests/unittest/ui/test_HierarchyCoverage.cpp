#include <gtest/gtest.h>

#include <ui.hpp>

#include "src/ui/api/Hierarchy.hpp"
#include "src/ui/common/Components.hpp"
#include "src/ui/common/Tags.hpp"
#include "src/ui/singleton/Registry.hpp"

namespace ui::tests
{

class HierarchyTest : public ::testing::Test
{
protected:
    void SetUp() override { Registry::Clear(); }
    void TearDown() override { Registry::Clear(); }
};

// ===================== AddChild =====================

TEST_F(HierarchyTest, AddChildSetsParentOnChild)
{
    const auto parent = factory::CreateVBoxLayout("h_parent_1");
    const auto child = factory::CreateLabel("C", "h_child_1");

    hierarchy::AddChild(parent, child);

    const auto* hier = Registry::TryGet<components::Hierarchy>(child);
    ASSERT_NE(hier, nullptr);
    EXPECT_EQ(hier->parent, parent);
}

TEST_F(HierarchyTest, AddChildAppendsToParentChildrenList)
{
    const auto parent = factory::CreateVBoxLayout("h_parent_2");
    const auto child = factory::CreateLabel("C", "h_child_2");

    hierarchy::AddChild(parent, child);

    const auto* pHier = Registry::TryGet<components::Hierarchy>(parent);
    ASSERT_NE(pHier, nullptr);
    ASSERT_EQ(pHier->children.size(), 1U);
    EXPECT_EQ(pHier->children[0], child);
}

TEST_F(HierarchyTest, AddChildRemovesRootTagFromChild)
{
    const auto parent = factory::CreateVBoxLayout("h_parent_root");
    const auto child = factory::CreateLabel("C", "h_child_root");
    // factory 已给 child 设置 RootTag（初始状态），验证 AddChild 后被移除
    Registry::EmplaceOrReplace<components::RootTag>(child);

    hierarchy::AddChild(parent, child);

    EXPECT_FALSE(Registry::AllOf<components::RootTag>(child));
}

TEST_F(HierarchyTest, AddChildIsIdempotentOnDoubleCall)
{
    const auto parent = factory::CreateVBoxLayout("h_parent_idem");
    const auto child = factory::CreateLabel("C", "h_child_idem");

    hierarchy::AddChild(parent, child);
    hierarchy::AddChild(parent, child); // 重复添加

    const auto* pHier = Registry::TryGet<components::Hierarchy>(parent);
    ASSERT_NE(pHier, nullptr);
    // children 中不应出现重复
    EXPECT_EQ(pHier->children.size(), 1U);
}

TEST_F(HierarchyTest, AddChildReparentsFromOldParent)
{
    const auto parentA = factory::CreateVBoxLayout("h_parent_a");
    const auto parentB = factory::CreateVBoxLayout("h_parent_b");
    const auto child = factory::CreateLabel("C", "h_child_reparent");

    hierarchy::AddChild(parentA, child);
    hierarchy::AddChild(parentB, child); // 重新挂到 B 下

    const auto* cHier = Registry::TryGet<components::Hierarchy>(child);
    const auto* pAHier = Registry::TryGet<components::Hierarchy>(parentA);
    const auto* pBHier = Registry::TryGet<components::Hierarchy>(parentB);

    ASSERT_NE(cHier, nullptr);
    ASSERT_NE(pAHier, nullptr);
    ASSERT_NE(pBHier, nullptr);

    EXPECT_EQ(cHier->parent, parentB);
    EXPECT_TRUE(pAHier->children.empty());
    ASSERT_EQ(pBHier->children.size(), 1U);
    EXPECT_EQ(pBHier->children[0], child);
}

TEST_F(HierarchyTest, AddChildWithInvalidParentIsNoOp)
{
    const auto child = factory::CreateLabel("C", "h_null_parent");
    EXPECT_NO_FATAL_FAILURE(hierarchy::AddChild(entt::null, child));
}

TEST_F(HierarchyTest, AddChildWithInvalidChildIsNoOp)
{
    const auto parent = factory::CreateVBoxLayout("h_null_child_parent");
    EXPECT_NO_FATAL_FAILURE(hierarchy::AddChild(parent, entt::null));
}

// ===================== RemoveChild =====================

TEST_F(HierarchyTest, RemoveChildClearsParentOnChild)
{
    const auto parent = factory::CreateVBoxLayout("h_remove_parent");
    const auto child = factory::CreateLabel("C", "h_remove_child");

    hierarchy::AddChild(parent, child);
    hierarchy::RemoveChild(parent, child);

    const auto* cHier = Registry::TryGet<components::Hierarchy>(child);
    ASSERT_NE(cHier, nullptr);
    EXPECT_TRUE(cHier->parent == entt::null);
}

TEST_F(HierarchyTest, RemoveChildErasesFromParentChildrenList)
{
    const auto parent = factory::CreateVBoxLayout("h_remove_list_parent");
    const auto child = factory::CreateLabel("C", "h_remove_list_child");

    hierarchy::AddChild(parent, child);
    hierarchy::RemoveChild(parent, child);

    const auto* pHier = Registry::TryGet<components::Hierarchy>(parent);
    ASSERT_NE(pHier, nullptr);
    EXPECT_TRUE(pHier->children.empty());
}

TEST_F(HierarchyTest, RemoveChildRestoresRootTagOnChild)
{
    const auto parent = factory::CreateVBoxLayout("h_roottag_parent");
    const auto child = factory::CreateLabel("C", "h_roottag_child");

    hierarchy::AddChild(parent, child);
    ASSERT_FALSE(Registry::AllOf<components::RootTag>(child)); // AddChild 已移除

    hierarchy::RemoveChild(parent, child);

    EXPECT_TRUE(Registry::AllOf<components::RootTag>(child));
}

TEST_F(HierarchyTest, RemoveChildOnNonChildEntityIsNoOp)
{
    const auto parent = factory::CreateVBoxLayout("h_nonchild_parent");
    const auto stranger = factory::CreateLabel("S", "h_stranger");

    // stranger 从未被添加为 parent 的子节点
    EXPECT_NO_FATAL_FAILURE(hierarchy::RemoveChild(parent, stranger));

    const auto* pHier = Registry::TryGet<components::Hierarchy>(parent);
    // children 应保持为空
    if (pHier != nullptr)
    {
        EXPECT_TRUE(pHier->children.empty());
    }
}

TEST_F(HierarchyTest, RemoveChildWithInvalidEntitiesIsNoOp)
{
    EXPECT_NO_FATAL_FAILURE(hierarchy::RemoveChild(entt::null, entt::null));
}

// ===================== TraverseChildren =====================

TEST_F(HierarchyTest, TraverseChildrenVisitsAllDescendantsPreOrder)
{
    //   parent
    //   ├── child1
    //   │   └── grandchild
    //   └── child2
    const auto parent = factory::CreateVBoxLayout("h_traverse_parent");
    const auto child1 = factory::CreateLabel("C1", "h_traverse_c1");
    const auto child2 = factory::CreateLabel("C2", "h_traverse_c2");
    const auto grandchild = factory::CreateLabel("GC", "h_traverse_gc");

    hierarchy::AddChild(parent, child1);
    hierarchy::AddChild(child1, grandchild);
    hierarchy::AddChild(parent, child2);

    std::vector<entt::entity> visited;
    hierarchy::TraverseChildren(parent, [&visited](entt::entity e) { visited.push_back(e); });

    // TraverseChildren 是后序（先递归子节点再访问当前节点）——按实现实际顺序断言：
    // grandchild → child1 → child2
    ASSERT_EQ(visited.size(), 3U);
    EXPECT_EQ(visited[0], grandchild);
    EXPECT_EQ(visited[1], child1);
    EXPECT_EQ(visited[2], child2);
}

TEST_F(HierarchyTest, TraverseChildrenOnLeafEntityCallsVisitorZeroTimes)
{
    const auto leaf = factory::CreateLabel("Leaf", "h_traverse_leaf");

    int callCount = 0;
    hierarchy::TraverseChildren(leaf, [&callCount](entt::entity) { ++callCount; });

    EXPECT_EQ(callCount, 0);
}

} // namespace ui::tests
