/**
 * ************************************************************************
 *
 * @file Entity.hpp
 * @author AnakinLiu (azrael2759@qq.com)
 * @date 2026-05-30
 * @version 0.1
 * @brief ui::entity — 公开 API 层对实体句柄别名的再导出。
 *
 * 实际定义在 common/EntityTypes.hpp，此文件仅做转发，
 * 保持 api/ 层命名一致性，供外部消费者直接包含。
 *
 * ************************************************************************
 * @copyright Copyright (c) 2026 AnakinLiu
 * For study and research only, no reprinting.
 * ************************************************************************
 */
#pragma once

// IWYU pragma: begin_exports
#include "common/EntityTypes.hpp"
// IWYU pragma: end_exports
