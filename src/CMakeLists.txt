#  ui 模块 CMake 配置文件
cmake_minimum_required(VERSION 3.20)
# BUILD_SHARED_LIBS 由顶层工程声明；第三方依赖配置完成后会恢复用户选择。
# 代码覆盖率选项
option(ENABLE_COVERAGE "Enable code coverage instrumentation" OFF)
# 调试选项
option(UI_ENABLE_ASAN "Enable AddressSanitizer for UI module" OFF)
# CPU 渲染选项：跳过所有 GPU 后端，直接使用 SDL_Renderer software 后端（用于无 GPU 环境或调试）
option(UI_FORCE_CPU_RENDER "Force CPU-only software rendering (skip all GPU backends)" OFF)
# 平台显示缩放：默认跟随系统 DPI / Wayland scale / X11 scale
option(UI_ENABLE_PLATFORM_SCALING "Enable platform display scaling by default" ON)
# Linux 窗口系统后端：AUTO 自动探测、X11、WAYLAND（仅 Linux 生效）
set(UI_LINUX_WINDOW_SYSTEM "AUTO" CACHE STRING
    "Linux window system backend: AUTO (detect X11 first, fallback Wayland), X11, WAYLAND")
set_property(CACHE UI_LINUX_WINDOW_SYSTEM PROPERTY STRINGS AUTO X11 WAYLAND)
# DXC 着色器编译选项
# AUTO：构建时自动检测 DXC；若找到则编译，否则回退到预编译二进制
# ON  ：强制要求 DXC，找不到则报错
# OFF ：禁用，直接使用预编译二进制（构建更快，但 HLSL 改动后不自动更新）
set(UI_ENABLE_SHADER_COMPILATION "AUTO" CACHE STRING
    "Shader compilation mode: AUTO (auto-detect DXC), ON (require DXC), OFF (precompiled only)")
set_property(CACHE UI_ENABLE_SHADER_COMPILATION PROPERTY STRINGS AUTO ON OFF)


# 资源后端选项
# STD_EMBED：将资源表生成为 .cpp 直接编译进 ui（无 cmrc 依赖）——自包含发行默认后端。
# CMRC    ：基于 cmrc 的嵌入资源库（开发/测试用；发行 install 会额外暴露 cmrc 目标）。
set(UI_RESOURCE_BACKEND "STD_EMBED" CACHE STRING "UI resource backend (CMRC or STD_EMBED)")
# 资源后端选项字符串列表（用于 CMake GUI 和命令行提示）
set_property(CACHE UI_RESOURCE_BACKEND PROPERTY STRINGS CMRC STD_EMBED)

if(NOT UI_RESOURCE_BACKEND STREQUAL "CMRC" AND NOT UI_RESOURCE_BACKEND STREQUAL "STD_EMBED")
    message(FATAL_ERROR "Unsupported UI_RESOURCE_BACKEND='${UI_RESOURCE_BACKEND}'. Expected CMRC or STD_EMBED.")
endif()

# STD_EMBED 已转正为自包含发行默认后端；不再要求 experimental 开关。
# 旧变量 UI_ENABLE_STD_EMBED_EXPERIMENTAL 已废弃，仅对历史命令行参数做兼容忽略。
if(UI_RESOURCE_BACKEND STREQUAL "STD_EMBED" AND NOT UI_ENABLE_STD_EMBED_EXPERIMENTAL AND DEFINED UI_ENABLE_STD_EMBED_EXPERIMENTAL)
    message(STATUS "UI_ENABLE_STD_EMBED_EXPERIMENTAL is deprecated and ignored.")
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

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# 必须在配置 Object 库前探测平台能力，避免 UI_LINUX_X11 宏依赖
# 第三方项目偶然设置的 X11_FOUND。
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
   (UI_LINUX_WINDOW_SYSTEM STREQUAL "AUTO" OR UI_LINUX_WINDOW_SYSTEM STREQUAL "X11"))
    find_package(X11 QUIET)
    if(NOT X11_FOUND AND UI_LINUX_WINDOW_SYSTEM STREQUAL "X11")
        message(FATAL_ERROR "[UI] UI_LINUX_WINDOW_SYSTEM=X11 but libX11 not found.")
    endif()
endif()

