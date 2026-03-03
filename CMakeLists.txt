cmake_minimum_required(VERSION 4.0)
project(PestMan LANGUAGES CXX)

# 确保 C++ 标准对所有目标生效（包括第三方库）
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CheckIPOSupported)
if(MSVC)
    add_compile_definitions(
        ASIO_DISABLE_STD_EXECUTION
        ASIO_STANDALONE
    )
    
endif()

option(ENABLE_BUILD_TESTS "Enable building of unit tests" OFF)

#==================== IPO / LTO 设置 ====================
check_ipo_supported(RESULT ipo_supported OUTPUT ipo_error)

if(ipo_supported)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO TRUE)
    message(STATUS "IPO / LTO enabled for Release configs")
else()
    message(WARNING "IPO / LTO not supported: ${ipo_error}")
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
add_subdirectory(third_party/mimalloc)
add_subdirectory(third_party/entt)
add_subdirectory(third_party/json)
add_subdirectory(third_party/spdlog)



add_subdirectory(third_party/SDL)

add_subdirectory(third_party/stb)

add_subdirectory(third_party/abseil-cpp)
add_subdirectory(third_party/kcp)
add_subdirectory(third_party/googletest)

add_subdirectory(third_party/yoga)
add_subdirectory(third_party/eigen-5.0.0)
add_subdirectory(third_party/freetype)
add_subdirectory(third_party/harfbuzz)



# ==================== 静态分析工具 clang-tidy ====================
# 允许用户通过选项启用或禁用 clang-tidy
option(ENABLE_CLANG_TIDY "Enable clang-tidy static analysis" ON)

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

# 自动搜索 third_party 下的所有一级子目录并加入 include
file(GLOB children RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}/third_party ${CMAKE_CURRENT_SOURCE_DIR}/third_party/*)
foreach(child ${children})
    if(IS_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/third_party/${child})
        include_directories(${CMAKE_CURRENT_SOURCE_DIR}/third_party/${child})
    endif()
endforeach()
# ===================== 项目主程序 ====================
add_subdirectory(src/utils)
add_subdirectory(src/shared)
add_subdirectory(src/ui)
add_subdirectory(src/net)
 add_subdirectory(src/client)
 add_subdirectory(src/server)
# ===================== 单元测试 =====================
if(ENABLE_BUILD_TESTS)
    add_subdirectory(tests/unittest)
endif()
