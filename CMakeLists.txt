cmake_minimum_required(VERSION 3.31)
project(VMP-ui VERSION 0.1.0 LANGUAGES CXX)


set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CheckIPOSupported)
include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

option(ENABLE_BUILD_TESTS "Enable building of unit tests" OFF)
option(ENABLE_BUILD_BENCHMARKS "Enable building of benchmarks (Google Benchmark)" OFF)
option(BUILD_EXAMPLES "Enable building examples" ON)
option(BUILD_SHARED_LIBS "Build VMPUI as a shared library" OFF)
# 第三方依赖必须保持静态，以便静态模式合并为单库、动态模式链接进单 DLL。
# 保存用户对 VMPUI 本身的选择；配置第三方期间会临时覆盖 BUILD_SHARED_LIBS。
set(_VMPUI_BUILD_SHARED_LIBS "${BUILD_SHARED_LIBS}")
if(_VMPUI_BUILD_SHARED_LIBS)
    # 非 Windows 平台将静态第三方对象链接进 VMPUI 共享库时需要 PIC。
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()
# IPO/LTO 默认关闭：自包含发行模式下合并后的静态库含第三方对象，开启 LTO 会要求
# 消费者链接时使用相同 LTO 配置（LTCG / ThinLTO），破坏"即插即用"。需要时显式启用：
#   cmake -B build -DENABLE_LTO=ON
option(ENABLE_LTO "Enable IPO/LTO for Release and RelWithDebInfo builds" OFF)
option(ENABLE_CLANG_TIDY "Enable clang-tidy static analysis" OFF)

# 可选编译缓存：AUTO 优先使用 sccache，其次 ccache；未安装时保持常规增量构建。
# ON 要求缓存程序存在，OFF 完全禁用。launcher 必须在创建第三方和项目目标前设置。
set(UI_COMPILER_CACHE "AUTO" CACHE STRING "Compiler cache mode: AUTO, ON, or OFF")
set_property(CACHE UI_COMPILER_CACHE PROPERTY STRINGS AUTO ON OFF)
if(NOT UI_COMPILER_CACHE MATCHES "^(AUTO|ON|OFF)$")
    message(FATAL_ERROR "UI_COMPILER_CACHE must be AUTO, ON, or OFF")
endif()
if(NOT UI_COMPILER_CACHE STREQUAL "OFF")
    find_program(UI_COMPILER_CACHE_PROGRAM NAMES sccache ccache)
    if(UI_COMPILER_CACHE_PROGRAM)
        set(CMAKE_C_COMPILER_LAUNCHER "${UI_COMPILER_CACHE_PROGRAM}")
        set(CMAKE_CXX_COMPILER_LAUNCHER "${UI_COMPILER_CACHE_PROGRAM}")
        message(STATUS "Compiler cache enabled: ${UI_COMPILER_CACHE_PROGRAM}")
    elseif(UI_COMPILER_CACHE STREQUAL "ON")
        message(FATAL_ERROR
            "UI_COMPILER_CACHE=ON but neither sccache nor ccache was found")
    else()
        message(STATUS "Compiler cache not found; using Ninja dependency-based incremental builds")
    endif()
endif()

# 参考 LLVM HandleLLVMOptions.cmake：根据配置时的可用物理内存和逻辑核心数
# 自动计算 Ninja job pool 深度。显式 UI_PARALLEL_*_JOBS 优先于自动结果。
# clang-cl 编译本项目的 EnTT/Eigen 重型 TU 峰值约 3 GiB，保守预留 4 GiB；
# 链接/静态库合并同样预留 4 GiB，避免与编译任务叠加时耗尽系统提交量。
set(UI_RAM_PER_COMPILE_JOB 4096 CACHE STRING
    "Estimated available RAM required per compiler job in MiB (0 disables auto limit)")
set(UI_RAM_PER_LINK_JOB 4096 CACHE STRING
    "Estimated available RAM required per linker job in MiB (0 disables auto limit)")
set(UI_PARALLEL_COMPILE_JOBS "" CACHE STRING
    "Explicit maximum concurrent compiler jobs (empty = derive from available RAM)")
set(UI_PARALLEL_LINK_JOBS "" CACHE STRING
    "Explicit maximum concurrent linker jobs (empty = derive from available RAM)")