# 解析 AUTO 模式：尝试找到 DXC，根据实际能力决定编译内容
# DXC 版本分类：
#   Vulkan SDK DXC（LunarG）  → DXC_SUPPORTS_SPIRV=TRUE  + DXC_SUPPORTS_DXIL=TRUE
#   Windows Kits DXC（新）    → DXC_SUPPORTS_SPIRV=FALSE + DXC_SUPPORTS_DXIL=TRUE
#   Windows Kits DXC（旧，如 19041）→ 两者皆 FALSE，完全回退预编译
if(UI_ENABLE_SHADER_COMPILATION STREQUAL "AUTO")
    find_package(DXC QUIET)
    if(DXC_FOUND AND (DXC_SUPPORTS_SPIRV OR DXC_SUPPORTS_DXIL))
        set(_UI_DO_SHADER_COMPILE TRUE)
        if(DXC_SUPPORTS_SPIRV AND DXC_SUPPORTS_DXIL)
            message(STATUS "[UI Shaders] DXC (Vulkan SDK) at ${DXC_EXECUTABLE} — SPIR-V + DXIL.")
        elseif(DXC_SUPPORTS_DXIL)
            message(STATUS "[UI Shaders] DXC at ${DXC_EXECUTABLE} — DXIL only; SPIR-V uses precompiled binaries.")
        else()
            message(STATUS "[UI Shaders] DXC at ${DXC_EXECUTABLE} — SPIR-V only.")
        endif()
    else()
        set(_UI_DO_SHADER_COMPILE FALSE)
        if(DXC_FOUND)
            message(STATUS "[UI Shaders] DXC found but too old to compile shaders (sm6.6 unsupported). Using precompiled binaries.")
        else()
            message(STATUS "[UI Shaders] DXC not found. Using precompiled binaries.")
        endif()
    endif()
elseif(UI_ENABLE_SHADER_COMPILATION STREQUAL "ON" OR UI_ENABLE_SHADER_COMPILATION)
    find_package(DXC REQUIRED)
    if(NOT DXC_SUPPORTS_SPIRV AND NOT DXC_SUPPORTS_DXIL)
        message(FATAL_ERROR "[UI Shaders] DXC forced ON but cannot compile SPIR-V or DXIL sm6.6. "
            "Install Vulkan SDK or update Windows Kits.")
    endif()
    set(_UI_DO_SHADER_COMPILE TRUE)
    if(DXC_SUPPORTS_SPIRV AND DXC_SUPPORTS_DXIL)
        message(STATUS "[UI Shaders] DXC forced ON (${DXC_EXECUTABLE}) — SPIR-V + DXIL.")
    elseif(DXC_SUPPORTS_DXIL)
        message(STATUS "[UI Shaders] DXC forced ON (${DXC_EXECUTABLE}) — DXIL only.")
    else()
        message(STATUS "[UI Shaders] DXC forced ON (${DXC_EXECUTABLE}) — SPIR-V only.")
    endif()
else()
    set(_UI_DO_SHADER_COMPILE FALSE)
endif()

if(NOT _UI_DO_SHADER_COMPILE)
    # 验证所有预编译产物存在；Linux 不使用 D3D12，DXIL 可缺失
    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        set(_UI_REQUIRED_SHADER_OUTPUTS
            "${SHADER_OUTPUT_DIR}/vert.spv"
            "${SHADER_OUTPUT_DIR}/frag.spv")
    else()
        set(_UI_REQUIRED_SHADER_OUTPUTS ${UI_SHADER_OUTPUTS})
    endif()
    foreach(_shader IN LISTS _UI_REQUIRED_SHADER_OUTPUTS)
        if(NOT EXISTS "${_shader}")
            message(FATAL_ERROR
                "[UI Shaders] Precompiled shader missing: ${_shader}.\n"
                "  Options:\n"
                "    1. Run src/assets/shader/compile.sh (Linux) or compile.bat (Windows)\n"
                "    2. Set UI_ENABLE_SHADER_COMPILATION=ON (requires DXC / Vulkan SDK)\n"
                "    3. Set UI_ENABLE_SHADER_COMPILATION=AUTO (auto-detects DXC)")
        endif()
    endforeach()
endif()


# ==================== 按模块拆分的 Object 库源列表（编译优化） ====================
# 将 ui 拆分为多个 OBJECT 库，每个库拥有独立的编译配置并共享全局内存感知 JOB_POOL：
#   - 全局并行度由顶层按可用内存计算，防止模块独立池叠加绕过内存预算；
#   - 增量构建边界清晰：仅依赖某模块的改动不会触发整库重新编译语义混乱；
#   - 聚合静态库 ui 由这些 object 库拼装，对外行为（VMPUI::ui / 合并 / install）完全不变。

set(UI_OBJECT_CORE_SOURCES
    core/Application.cpp
    core/EventLoop.cpp
    core/SdlEventWakeup.cpp
    core/SystemManager.cpp
    core/PlatformWindow.cpp
    services/TextEditingService.cpp
    common/ErrorCodes.cpp
    utils/Logger.cpp
)

set(UI_OBJECT_SYSTEMS_SOURCES
    systems/LayoutSystem.cpp
    systems/HitTestSystem.cpp
    systems/StateSystem.cpp
    systems/ThemeSystem.cpp
    systems/TimerSystem.cpp
    systems/ShortcutSystem.cpp
    systems/FocusNavigationSystem.cpp
    systems/OverlaySystem.cpp
)

set(UI_OBJECT_RENDER_SOURCES
    systems/render/RenderBackend.cpp
    systems/render/RenderResources.cpp
    systems/render/RenderFrame.cpp
)

set(UI_OBJECT_MANAGERS_SOURCES
    managers/FontManager.cpp
    managers/IconManager.cpp
    managers/ResourceProvider.cpp
    managers/TextTextureCache.cpp
    managers/ImageManager.cpp
)

