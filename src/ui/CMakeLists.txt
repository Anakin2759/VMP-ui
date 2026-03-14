#  ui 模块 CMake 配置文件
cmake_minimum_required(VERSION 3.20)
OPTION(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
OPTION(BUILD_STATIC_LIBS "Build using static libraries" ON)
option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)
# Find DXC compiler for shader compilation
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
find_package(DXC REQUIRED)








set(UI_SOURCES 
    core/Application.cpp
    core/EventLoop.cpp
    core/SystemManager.cpp
    systems/RenderSystem.cpp
    systems/TimerSystem.cpp
    managers/FontManager.cpp
    managers/IconManager.cpp
    api/Size.cpp
    api/Visibility.cpp
    api/Layout.cpp
    api/Text.cpp
    api/Animation.cpp
    api/Hierarchy.cpp
    api/Icon.cpp
    api/Utils.cpp
    api/Factory.cpp
    api/Controls.cpp
    core/PlatformWindow.cpp
)
set(UI_HEADERS
    # Components
    common/Components.hpp
    common/Policies.hpp
    common/Tags.hpp
    common/Events.hpp
    common/Types.hpp
    common/RenderTypes.hpp
    common/UiErrors.hpp
    
    # Systems
    systems/InteractionSystem.hpp
    systems/TweenSystem.hpp
    systems/LayoutSystem.hpp
    systems/RenderSystem.hpp
    systems/StateSystem.hpp
    systems/HitTestSystem.hpp
    systems/ActionSystem.hpp
    systems/TimerSystem.hpp
    
    # Managers
    managers/DeviceManager.hpp
    managers/FontManager.hpp
    managers/PipelineCache.hpp
    managers/TextTextureCache.hpp
    managers/IconManager.hpp
    managers/BatchManager.hpp
    managers/CommandBuffer.hpp
    
    # Core
    core/Application.hpp
    core/SystemManager.hpp
    core/EventLoop.hpp
    core/TaskChain.hpp
    core/TextUtils.hpp
    interface/IRenderer.hpp
    interface/IBackendRenderer.hpp
    core/RenderContext.hpp
    core/PlatformWindow.hpp
    renderers/ShapeRenderer.hpp
    renderers/FallbackBackendRenderer.hpp
    renderers/SliderRenderer.hpp
    renderers/ProgressBarRenderer.hpp
    renderers/TextRenderer.hpp
    renderers/IconRenderer.hpp
    renderers/ScrollBarRenderer.hpp


    singleton/Registry.hpp
    singleton/SingletonBase.hpp
    singleton/Logger.hpp
    singleton/Dispatcher.hpp
    
    traits/ComponentsTraits.hpp
    traits/PoliciesTraits.hpp

    # Interface
    interface/Isystem.hpp
    
    # API
    api/Utils.hpp
    api/Layout.hpp
    api/Controls.hpp
    
    ui/ui.hpp
)
# UI 模块库定义
add_library(ui STATIC ${UI_SOURCES} ${UI_HEADERS})

# 头文件目录
target_include_directories(ui
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/stb
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
)

# ===========================
# Shader Compilation
# ===========================
set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/shader")
set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/shader")

# Compile vertex shader for both Vulkan and D3D12
add_custom_command(
    OUTPUT 
        "${SHADER_OUTPUT_DIR}/vert.spv"
        "${SHADER_OUTPUT_DIR}/vert.dxil"
    COMMAND ${DXC_EXECUTABLE}
        -spirv -T vs_6_0 -E main_vs -fspv-target-env=vulkan1.3
        -Fo "${SHADER_OUTPUT_DIR}/vert.spv"
        "${SHADER_SOURCE_DIR}/vert.hlsl"
    COMMAND ${DXC_EXECUTABLE}
        -T vs_6_6 -E main_vs
        -Fo "${SHADER_OUTPUT_DIR}/vert.dxil"
        "${SHADER_SOURCE_DIR}/vert.hlsl"
    DEPENDS "${SHADER_SOURCE_DIR}/vert.hlsl" "${SHADER_SOURCE_DIR}/common.hlsl"
    WORKING_DIRECTORY "${SHADER_SOURCE_DIR}"
    COMMENT "Compiling vertex shader (SPIR-V + DXIL)"
    VERBATIM
)

