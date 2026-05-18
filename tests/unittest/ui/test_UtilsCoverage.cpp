#include <gtest/gtest.h>

#include <ui.hpp>

#include "src/ui/api/Hierarchy.hpp"
#include "src/ui/api/Utils.hpp"
#include "src/ui/common/Components.hpp"
#include "src/ui/common/Tags.hpp"
#include "src/ui/singleton/Registry.hpp"

namespace ui::tests
{

class UtilsTest : public ::testing::Test
{
protected:
    UiRuntime m_runtime;
    std::unique_ptr<UiRuntimeScope> m_scope;

    void SetUp() override { m_scope = std::make_unique<UiRuntimeScope>(m_runtime); }
    void TearDown() override { m_scope.reset(); }
};

// ===================== MarkLayoutChanged =====================

TEST_F(UtilsTest, MarkLayoutChangedTagsEntityWithLayoutDirtyTag)
{
    const auto entity = factory::CreateLabel("L", "util_label_1");
    Registry::Remove<components::LayoutDirtyTag>(entity);

    utils::MarkLayoutChanged(entity);

    EXPECT_TRUE(Registry::AllOf<components::LayoutDirtyTag>(entity));
}

TEST_F(UtilsTest, MarkLayoutChangedBubblesToParentAndGrandparent)
{
    const auto grandparent = factory::CreateVBoxLayout("gp_layout");
    const auto parent = factory::CreateVBoxLayout("p_layout");
    const auto child = factory::CreateLabel("C", "child_layout");

    hierarchy::AddChild(grandparent, parent);
    hierarchy::AddChild(parent, child);

    for (auto e : {grandparent, parent, child})
        Registry::Remove<components::LayoutDirtyTag>(e);

    utils::MarkLayoutChanged(child);

    EXPECT_TRUE(Registry::AllOf<components::LayoutDirtyTag>(child));
    EXPECT_TRUE(Registry::AllOf<components::LayoutDirtyTag>(parent));
    EXPECT_TRUE(Registry::AllOf<components::LayoutDirtyTag>(grandparent));
}

TEST_F(UtilsTest, MarkLayoutChangedOnLeafDoesNotTagSibling)
{
    const auto parent = factory::CreateVBoxLayout("sib_parent");
    const auto child1 = factory::CreateLabel("C1", "child1_layout");
    const auto child2 = factory::CreateLabel("C2", "child2_layout");

    hierarchy::AddChild(parent, child1);
    hierarchy::AddChild(parent, child2);

    for (auto e : {parent, child1, child2})
        Registry::Remove<components::LayoutDirtyTag>(e);

    utils::MarkLayoutChanged(child1);

    EXPECT_TRUE(Registry::AllOf<components::LayoutDirtyTag>(child1));
    EXPECT_FALSE(Registry::AllOf<components::LayoutDirtyTag>(child2));
}

TEST_F(UtilsTest, MarkLayoutChangedOnNullEntityIsNoOp)
{
    EXPECT_NO_FATAL_FAILURE(utils::MarkLayoutChanged(entt::null));
}

// ===================== MarkVisualChanged =====================

TEST_F(UtilsTest, MarkVisualChangedTagsEntityWithRenderDirtyTag)
{
    const auto entity = factory::CreateLabel("V", "util_vis_1");
    Registry::Remove<components::RenderDirtyTag>(entity);

    utils::MarkVisualChanged(entity);

    EXPECT_TRUE(Registry::AllOf<components::RenderDirtyTag>(entity));
}

TEST_F(UtilsTest, MarkVisualChangedAlsoTagsAncestorWindowEntity)
{
    // 创建一个携带 WindowTag 的祖先实体
    const auto windowEntity = Registry::Create();
    Registry::Emplace<components::BaseInfo>(windowEntity).alias = "win_root";
    Registry::Emplace<components::WindowTag>(windowEntity);
    Registry::Emplace<components::Hierarchy>(windowEntity);

    const auto child = factory::CreateLabel("CW", "child_under_window");
    hierarchy::AddChild(windowEntity, child);

    Registry::Remove<components::RenderDirtyTag>(windowEntity);
    Registry::Remove<components::RenderDirtyTag>(child);

    utils::MarkVisualChanged(child);

    EXPECT_TRUE(Registry::AllOf<components::RenderDirtyTag>(child));
    EXPECT_TRUE(Registry::AllOf<components::RenderDirtyTag>(windowEntity));
}

TEST_F(UtilsTest, MarkVisualChangedOnEntityWithoutWindowAncestorOnlyTagsSelf)
{
    const auto entity = factory::CreateLabel("Alone", "util_vis_alone");
    Registry::Remove<components::RenderDirtyTag>(entity);

    utils::MarkVisualChanged(entity);

    EXPECT_TRUE(Registry::AllOf<components::RenderDirtyTag>(entity));
}

TEST_F(UtilsTest, MarkVisualChangedOnNullEntityIsNoOp)
{
    EXPECT_NO_FATAL_FAILURE(utils::MarkVisualChanged(entt::null));
}

// ===================== MarkLayoutAndVisualChanged =====================

TEST_F(UtilsTest, MarkLayoutAndVisualChangedSetsBothDirtyTags)
{
    const auto entity = factory::CreateLabel("Both", "util_both_dirty");
    Registry::Remove<components::LayoutDirtyTag>(entity);
    Registry::Remove<components::RenderDirtyTag>(entity);

    utils::MarkLayoutAndVisualChanged(entity);

    EXPECT_TRUE(Registry::AllOf<components::LayoutDirtyTag>(entity));
    EXPECT_TRUE(Registry::AllOf<components::RenderDirtyTag>(entity));
}

// ===================== HasAlignment =====================

TEST_F(UtilsTest, HasAlignmentReturnsTrueForExactMatch)
{
    EXPECT_TRUE(utils::HasAlignment(policies::Alignment::LEFT, policies::Alignment::LEFT));
}

TEST_F(UtilsTest, HasAlignmentReturnsTrueForCombinedFlag)
{
    // CENTER = HCENTER | VCENTER, 测试其中一个子标志
    EXPECT_TRUE(utils::HasAlignment(policies::Alignment::CENTER, policies::Alignment::HCENTER));
    EXPECT_TRUE(utils::HasAlignment(policies::Alignment::CENTER, policies::Alignment::VCENTER));
}

TEST_F(UtilsTest, HasAlignmentReturnsFalseWhenFlagAbsent)
{
    EXPECT_FALSE(utils::HasAlignment(policies::Alignment::LEFT, policies::Alignment::RIGHT));
    EXPECT_FALSE(utils::HasAlignment(policies::Alignment::TOP, policies::Alignment::BOTTOM));
}

// ===================== IsEntityExist =====================

TEST_F(UtilsTest, IsEntityExistReturnsTrueForRegisteredAlias)
{
    factory::CreateLabel("Exists", "exist_check_alias");

    EXPECT_TRUE(utils::IsEntityExist("exist_check_alias"));
}

TEST_F(UtilsTest, IsEntityExistReturnsFalseForUnknownAlias)
{
    EXPECT_FALSE(utils::IsEntityExist("totally_unknown_alias_xyz"));
}

TEST_F(UtilsTest, IsEntityExistReturnsFalseAfterRegistryClear)
{
    factory::CreateLabel("Gone", "gone_alias");
    Registry::Clear();

    EXPECT_FALSE(utils::IsEntityExist("gone_alias"));
}

} // namespace ui::tests