if(CMAKE_GENERATOR MATCHES "Ninja")
    foreach(_ui_memory_value IN ITEMS UI_RAM_PER_COMPILE_JOB UI_RAM_PER_LINK_JOB)
        if(NOT "${${_ui_memory_value}}" MATCHES "^[0-9]+$")
            message(FATAL_ERROR "${_ui_memory_value} must be a non-negative integer")
        endif()
    endforeach()
    foreach(_ui_jobs_value IN ITEMS UI_PARALLEL_COMPILE_JOBS UI_PARALLEL_LINK_JOBS)
        if(NOT "${${_ui_jobs_value}}" STREQUAL "" AND
           NOT "${${_ui_jobs_value}}" MATCHES "^[1-9][0-9]*$")
            message(FATAL_ERROR "${_ui_jobs_value} must be a positive integer or empty")
        endif()
    endforeach()

    cmake_host_system_information(
        RESULT _UI_AVAILABLE_PHYSICAL_MEMORY_MB
        QUERY AVAILABLE_PHYSICAL_MEMORY)
    cmake_host_system_information(
        RESULT _UI_LOGICAL_CORES
        QUERY NUMBER_OF_LOGICAL_CORES)

    set(_UI_EFFECTIVE_COMPILE_JOB_LIMIT "${UI_PARALLEL_COMPILE_JOBS}")
    if(NOT _UI_EFFECTIVE_COMPILE_JOB_LIMIT AND UI_RAM_PER_COMPILE_JOB GREATER 0)
        math(EXPR _UI_EFFECTIVE_COMPILE_JOB_LIMIT
            "${_UI_AVAILABLE_PHYSICAL_MEMORY_MB} / ${UI_RAM_PER_COMPILE_JOB}")
        if(_UI_EFFECTIVE_COMPILE_JOB_LIMIT LESS 1)
            set(_UI_EFFECTIVE_COMPILE_JOB_LIMIT 1)
        elseif(_UI_EFFECTIVE_COMPILE_JOB_LIMIT GREATER _UI_LOGICAL_CORES)
            set(_UI_EFFECTIVE_COMPILE_JOB_LIMIT ${_UI_LOGICAL_CORES})
        endif()
    endif()

    set(_UI_EFFECTIVE_LINK_JOB_LIMIT "${UI_PARALLEL_LINK_JOBS}")
    if(NOT _UI_EFFECTIVE_LINK_JOB_LIMIT AND UI_RAM_PER_LINK_JOB GREATER 0)
        math(EXPR _UI_EFFECTIVE_LINK_JOB_LIMIT
            "${_UI_AVAILABLE_PHYSICAL_MEMORY_MB} / ${UI_RAM_PER_LINK_JOB}")
        if(_UI_EFFECTIVE_LINK_JOB_LIMIT LESS 1)
            set(_UI_EFFECTIVE_LINK_JOB_LIMIT 1)
        elseif(_UI_EFFECTIVE_LINK_JOB_LIMIT GREATER _UI_LOGICAL_CORES)
            set(_UI_EFFECTIVE_LINK_JOB_LIMIT ${_UI_LOGICAL_CORES})
        endif()
    endif()

    if(_UI_EFFECTIVE_COMPILE_JOB_LIMIT)
        set_property(GLOBAL APPEND PROPERTY JOB_POOLS
            ui_compile_pool=${_UI_EFFECTIVE_COMPILE_JOB_LIMIT})
        set(CMAKE_JOB_POOL_COMPILE ui_compile_pool)
    else()
        set(_UI_EFFECTIVE_COMPILE_JOB_LIMIT "unlimited")
    endif()
    if(_UI_EFFECTIVE_LINK_JOB_LIMIT)
        set_property(GLOBAL APPEND PROPERTY JOB_POOLS
            ui_link_pool=${_UI_EFFECTIVE_LINK_JOB_LIMIT})
        set(CMAKE_JOB_POOL_LINK ui_link_pool)
    else()
        set(_UI_EFFECTIVE_LINK_JOB_LIMIT "unlimited")
    endif()
    message(STATUS
        "Ninja memory-aware job pools: available=${_UI_AVAILABLE_PHYSICAL_MEMORY_MB} MiB, "
        "logical-cores=${_UI_LOGICAL_CORES}, compile=${_UI_EFFECTIVE_COMPILE_JOB_LIMIT} "
        "(${UI_RAM_PER_COMPILE_JOB} MiB/job), link=${_UI_EFFECTIVE_LINK_JOB_LIMIT} "
        "(${UI_RAM_PER_LINK_JOB} MiB/job)")
endif()

#==================== IPO / LTO 设置 ====================
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)

if(ENABLE_LTO AND ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        add_compile_options($<$<CONFIG:Release,RelWithDebInfo>:-flto=auto>)
        add_link_options($<$<CONFIG:Release,RelWithDebInfo>:-flto=auto>)
        message(STATUS "IPO / LTO enabled for Release configs with GCC parallel LTO")
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        if(CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
            message(STATUS "IPO / LTO enabled for Release configs with clang-cl via CMake IPO")
        else()
            add_compile_options($<$<CONFIG:Release,RelWithDebInfo>:-flto=thin>)
            add_link_options($<$<CONFIG:Release,RelWithDebInfo>:-flto=thin>)
            message(STATUS "IPO / LTO enabled for Release configs with Clang ThinLTO")
        endif()
    elseif(MSVC)
        message(STATUS "IPO / LTO enabled for Release configs with MSVC full LTCG")
    else()
        message(STATUS "IPO / LTO enabled for Release configs")
    endif()
else()
    if(NOT ENABLE_LTO)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE FALSE)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO FALSE)
        message(STATUS "IPO / LTO disabled (ENABLE_LTO=OFF)")
    else()
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE FALSE)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO FALSE)
        message(WARNING "IPO / LTO not supported: ${ipo_error}")
    endif()