set(UI_OBJECT_RENDERERS_SOURCES
    renderers/TableRenderer.cpp
)

set(UI_OBJECT_API_SOURCES
    api/Size.cpp
    api/Scale.cpp
    api/Visibility.cpp
    api/Layout.cpp
    api/Text.cpp
    api/Animation.cpp
    api/Hierarchy.cpp
    api/Icon.cpp
    api/Utils.cpp
    api/Factory.cpp
    api/Controls.cpp
    api/Event.cpp
    api/Query.cpp
    api/Canvas.cpp
    api/Image.cpp
    api/Table.cpp
    api/Theme.cpp
    api/Shortcut.cpp
    api/Log.cpp
    api/Timer.cpp
    helper/HelperAnimation.cpp
)

set(UI_OBJECT_RESOURCES_SOURCES
    ${UI_STD_EMBED_RESOURCE_TABLE_SOURCE}
)

# 汇总（供诊断/工具使用；聚合库通过 $<TARGET_OBJECTS> 引用各 object 库）
set(UI_SOURCES
    ${UI_OBJECT_CORE_SOURCES}
    ${UI_OBJECT_SYSTEMS_SOURCES}
    ${UI_OBJECT_RENDER_SOURCES}
    ${UI_OBJECT_MANAGERS_SOURCES}
    ${UI_OBJECT_RENDERERS_SOURCES}
    ${UI_OBJECT_API_SOURCES}
    ${UI_OBJECT_RESOURCES_SOURCES}
)
set(UI_HEADERS
    # Public foundations
    ${CMAKE_SOURCE_DIR}/include/ui/ErrorCodes.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/Callback.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/Color.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/Geometry.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/Result.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/Policies.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/TweenOptions.hpp

    # Components
    common/CustomizationPoints.hpp
    common/Policies.hpp
    common/Tags.hpp
    common/Events.hpp
    common/Animation.hpp
    common/Scale.hpp
    common/Shortcut.hpp
    common/Table.hpp
    common/Theme.hpp
    common/Types.hpp
    common/RenderTypes.hpp
    common/Result.hpp
    common/ErrorCodes.hpp
    common/components/Overlay.hpp

    # Internal helpers
    helper/Helper.hpp
    
    # Systems
    systems/InteractionSystem.hpp
    systems/TweenSystem.hpp
    systems/LayoutSystem.hpp
    systems/RenderSystem.hpp
    systems/StateSystem.hpp
    systems/HitTestSystem.hpp
    systems/ActionSystem.hpp
    systems/TimerSystem.hpp
    systems/ShortcutSystem.hpp
    systems/ThemeSystem.hpp
    systems/FocusNavigationSystem.hpp
    systems/OverlaySystem.hpp
    
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
    ${CMAKE_SOURCE_DIR}/include/ui/Application.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/WindowsMacroShield.hpp
    core/Application.hpp
    core/SystemManager.hpp
    core/EventLoop.hpp
    core/SdlEventWakeup.hpp
    core/UiRuntime.hpp
    core/WindowEntityLookup.hpp
    core/WindowSync.hpp
    core/TaskChain.hpp
    core/TextUtils.hpp
    services/TextEditingService.hpp
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
    renderers/ImageRenderer.hpp
    renderers/CanvasRenderer.hpp
    renderers/TableRenderer.hpp
    renderers/CheckBoxRenderer.hpp
    renderers/DropDownRenderer.hpp
    renderers/SwitchRenderer.hpp
    renderers/RadioButtonRenderer.hpp


    utils/Logger.hpp

    traits/ComponentsTraits.hpp
    traits/PoliciesTraits.hpp

    # Interface
    interface/ISystem.hpp
    
    # API
    ${CMAKE_SOURCE_DIR}/include/ui/api/Factory.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/api/Animation.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/api/Chains.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/api/Hierarchy.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/api/Icon.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/api/Image.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/api/Layout.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/api/Log.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/api/Query.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/api/Scale.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/api/Size.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/api/Utils.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/api/Controls.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/api/Event.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/api/Canvas.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/api/Table.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/api/Theme.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/api/Text.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/api/Visibility.hpp
    ${CMAKE_SOURCE_DIR}/include/ui/api/Shortcut.hpp
    managers/ImageManager.hpp
)
# ---------------------------------------------------------------------------
# 注：聚合库 ui 与各 Object 库的定义已移至 CMRC 资源库（ui_fonts）之后，
#     以保证 configure 阶段引用到全部第三方/资源 target（见下文 Object 库拆分段）。
# ---------------------------------------------------------------------------

