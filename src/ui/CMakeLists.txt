#  ui 模块 CMake 配置文件
cmake_minimum_required(VERSION 3.20)
# 构建选项
# 共享库和静态库选项（默认构建静态库）
OPTION(BUILD_SHARED_LIBS "Build using shared libraries" OFF)
OPTION(BUILD_STATIC_LIBS "Build using static libraries" ON)
# 代码覆盖率选项
option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)
# 资源后端选项
option(UI_ENABLE_STD_EMBED_EXPERIMENTAL "Enable the experimental std::embed resource backend skeleton" ON)
# 调试选项
option(UI_ENABLE_ASAN "Enable AddressSanitizer for UI module" OFF)
# DXC 着色器编译选项
option(UI_ENABLE_SHADER_COMPILATION "Compile UI shaders with DXC during the build" OFF)


# 资源后端选项（CMRC 或 STD_EMBED）
set(UI_RESOURCE_BACKEND "STD_EMBED" CACHE STRING "UI resource backend (CMRC or STD_EMBED)")
# 资源后端选项字符串列表（用于 CMake GUI 和命令行提示）
set_property(CACHE UI_RESOURCE_BACKEND PROPERTY STRINGS CMRC STD_EMBED)

if(NOT UI_RESOURCE_BACKEND STREQUAL "CMRC" AND NOT UI_RESOURCE_BACKEND STREQUAL "STD_EMBED")
    message(FATAL_ERROR "Unsupported UI_RESOURCE_BACKEND='${UI_RESOURCE_BACKEND}'. Expected CMRC or STD_EMBED.")
endif()

if(UI_RESOURCE_BACKEND STREQUAL "STD_EMBED" AND NOT UI_ENABLE_STD_EMBED_EXPERIMENTAL)
    message(FATAL_ERROR "UI_RESOURCE_BACKEND=STD_EMBED requires UI_ENABLE_STD_EMBED_EXPERIMENTAL=ON.")