endif()
#==================== 全局设置 ====================
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF) #禁用编译器扩展
set(CMAKE_EXPORT_COMPILE_COMMANDS ON) #生成compile_commands.json文件

# 必须在 project() 之后立即添加
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# 针对旧版 CMake 或某些第三方库的兼容处理
if(MSVC OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_SIMULATE_ID STREQUAL "MSVC"))
    set(variables 
        CMAKE_C_FLAGS_RELEASE 
        CMAKE_C_FLAGS_DEBUG 
        CMAKE_CXX_FLAGS_RELEASE 
        CMAKE_CXX_FLAGS_DEBUG)
    foreach(variable ${variables})
        if(${variable} MATCHES "/MD")
            string(REPLACE "/MD" "/MT" ${variable} "${${variable}}")
        endif()
    endforeach()
endif()
#==================== 编译选项 ====================
# 通用警告选项
if (MSVC)
    add_compile_options(/utf-8)
    # 对所有 target 都启用异常展开
    add_compile_options(/EHsc)

    add_compile_definitions(_WIN32_WINNT=0x0A00 WINVER=0x0A00)

    # 2. 核心：防止 windows.h 引入不必要的宏（如 data, min, max）
    add_compile_definitions(WIN32_LEAN_AND_MEAN NOMINMAX)
        
    # 忽略烦人的 C4228 警告
    add_compile_options(/wd4228)
endif()
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC")
    # clang-cl 使用 MSVC 风格的链接
    add_compile_options(/utf-8)
    add_compile_options(/EHsc)  # <--- 启用异常展开
endif()