if(_UI_DO_SHADER_COMPILE)
    # ===========================
    # Shader Compilation
    # CMake 的 add_custom_command 天然基于 OUTPUT 依赖做增量检查：
    # 只有 DEPENDS 中的 .hlsl 比 OUTPUT 中的产物新时才重新编译。
    # ===========================

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # Linux：DXC 通常不支持 DXIL，只编译 SPIR-V
        if(NOT DXC_SUPPORTS_SPIRV)
            message(FATAL_ERROR "[UI Shaders] Linux requires DXC with SPIR-V support.\n"
                "  Install: sudo apt install directx-shader-compiler\n"
                "  Or: https://vulkan.lunarg.com/sdk/home#linux")
        endif()
        add_custom_command(
            OUTPUT  "${SHADER_OUTPUT_DIR}/vert.spv"
            COMMAND ${DXC_EXECUTABLE}
                -spirv -T vs_6_0 -E main_vs -fspv-target-env=vulkan1.3
                -Fo "${SHADER_OUTPUT_DIR}/vert.spv"
                "${SHADER_SOURCE_DIR}/vert.hlsl"
            DEPENDS "${SHADER_SOURCE_DIR}/vert.hlsl" "${SHADER_SOURCE_DIR}/common.hlsl"
            WORKING_DIRECTORY "${SHADER_SOURCE_DIR}"
            COMMENT "[UI Shaders] Compiling vertex shader (SPIR-V, Linux)"
            VERBATIM
        )
        add_custom_command(
            OUTPUT  "${SHADER_OUTPUT_DIR}/frag.spv"
            COMMAND ${DXC_EXECUTABLE}
                -spirv -T ps_6_0 -E main_ps -fspv-target-env=vulkan1.3
                -Fo "${SHADER_OUTPUT_DIR}/frag.spv"
                "${SHADER_SOURCE_DIR}/frag.hlsl"
            DEPENDS "${SHADER_SOURCE_DIR}/frag.hlsl" "${SHADER_SOURCE_DIR}/common.hlsl"
            WORKING_DIRECTORY "${SHADER_SOURCE_DIR}"
            COMMENT "[UI Shaders] Compiling fragment shader (SPIR-V, Linux)"
            VERBATIM
        )
        add_custom_target(ui_shaders
            DEPENDS "${SHADER_OUTPUT_DIR}/vert.spv" "${SHADER_OUTPUT_DIR}/frag.spv"
            COMMENT "[UI Shaders] Building UI shaders (SPIR-V only)"
        )

    elseif(DXC_SUPPORTS_SPIRV AND DXC_SUPPORTS_DXIL)
        # Windows / macOS + Vulkan SDK DXC：同时输出 SPIR-V + DXIL
        add_custom_command(
            OUTPUT  "${SHADER_OUTPUT_DIR}/vert.spv" "${SHADER_OUTPUT_DIR}/vert.dxil"
            COMMAND ${DXC_EXECUTABLE}
                -spirv -T vs_6_0 -E main_vs -fspv-target-env=vulkan1.3
                -Fo "${SHADER_OUTPUT_DIR}/vert.spv" "${SHADER_SOURCE_DIR}/vert.hlsl"
            COMMAND ${DXC_EXECUTABLE}
                -T vs_6_0 -E main_vs
                -Fo "${SHADER_OUTPUT_DIR}/vert.dxil" "${SHADER_SOURCE_DIR}/vert.hlsl"
            DEPENDS "${SHADER_SOURCE_DIR}/vert.hlsl" "${SHADER_SOURCE_DIR}/common.hlsl"
            WORKING_DIRECTORY "${SHADER_SOURCE_DIR}"
            COMMENT "[UI Shaders] Compiling vertex shader (SPIR-V + DXIL)"
            VERBATIM
        )
        add_custom_command(
            OUTPUT  "${SHADER_OUTPUT_DIR}/frag.spv" "${SHADER_OUTPUT_DIR}/frag.dxil"
            COMMAND ${DXC_EXECUTABLE}
                -spirv -T ps_6_0 -E main_ps -fspv-target-env=vulkan1.3
                -Fo "${SHADER_OUTPUT_DIR}/frag.spv" "${SHADER_SOURCE_DIR}/frag.hlsl"
            COMMAND ${DXC_EXECUTABLE}
                -T ps_6_0 -E main_ps
                -Fo "${SHADER_OUTPUT_DIR}/frag.dxil" "${SHADER_SOURCE_DIR}/frag.hlsl"
            DEPENDS "${SHADER_SOURCE_DIR}/frag.hlsl" "${SHADER_SOURCE_DIR}/common.hlsl"
            WORKING_DIRECTORY "${SHADER_SOURCE_DIR}"
            COMMENT "[UI Shaders] Compiling fragment shader (SPIR-V + DXIL)"
            VERBATIM
        )
        add_custom_target(ui_shaders
            DEPENDS ${UI_SHADER_OUTPUTS}
            COMMENT "[UI Shaders] Building UI shaders (SPIR-V + DXIL)"
        )

    else()
        # Windows + Windows SDK DXC：仅支持 DXIL，SPIR-V 必须使用预编译二进制
        foreach(_spv "${SHADER_OUTPUT_DIR}/vert.spv" "${SHADER_OUTPUT_DIR}/frag.spv")
            if(NOT EXISTS "${_spv}")
                message(FATAL_ERROR
                    "[UI Shaders] Windows Kits DXC does not support SPIR-V, "
                    "but precompiled SPIR-V binary is missing: ${_spv}.\n"
                    "  Options:\n"
                    "    1. Install Vulkan SDK to compile SPIR-V (recommended)\n"
                    "    2. Run compile.bat from a machine with Vulkan SDK to generate .spv files")
            endif()
        endforeach()
        add_custom_command(
            OUTPUT  "${SHADER_OUTPUT_DIR}/vert.dxil"
            COMMAND ${DXC_EXECUTABLE}
                -T vs_6_0 -E main_vs
                -Fo "${SHADER_OUTPUT_DIR}/vert.dxil" "${SHADER_SOURCE_DIR}/vert.hlsl"
            DEPENDS "${SHADER_SOURCE_DIR}/vert.hlsl" "${SHADER_SOURCE_DIR}/common.hlsl"
            WORKING_DIRECTORY "${SHADER_SOURCE_DIR}"
            COMMENT "[UI Shaders] Compiling vertex shader (DXIL only, sm6.0)"
            VERBATIM
        )
        add_custom_command(
            OUTPUT  "${SHADER_OUTPUT_DIR}/frag.dxil"
            COMMAND ${DXC_EXECUTABLE}
                -T ps_6_0 -E main_ps
                -Fo "${SHADER_OUTPUT_DIR}/frag.dxil" "${SHADER_SOURCE_DIR}/frag.hlsl"
            DEPENDS "${SHADER_SOURCE_DIR}/frag.hlsl" "${SHADER_SOURCE_DIR}/common.hlsl"
            WORKING_DIRECTORY "${SHADER_SOURCE_DIR}"
            COMMENT "[UI Shaders] Compiling fragment shader (DXIL only, sm6.0)"
            VERBATIM
        )
        add_custom_target(ui_shaders
            DEPENDS
                "${SHADER_OUTPUT_DIR}/vert.dxil"
                "${SHADER_OUTPUT_DIR}/frag.dxil"
            COMMENT "[UI Shaders] Building UI shaders (DXIL only, SPIR-V from precompiled)"
        )
    endif()
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

    if(_UI_DO_SHADER_COMPILE)
        add_dependencies(ui_fonts ui_shaders)
    endif()

    # 禁用 clang-tidy 对自动生成的资源库目标（防止 OOM 崩溃）
    # cmrc 将大型二进制文件（字体/着色器）嵌入为巨型 C++ 字节数组，
    # clang-tidy 分析这些文件会耗尽内存。
    # 同时关闭 IPO/LTO，避免 clang-cl 在 RelWithDebInfo 下编译超大嵌入资源时耗尽内存。
    set_target_properties(ui_fonts PROPERTIES
        CXX_CLANG_TIDY ""
        INTERPROCEDURAL_OPTIMIZATION FALSE
        # 不把自动生成的巨型字节数组 lib.cpp 写入 compile_commands.json。
        # 否则 clangd 会把这条“缺少 entt/SDL/yoga 等头文件搜索路径”的命令
        # 误推断给 include/ui.hpp，导致 'entt/entt.hpp' file not found 误报。
        # 关闭后 clangd 会改从真实消费者（main.cpp / ui 源文件）推断完整 flags。
        EXPORT_COMPILE_COMMANDS OFF
    )