# Compile fragment shader for both Vulkan and D3D12
add_custom_command(
    OUTPUT 
        "${SHADER_OUTPUT_DIR}/frag.spv"
        "${SHADER_OUTPUT_DIR}/frag.dxil"
    COMMAND ${DXC_EXECUTABLE}
        -spirv -T ps_6_0 -E main_ps -fspv-target-env=vulkan1.3
        -Fo "${SHADER_OUTPUT_DIR}/frag.spv"
        "${SHADER_SOURCE_DIR}/frag.hlsl"
    COMMAND ${DXC_EXECUTABLE}
        -T ps_6_6 -E main_ps
        -Fo "${SHADER_OUTPUT_DIR}/frag.dxil"
        "${SHADER_SOURCE_DIR}/frag.hlsl"
    DEPENDS "${SHADER_SOURCE_DIR}/frag.hlsl" "${SHADER_SOURCE_DIR}/common.hlsl"
    WORKING_DIRECTORY "${SHADER_SOURCE_DIR}"
    COMMENT "Compiling fragment shader (SPIR-V + DXIL)"
    VERBATIM
)

# Create target for shader compilation
add_custom_target(ui_shaders
    DEPENDS
        "${SHADER_OUTPUT_DIR}/vert.spv"
        "${SHADER_OUTPUT_DIR}/vert.dxil"
        "${SHADER_OUTPUT_DIR}/frag.spv"
        "${SHADER_OUTPUT_DIR}/frag.dxil"
    COMMENT "Building UI shaders"
)

cmrc_add_resource_library(ui_fonts
    ALIAS fonts              # 在 C++ 中调用的别名
    NAMESPACE ui_fonts          # C++ 命名空间
    assets/shader/vert.spv
    assets/shader/frag.spv
    assets/shader/vert.dxil
    assets/shader/frag.dxil

)

# Ensure shaders are compiled before embedding into resources
add_dependencies(ui_fonts ui_shaders)

# 禁用 clang-tidy 对自动生成的资源库目标（防止 OOM 崩溃）
# cmrc 将大型二进制文件（字体/着色器）嵌入为巨型 C++ 字节数组，
# clang-tidy 分析这些文件会耗尽内存。
# 同时关闭 IPO/LTO，避免 clang-cl 在 RelWithDebInfo 下编译超大嵌入资源时耗尽内存。
set_target_properties(ui_fonts PROPERTIES
    CXX_CLANG_TIDY ""
    INTERPROCEDURAL_OPTIMIZATION FALSE
)

# 依赖项
target_link_libraries(ui PRIVATE
    SDL3::SDL3
    EnTT::EnTT
    asio::asio
    yogacore
    ui_fonts
    spdlog::spdlog_header_only
    freetype
    harfbuzz
)

target_link_libraries(ui PUBLIC
    Eigen3::Eigen
)

target_compile_definitions(ui PRIVATE UI_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

# Windows 平台链接 DWM + comctl32（用于无边框窗口透明合成 + 子类化）
if(WIN32)
    target_link_libraries(ui PRIVATE dwmapi comctl32)
endif()
target_compile_features(ui PUBLIC cxx_std_23)

target_compile_options(ui PUBLIC
     
    # GCC
    $<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Debug>>:-Wall -Wextra -Wpedantic -O0 -g >
    $<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Release>>:-Wall -O3 -DNDEBUG >

    # Clang (non-MSVC frontend)
    $<$<AND:$<CXX_COMPILER_ID:Clang>,$<NOT:$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>,$<CONFIG:Debug>>:-Wall -Wextra -Wpedantic -O0 -g >
    $<$<AND:$<CXX_COMPILER_ID:Clang>,$<NOT:$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>,$<CONFIG:Release>>:-Wall -O3 -DNDEBUG >

    # MSVC
    $<$<CXX_COMPILER_ID:MSVC>:/utf-8  /EHsc>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/W4 /Od /Zi>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2 /DNDEBUG>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:MinSizeRel>>:/O2 /DNDEBUG>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:RelWithDebInfo>>:/O2 /DNDEBUG /W4 /Zi>

    # Clang-cl (MSVC frontend)
    $<$<AND:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>,$<CONFIG:Debug>>:/EHsc /Zi /W4>
    $<$<AND:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>,$<CONFIG:Release>>:/EHsc /O2 /DNDEBUG >

)


if(ENABLE_COVERAGE AND MSVC)
    # MSVC 需要生成完整的调试信息以便 OpenCppCoverage 关联源码
    target_compile_options(ui PRIVATE /Zi /Od)
    target_link_options(ui PRIVATE /DEBUG)
endif()