# ==================== 第三方库 ====================
# BUILD_SHARED_LIBS 是 CMake 的全局约定变量，但这里主库与第三方的策略不同：
# VMPUI 的类型由用户选择，第三方始终静态并封装进最终 VMPUI 产物。
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL ON CACHE BOOL "" FORCE)
set(SDL_INSTALL_CPACK OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
set(SDL_INSTALL_TESTS OFF CACHE BOOL "" FORCE)
set(ENTT_INSTALL ON CACHE BOOL "" FORCE)
set(EIGEN_BUILD_CMAKE_PACKAGE ON CACHE BOOL "" FORCE)
set(HB_BUILD_SUBSET OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/cmrc)
add_subdirectory(third_party/entt)
add_subdirectory(third_party/SDL)
add_subdirectory(third_party/stb)
add_subdirectory(third_party/yoga)
if(ENABLE_LTO AND CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND TARGET yogacore)
    set_property(TARGET yogacore PROPERTY INTERPROCEDURAL_OPTIMIZATION FALSE)
endif()
# Eigen 5.0.0 内部 `cmake_policy(SET CMP0146 OLD)` 触发 deprecation 警告；
# 该策略仅用于其禁用 FindCUDA 警告，对本项目无影响。临时抑制 deprecated 警告。
set(_pmk_saved_warn_deprecated ${CMAKE_WARN_DEPRECATED})
set(CMAKE_WARN_DEPRECATED OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/eigen-5.0.0)
set(CMAKE_WARN_DEPRECATED ${_pmk_saved_warn_deprecated} CACHE BOOL "" FORCE)
unset(_pmk_saved_warn_deprecated)
add_subdirectory(third_party/freetype)
add_subdirectory(third_party/harfbuzz)

if(ENABLE_BUILD_TESTS)
    add_subdirectory(third_party/googletest)
endif()

if(ENABLE_BUILD_BENCHMARKS)
    # Google Benchmark 默认会构建其自身依赖 gtest 的单元测试并执行 install，
    # 这里全部关闭，只引入 benchmark 库本身。
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    add_subdirectory(third_party/benchmark)
endif()

# 第三方配置完成后恢复用户对 VMPUI 的选择，保证 src/CMakeLists.txt 的
# STATIC/SHARED 分支真实可达，并让 cache/configure 输出与用户请求一致。
set(BUILD_SHARED_LIBS "${_VMPUI_BUILD_SHARED_LIBS}" CACHE BOOL
    "Build VMPUI as a shared library" FORCE)
if(BUILD_SHARED_LIBS)
    message(STATUS "[VMPUI] Library type: SHARED")
else()
    message(STATUS "[VMPUI] Library type: STATIC")
endif()

# ===================== SDK package =====================
set(VMPUI_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/VMPUI")
configure_package_config_file(
    "${CMAKE_SOURCE_DIR}/cmake/VMPUIConfig.cmake.in"
    "${CMAKE_CURRENT_BINARY_DIR}/VMPUIConfig.cmake"
    INSTALL_DESTINATION "${VMPUI_INSTALL_CMAKEDIR}"
)
write_basic_package_version_file(
    "${CMAKE_CURRENT_BINARY_DIR}/VMPUIConfigVersion.cmake"
    VERSION "${PROJECT_VERSION}"
    COMPATIBILITY SameMajorVersion
)
install(EXPORT VMPUITargets
    FILE VMPUITargets.cmake
    NAMESPACE VMPUI::
    DESTINATION "${VMPUI_INSTALL_CMAKEDIR}"
)
install(FILES
    "${CMAKE_CURRENT_BINARY_DIR}/VMPUIConfig.cmake"
    "${CMAKE_CURRENT_BINARY_DIR}/VMPUIConfigVersion.cmake"
    DESTINATION "${VMPUI_INSTALL_CMAKEDIR}"
)



# ==================== 静态分析工具 clang-tidy ====================
# 常规构建默认关闭 clang-tidy；它应作为显式的静态分析步骤启用，
# 否则在 Windows + clang-cl + 大型模板/生成源码下很容易出现 OOM。


# 查找 clang-tidy 可执行文件
find_program(CLANG_TIDY_EXE
    NAMES clang-tidy
)
if(ENABLE_CLANG_TIDY)
    if(CLANG_TIDY_EXE)
        message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE}")
        # clang-tidy 检查选项
        set(CLANG_TIDY_OPTIONS
           "--config-file=${CMAKE_SOURCE_DIR}/.clang-tidy"
            "-header-filter=^(?!.*third_party/).*"
            "--extra-arg=/EHsc"   # clang-cl (--driver-mode=cl) 下必须用 /EHsc 而非 -fexceptions 启用异常
        )

        # 设置 CMAKE_CXX_CLANG_TIDY
        set(CMAKE_CXX_CLANG_TIDY
            "${CLANG_TIDY_EXE};${CLANG_TIDY_OPTIONS}"
        )
    else()
        message(STATUS "ENABLE_CLANG_TIDY is ON but clang-tidy executable not found.")
        message(STATUS "Install clang-tidy or set CLANG_TIDY_EXE to its path to enable.")
    endif()
else()
    message(STATUS "clang-tidy integration disabled (ENABLE_CLANG_TIDY=OFF)")
endif()

# ==================== 架构边界 P0 软门禁 ====================
# 现有边界债已在脚本中 baseline；默认构建会检查是否新增：
# - systems/renderers/services 反向 include api/*
# - systems 使用 deprecated Registry::Xxx / Dispatcher::Xxx 静态入口
# - common 依赖 RuntimeFacade / Registry
find_package(Python3 COMPONENTS Interpreter QUIET)
if(Python3_Interpreter_FOUND)
    add_custom_target(ui_architecture_boundary_check ALL
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_SOURCE_DIR}/tools/check_architecture_boundaries.py"
            --root "${CMAKE_SOURCE_DIR}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "[UI Architecture] Checking P0 boundary debt does not grow"
        VERBATIM
    )

    # 公共头自包含门禁：include/ui/** 与 include/ui.hpp 不得引入任何第三方库头
    # （Eigen/entt/SDL3/spdlog/yoga/freetype/harfbuzz/cmrc）或 src/ 内部路径，
    # 保证自包含发行模式下消费者只需要 ui 头 + 链接 VMPUI 单库。
    add_custom_target(ui_public_headers_self_contained_check ALL
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_SOURCE_DIR}/tools/check_public_headers_self_contained.py"
            --root "${CMAKE_SOURCE_DIR}"
        WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
        COMMENT "[UI Distribution] Checking public headers have zero third-party dependencies"
        VERBATIM
    )
else()
    message(WARNING "Python3 interpreter not found; ui_architecture_boundary_check / ui_public_headers_self_contained_check targets are disabled.")
endif()

# ===================== UI 库 ====================
add_subdirectory(src)

# ===================== 示例程序 ====================
if(BUILD_EXAMPLES)
    add_subdirectory(example)
endif()

# ===================== 单元测试 =====================
if(ENABLE_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests/unittest)
    # P1-5 截图回归（offscreen 确定性渲染 + golden 比较），独立于默认 ui_tests
    add_subdirectory(tests/screenshot)
    # P1-5 交互录制骨架（EventWatch 录制 + PushEvent 回放）
    add_subdirectory(tests/interaction)
endif()

# ===================== 基准测试 =====================
if(ENABLE_BUILD_BENCHMARKS)
    add_subdirectory(tests/benchmark)
endif()