endif()
# 资源定义
set(UI_STD_EMBED_RESOURCE_TABLE_SOURCE "managers/StdEmbedResourceTable.cpp")
set(SHADER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/shader")
set(SHADER_OUTPUT_DIR "${CMAKE_CURRENT_SOURCE_DIR}/assets/shader")
set(UI_SHADER_OUTPUTS
    "${SHADER_OUTPUT_DIR}/vert.spv"
    "${SHADER_OUTPUT_DIR}/frag.spv"
    "${SHADER_OUTPUT_DIR}/vert.dxil"
    "${SHADER_OUTPUT_DIR}/frag.dxil"
)
set(UI_EMBEDDED_RESOURCES
    assets/shader/vert.spv
    assets/shader/frag.spv
    assets/shader/vert.dxil
    assets/shader/frag.dxil
    assets/fonts/NotoSansSC-VariableFont_wght.ttf
    assets/icons/MaterialSymbolsRounded[FILL,GRAD,opsz,wght].ttf
    assets/icons/MaterialSymbolsRounded[FILL,GRAD,opsz,wght].codepoints
)

if(UI_RESOURCE_BACKEND STREQUAL "STD_EMBED")
    set(UI_STD_EMBED_RESOURCE_TABLE_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/StdEmbedResourceTable.generated.cpp")
    set_source_files_properties(
        "${UI_STD_EMBED_RESOURCE_TABLE_SOURCE}"
        PROPERTIES
            GENERATED TRUE
            SKIP_LINTING ON
            SKIP_PRECOMPILE_HEADERS ON
    )
endif()

if(UI_ENABLE_SHADER_COMPILATION)
    list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")
    find_package(DXC REQUIRED)
else()
    foreach(UI_SHADER_OUTPUT IN LISTS UI_SHADER_OUTPUTS)
        if(NOT EXISTS "${UI_SHADER_OUTPUT}")
            message(FATAL_ERROR
                "UI shader compilation is disabled, but required shader artifact is missing: ${UI_SHADER_OUTPUT}. "
                "Enable UI_ENABLE_SHADER_COMPILATION=ON or provide the precompiled shader files."
            )
        endif()
    endforeach()
endif()


set(UI_SOURCES 
    core/Application.cpp
    core/EventLoop.cpp
    core/RuntimeFacade.cpp
    core/SystemManager.cpp
    systems/RenderSystem.cpp
    systems/TimerSystem.cpp
    managers/FontManager.cpp
    managers/IconManager.cpp
    managers/ResourceProvider.cpp
    ${UI_STD_EMBED_RESOURCE_TABLE_SOURCE}
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
    common/CustomizationPoints.hpp
    common/Policies.hpp
    common/Tags.hpp
    common/Events.hpp
    common/Types.hpp
    common/WindowSync.hpp
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
    managers/ResourceProvider.hpp
    managers/StdEmbedResourceTable.hpp
    
    # Core
    core/Application.hpp
    core/SystemManager.hpp
    core/EventLoop.hpp
    core/UiRuntime.hpp
    core/RuntimeFacade.hpp
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

target_precompile_headers(ui PRIVATE
    "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.hpp>"
)

# 头文件目录
target_include_directories(ui
    PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/stb
    PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
)

if(UI_ENABLE_SHADER_COMPILATION)
    # ===========================
    # Shader Compilation
    # ===========================
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

    add_custom_target(ui_shaders
        DEPENDS ${UI_SHADER_OUTPUTS}
        COMMENT "Building UI shaders"
    )
endif()

if(UI_RESOURCE_BACKEND STREQUAL "STD_EMBED")
    add_custom_command(
        OUTPUT "${UI_STD_EMBED_RESOURCE_TABLE_SOURCE}"
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/generated"
        COMMAND ${CMAKE_COMMAND}
            "-DOUTPUT_FILE=${UI_STD_EMBED_RESOURCE_TABLE_SOURCE}"
            "-DSOURCE_ROOT=${CMAKE_CURRENT_SOURCE_DIR}"
            "-DRESOURCE_FILES=${UI_EMBEDDED_RESOURCES}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/GenerateStdEmbedResourceTable.cmake"
        DEPENDS
            ${UI_SHADER_OUTPUTS}
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/NotoSansSC-VariableFont_wght.ttf"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/MaterialSymbolsRounded[FILL,GRAD,opsz,wght].ttf"
            "${CMAKE_CURRENT_SOURCE_DIR}/assets/icons/MaterialSymbolsRounded[FILL,GRAD,opsz,wght].codepoints"
            "${CMAKE_CURRENT_SOURCE_DIR}/GenerateStdEmbedResourceTable.cmake"
        COMMENT "Generating STD_EMBED shader resource table"
        VERBATIM
    )
endif()

if(UI_RESOURCE_BACKEND STREQUAL "CMRC")
    cmrc_add_resource_library(ui_fonts
        ALIAS fonts              # 在 C++ 中调用的别名
        NAMESPACE ui_fonts          # C++ 命名空间
        ${UI_EMBEDDED_RESOURCES}

    )

    if(UI_ENABLE_SHADER_COMPILATION)
        add_dependencies(ui_fonts ui_shaders)
    endif()

    # 禁用 clang-tidy 对自动生成的资源库目标（防止 OOM 崩溃）
    # cmrc 将大型二进制文件（字体/着色器）嵌入为巨型 C++ 字节数组，
    # clang-tidy 分析这些文件会耗尽内存。
    # 同时关闭 IPO/LTO，避免 clang-cl 在 RelWithDebInfo 下编译超大嵌入资源时耗尽内存。
    set_target_properties(ui_fonts PROPERTIES
        CXX_CLANG_TIDY ""
        INTERPROCEDURAL_OPTIMIZATION FALSE
    )
endif()

if(UI_RESOURCE_BACKEND STREQUAL "CMRC")
    target_compile_definitions(ui PRIVATE
        UI_RESOURCE_BACKEND_CMRC=1
        UI_RESOURCE_BACKEND_NAME="CMRC"
    )
elseif(UI_RESOURCE_BACKEND STREQUAL "STD_EMBED")
    target_compile_definitions(ui PRIVATE
        UI_RESOURCE_BACKEND_STD_EMBED=1
        UI_RESOURCE_BACKEND_NAME="STD_EMBED"
    )
endif()

# 依赖项
target_link_libraries(ui PRIVATE
    SDL3::SDL3
    EnTT::EnTT
    asio::asio
    yogacore
    spdlog::spdlog_header_only
    freetype
    harfbuzz
)

if(UI_RESOURCE_BACKEND STREQUAL "CMRC")
    target_link_libraries(ui PRIVATE ui_fonts)
endif()

target_link_libraries(ui PUBLIC
    Eigen3::Eigen
)

target_compile_definitions(ui PRIVATE UI_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

# Windows 平台链接 DWM + comctl32（用于无边框窗口透明合成 + 子类化）
if(WIN32)
    target_link_libraries(ui PRIVATE dwmapi comctl32)
endif()
target_compile_features(ui PUBLIC cxx_std_23)

target_compile_options(ui PRIVATE
     
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


