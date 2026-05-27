cmake_minimum_required(VERSION 3.31)
project(VMP-ui LANGUAGES CXX)


set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CheckIPOSupported)
if(MSVC)
    add_compile_definitions(
        ASIO_DISABLE_STD_EXECUTION
        ASIO_STANDALONE
    )
    
endif()

# Linux + Clang (非 MSVC 前端): 强制使用 libc++，避免与 libstdc++ ABI 混用。
# macOS Clang 默认已是 libc++，此处为 no-op 但无害。
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang"
        AND NOT CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"
        AND NOT WIN32)
    add_compile_options(-stdlib=libc++)
    add_link_options(-stdlib=libc++)
    message(STATUS "[Clang] Using libc++ (LLVM C++ standard library)")
endif()

option(ENABLE_BUILD_TESTS "Enable building of unit tests" OFF)
option(BUILD_EXAMPLES "Enable building examples" ON)
option(ENABLE_LTO "Enable IPO/LTO for Release and RelWithDebInfo builds" ON)
option(ENABLE_CLANG_TIDY "Enable clang-tidy static analysis" OFF)

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
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /clang:-fms-compatibility-version=19.33")
endif()



# ==================== 第三方库 ====================
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
add_subdirectory(third_party/asio)
add_subdirectory(third_party/cmrc)
add_subdirectory(third_party/entt)
add_subdirectory(third_party/spdlog)
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
endif()