endif()

# ==================== Object 库拆分（编译优化） ====================
# 将 ui 拆分为多个 OBJECT 库，每个库拥有独立的编译配置并共享全局内存感知 JOB_POOL：
#   - 全局并行度由顶层按可用内存计算，防止模块独立池叠加绕过内存预算；
#   - 增量构建边界清晰，各模块可独立重编译；
#   - 聚合静态库 ui 由这些 object 库拼装，对外行为（VMPUI::ui / 合并 / install）完全不变。
# 共享编译选项（提取为变量，供每个 object 库应用）
set(_UI_COMMON_COMPILE_OPTIONS
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

option(UI_ENABLE_PCH "Enable precompiled headers in UI object libraries" ON)

function(ui_configure_object_library target_name pch_mode)
    target_compile_features(${target_name} PRIVATE cxx_std_23)
    target_include_directories(${target_name} PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/include
        ${CMAKE_SOURCE_DIR}/third_party/stb
    )
    if(UI_ENABLE_PCH)
        if(pch_mode STREQUAL "CREATE")
            target_precompile_headers(${target_name} PRIVATE
                "$<$<COMPILE_LANGUAGE:CXX>:${CMAKE_CURRENT_SOURCE_DIR}/pch.hpp>"
            )
        elseif(pch_mode STREQUAL "REUSE")
            target_precompile_headers(${target_name} REUSE_FROM ui_core_objects)
        elseif(NOT pch_mode STREQUAL "OFF")
            message(FATAL_ERROR "Unknown PCH mode for ${target_name}: '${pch_mode}'")
        endif()
    endif()

    if(UI_RESOURCE_BACKEND STREQUAL "CMRC")
        target_compile_definitions(${target_name} PRIVATE
            UI_RESOURCE_BACKEND_CMRC=1
            UI_RESOURCE_BACKEND_NAME="CMRC")
    elseif(UI_RESOURCE_BACKEND STREQUAL "STD_EMBED")
        target_compile_definitions(${target_name} PRIVATE
            UI_RESOURCE_BACKEND_STD_EMBED=1
            UI_RESOURCE_BACKEND_NAME="STD_EMBED")
    endif()
    target_compile_definitions(${target_name} PRIVATE UI_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")
    if(UI_FORCE_CPU_RENDER)
        target_compile_definitions(${target_name} PRIVATE UI_FORCE_CPU_RENDER=1)
    endif()

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        if(UI_LINUX_WINDOW_SYSTEM STREQUAL "AUTO" OR UI_LINUX_WINDOW_SYSTEM STREQUAL "X11")
            if(X11_FOUND)
                target_compile_definitions(${target_name} PRIVATE UI_LINUX_X11=1)
            endif()
        endif()
        if(UI_LINUX_WINDOW_SYSTEM STREQUAL "AUTO" OR UI_LINUX_WINDOW_SYSTEM STREQUAL "WAYLAND")
            target_compile_definitions(${target_name} PRIVATE UI_LINUX_WAYLAND=1)
        endif()
    endif()

    # header-only / 编译期依赖的 include path（OBJECT 库不归档，PRIVATE 不污染消费者）
    target_link_libraries(${target_name} PRIVATE
        SDL3::SDL3
        yogacore
        freetype
        harfbuzz
        EnTT::EnTT
        Eigen3::Eigen
    )
    if(UI_RESOURCE_BACKEND STREQUAL "CMRC")
        target_link_libraries(${target_name} PRIVATE ui_fonts)
    endif()

    target_compile_options(${target_name} PRIVATE ${_UI_COMMON_COMPILE_OPTIONS})
    if(ENABLE_COVERAGE AND MSVC)
        target_compile_options(${target_name} PRIVATE /Zi /Od)
    endif()

endfunction()

# 各模块 Object 库
# PCH 策略：core 生成唯一 PCH，systems/render/managers 复用该产物，避免四次
# 重复编译相同 pch.hpp；api/renderers/resources 不使用 PCH。
add_library(ui_core_objects OBJECT ${UI_OBJECT_CORE_SOURCES})
ui_configure_object_library(ui_core_objects CREATE)

add_library(ui_systems_objects OBJECT ${UI_OBJECT_SYSTEMS_SOURCES})
ui_configure_object_library(ui_systems_objects REUSE)

add_library(ui_render_objects OBJECT ${UI_OBJECT_RENDER_SOURCES})
ui_configure_object_library(ui_render_objects REUSE)

add_library(ui_managers_objects OBJECT ${UI_OBJECT_MANAGERS_SOURCES})
ui_configure_object_library(ui_managers_objects REUSE)

add_library(ui_renderers_objects OBJECT ${UI_OBJECT_RENDERERS_SOURCES})
ui_configure_object_library(ui_renderers_objects OFF)

add_library(ui_api_objects OBJECT ${UI_OBJECT_API_SOURCES})
ui_configure_object_library(ui_api_objects OFF)

add_library(ui_resources_objects OBJECT ${UI_OBJECT_RESOURCES_SOURCES})
ui_configure_object_library(ui_resources_objects OFF)

# ==================== 聚合库 ====================
# BUILD_SHARED_LIBS=ON 时产出单 DLL；默认产出含第三方对象的静态单库。
set(_UI_OBJECT_REF_LIST
    $<TARGET_OBJECTS:ui_core_objects>
    $<TARGET_OBJECTS:ui_systems_objects>
    $<TARGET_OBJECTS:ui_render_objects>
    $<TARGET_OBJECTS:ui_managers_objects>
    $<TARGET_OBJECTS:ui_renderers_objects>
    $<TARGET_OBJECTS:ui_api_objects>
    $<TARGET_OBJECTS:ui_resources_objects>
)
if(BUILD_SHARED_LIBS)
    add_library(ui SHARED ${_UI_OBJECT_REF_LIST} ${UI_HEADERS} common/CustomEvent.hpp)
else()
    add_library(ui STATIC ${_UI_OBJECT_REF_LIST} ${UI_HEADERS} common/CustomEvent.hpp)
endif()
add_library(VMPUI::ui ALIAS ui)

# 发行产物统一命名为 VMPUI（静态: VMPUI.lib / 动态: VMPUI.dll + VMPUI.lib 导入库）
set_target_properties(ui PROPERTIES OUTPUT_NAME VMPUI)
if(WIN32 AND BUILD_SHARED_LIBS)
    # 当前公共 API 仍在逐步补齐显式导出宏。CMake 自动导出 OBJECT 库进入 DLL 的
    # 外部符号，确保现有消费者可链接；稳定 ABI 后可替换为逐 API 导出控制。
    set_target_properties(ui PROPERTIES WINDOWS_EXPORT_ALL_SYMBOLS ON)
endif()

# 聚合库只向消费者暴露公共 include（PRIVATE 编译目录已由各 object 库持有）
target_include_directories(ui
    PUBLIC
    $<BUILD_INTERFACE:${CMAKE_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>
)

# 依赖项（静态库的 PRIVATE 依赖会以 $<LINK_ONLY:...> 形式带到最终链接；
# 合并后的 VMPUI.lib 已含第三方对象，此处仍保留以保证动态模式与未合并场景正确）
target_link_libraries(ui PRIVATE
    SDL3::SDL3
    yogacore
    freetype
    harfbuzz
    EnTT::EnTT
    Eigen3::Eigen
)

if(UI_RESOURCE_BACKEND STREQUAL "CMRC")
    target_link_libraries(ui PRIVATE ui_fonts)
endif()

# ==================== 自包含发行：系统库需求收集 ====================
# 合并后的 VMPUI 单库不再携带任何第三方 CMake 目标，消费者只需链接 VMPUI::ui。
# 但 SDL3 静态库在 Windows 上还依赖一组操作系统库（winmm/dxgi/d3d11/d3d12/...）。
# 这些属于系统平台 API（非第三方），通过 PUBLIC $<LINK_ONLY:...> 随 VMPUITargets 导出，
# 消费者无需知道也不需安装任何第三方包。
set(_ui_export_system_libs "")
if(WIN32)
    get_target_property(_ui_sdl_interface SDL3::SDL3 INTERFACE_LINK_LIBRARIES)
    if(_ui_sdl_interface)
        foreach(_lib IN LISTS _ui_sdl_interface)
            # 静态库的 PRIVATE 依赖会以 $<LINK_ONLY:xxx> 形式出现在 INTERFACE
            if(_lib MATCHES "^\\$<LINK_ONLY:([A-Za-z0-9_.+-]+)>$")
                set(_lib "${CMAKE_MATCH_1}")
            endif()
            # 只收集裸系统库名：排除 CMake 目标（含 ::）与嵌套生成器表达式
            if(_lib MATCHES "^[A-Za-z0-9_.+-]+$")
                list(APPEND _ui_export_system_libs "$<LINK_ONLY:${_lib}>")
            endif()
        endforeach()
    endif()
    # SDL3-static 的 GPU（d3d11/d3d12）后端依赖以嵌套生成器表达式形式存在，
    # 自动收集不可靠，显式补齐这些 Windows 系统库（OS/SDK 自带，非第三方）：
    list(APPEND _ui_export_system_libs
        "$<LINK_ONLY:dxgi>" "$<LINK_ONLY:d3d11>" "$<LINK_ONLY:d3d12>" "$<LINK_ONLY:dxguid>")
    # Windows 平台 API（UI 自身需要的，位于下方平台链接段之前先声明）
    list(APPEND _ui_export_system_libs "$<LINK_ONLY:dwmapi>" "$<LINK_ONLY:comctl32>")
    list(REMOVE_DUPLICATES _ui_export_system_libs)
endif()
if(_ui_export_system_libs)
    target_link_libraries(ui PUBLIC ${_ui_export_system_libs})
    message(STATUS "[UI Distribution] Exporting system link libs to consumers: ${_ui_export_system_libs}")
endif()

# ==================== 自包含发行：静态库合并（单文件） ====================
# 将 ui 的对象与全部编译型第三方（SDL3/yogacore/freetype/harfbuzz；CMRC 模式含 ui_fonts）
# 合并为单个 VMPUI.lib，使消费者只链接 VMPUI::ui，无需任何第三方库。
# 注意：合并只在静态模式执行；动态模式第三方直接链接进 VMPUI.dll。
if(NOT BUILD_SHARED_LIBS)
    set(_ui_merge_tmp "${CMAKE_CURRENT_BINARY_DIR}/VMPUI-merged.lib")
    set(_ui_merge_extra_inputs "")
    if(UI_RESOURCE_BACKEND STREQUAL "CMRC")
        list(APPEND _ui_merge_extra_inputs "$<TARGET_FILE:ui_fonts>")
    endif()
    if(MSVC OR (CMAKE_CXX_COMPILER_ID MATCHES "Clang" AND CMAKE_CXX_COMPILER_FRONTEND_VARIANT STREQUAL "MSVC"))
        # MSVC lib.exe / LLVM llvm-lib.exe：直接支持 COFF 库合并
        add_custom_command(TARGET ui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E rm -f "${_ui_merge_tmp}"
            COMMAND "${CMAKE_AR}" /NOLOGO /OUT:${_ui_merge_tmp}
                "$<TARGET_FILE:ui>"
                "$<TARGET_FILE:SDL3::SDL3>"
                "$<TARGET_FILE:yogacore>"
                "$<TARGET_FILE:freetype>"
                "$<TARGET_FILE:harfbuzz>"
                ${_ui_merge_extra_inputs}
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_ui_merge_tmp}" "$<TARGET_FILE:ui>"
            COMMAND ${CMAKE_COMMAND} -E rm -f "${_ui_merge_tmp}"
            COMMENT "[UI Distribution] Merging third-party static libraries into VMPUI.lib"
            VERBATIM)
    else()
        # GNU ar：MRI 脚本展开合并
        set(_ui_merge_mri "${CMAKE_CURRENT_BINARY_DIR}/VMPUI-merge.mri")
        set(_ui_merge_mri_content
            "create \"${_ui_merge_tmp}\"\naddlib \"$<TARGET_FILE:ui>\"\naddlib \"$<TARGET_FILE:SDL3::SDL3>\"\naddlib \"$<TARGET_FILE:yogacore>\"\naddlib \"$<TARGET_FILE:freetype>\"\naddlib \"$<TARGET_FILE:harfbuzz>\"\n")
        if(UI_RESOURCE_BACKEND STREQUAL "CMRC")
            string(APPEND _ui_merge_mri_content "addlib \"$<TARGET_FILE:ui_fonts>\"\n")
        endif()
        string(APPEND _ui_merge_mri_content "save\nend\n")
        file(GENERATE OUTPUT "${_ui_merge_mri}" CONTENT "${_ui_merge_mri_content}")

        # add_custom_command 不通过 shell 执行，不能直接使用 “<” 重定向；
        # 由 CMake execute_process 的 INPUT_FILE 将 MRI 脚本送入 ar 的标准输入。
        set(_ui_merge_runner "${CMAKE_CURRENT_BINARY_DIR}/RunVMPUIMerge.cmake")
        file(GENERATE OUTPUT "${_ui_merge_runner}" CONTENT
            "execute_process(COMMAND \"${CMAKE_AR}\" -M INPUT_FILE \"${_ui_merge_mri}\" RESULT_VARIABLE _result)\nif(NOT _result EQUAL 0)\n  message(FATAL_ERROR \"Archive merge failed with exit code: \${_result}\")\nendif()\n")
        add_custom_command(TARGET ui POST_BUILD
            COMMAND ${CMAKE_COMMAND} -P "${_ui_merge_runner}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_ui_merge_tmp}" "$<TARGET_FILE:ui>"
            COMMENT "[UI Distribution] Merging third-party static libraries into VMPUI.lib"
            VERBATIM)
    endif()
endif()

if(UI_FORCE_CPU_RENDER)
    message(STATUS "[UI] UI_FORCE_CPU_RENDER=ON: GPU backends disabled, using SDL_Renderer software fallback.")
endif()

target_compile_definitions(ui PUBLIC UI_ENABLE_PLATFORM_SCALING=$<BOOL:${UI_ENABLE_PLATFORM_SCALING}>)

# ==================== 平台链接 ====================
# Windows: DWM 透明合成 + 窗口子类化
# dwmapi/comctl32 已由上方 _ui_export_system_libs 以 PUBLIC $<LINK_ONLY:...> 导出给消费者，
# 这里不再重复链接。
if(WIN32)
    # MinGW/GCC 会忽略 #pragma comment(lib, ...)，uxtheme 需在此显式链接。
    if(MINGW)
        target_link_libraries(ui PRIVATE uxtheme)
    endif()
endif()

# Linux: X11 / Wayland 后端
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_ui_linux_need_x11
        $<OR:$<STREQUAL:${UI_LINUX_WINDOW_SYSTEM},AUTO>,$<STREQUAL:${UI_LINUX_WINDOW_SYSTEM},X11>>)

    if(UI_LINUX_WINDOW_SYSTEM STREQUAL "AUTO" OR UI_LINUX_WINDOW_SYSTEM STREQUAL "X11")
        if(X11_FOUND)
            target_link_libraries(ui PRIVATE X11::X11)
            message(STATUS "[UI] Linux: X11 backend enabled")
        elseif(UI_LINUX_WINDOW_SYSTEM STREQUAL "X11")
            message(FATAL_ERROR "[UI] UI_LINUX_WINDOW_SYSTEM=X11 but libX11 not found.\n"
                "  Ubuntu/Debian : sudo apt install libx11-dev\n"
                "  Fedora/RHEL   : dnf install libX11-devel\n"
                "  Arch          : pacman -S libx11")
        else()
            message(STATUS "[UI] Linux: X11 not found; falling back to Wayland-only")
        endif()
    endif()

    if(UI_LINUX_WINDOW_SYSTEM STREQUAL "AUTO" OR UI_LINUX_WINDOW_SYSTEM STREQUAL "WAYLAND")
        # SDL3 已内建 Wayland 支持，无需额外链接 wayland-client
        # xdg-decoration / libdecor 等装饰协议可在此追加（P6 扩展）
        message(STATUS "[UI] Linux: Wayland backend enabled (via SDL3)")
    endif()
endif()
target_compile_features(ui PUBLIC cxx_std_23)

if(ENABLE_COVERAGE AND MSVC)
    # 编译期调试信息已由 ui_configure_object_library 应用到各 object 库；这里只保留链接期选项。
    target_link_options(ui PRIVATE /DEBUG)
endif()

# ==================== SDK install（自包含发行） ====================
# 发行包只包含：VMPUI 库（静态模式为合并后的单库 VMPUI.lib，动态模式为 VMPUI.dll + 导入库）、
# 公共头 include/ui/**、CMake config。不安装任何第三方库目标或头文件。
install(TARGETS ui
    EXPORT VMPUITargets
    ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY "${CMAKE_SOURCE_DIR}/include/"
    DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
)


