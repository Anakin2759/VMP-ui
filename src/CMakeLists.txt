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
# CPU 渲染选项：跳过所有 GPU 后端，直接使用 SDL_Renderer software 后端（用于无 GPU 环境或调试）
option(UI_FORCE_CPU_RENDER "Force CPU-only software rendering (skip all GPU backends)" OFF)
# 线程池：默认单线程，开启后启用 worker 线程做异步任务（图片解码等 CPU 热点）
option(UI_ENABLE_MULTITHREAD "Enable worker threads in ui::utils::ThreadPool (default OFF, runs all tasks inline)" OFF)
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


# 资源后端选项（CMRC 或 STD_EMBED）
set(UI_RESOURCE_BACKEND "CMRC" CACHE STRING "UI resource backend (CMRC or STD_EMBED)")
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

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

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


set(UI_SOURCES 
    core/Application.cpp
    core/EventLoop.cpp
    core/RuntimeFacade.cpp
    core/WorkerMailbox.cpp
    core/SystemManager.cpp
    systems/LayoutSystem.cpp
    systems/HitTestSystem.cpp
    systems/StateSystem.cpp
    systems/ThemeSystem.cpp
    systems/render/RenderBackend.cpp
    systems/render/RenderResources.cpp
    systems/render/RenderFrame.cpp
    systems/TimerSystem.cpp
    systems/ShortcutSystem.cpp
    managers/FontManager.cpp
    managers/IconManager.cpp
    managers/ResourceProvider.cpp
    managers/TextTextureCache.cpp
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
    api/Query.cpp
    api/Canvas.cpp
    api/Table.cpp
    api/Theme.cpp
    api/Shortcut.cpp
    api/Log.cpp
    managers/ImageManager.cpp
    core/PlatformWindow.cpp
    renderers/TableRenderer.cpp
    common/ErrorCodes.cpp
)
set(UI_HEADERS
    # Components
    common/CustomizationPoints.hpp
    common/Policies.hpp
    common/Tags.hpp
    common/Events.hpp
    common/Types.hpp
    common/WindowSync.hpp
    common/RenderTypes.hpp
    common/Result.hpp
    common/ErrorCodes.hpp
    common/ThreadPool.hpp
    
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
    renderers/ImageRenderer.hpp
    renderers/CanvasRenderer.hpp
    renderers/TableRenderer.hpp
    renderers/CheckBoxRenderer.hpp
    renderers/DropDownRenderer.hpp


    singleton/Registry.hpp
    singleton/Logger.hpp
    singleton/Dispatcher.hpp
    
    traits/ComponentsTraits.hpp
    traits/PoliciesTraits.hpp

    # Interface
    interface/ISystem.hpp
    
    # API
    api/Utils.hpp
    api/Layout.hpp
    api/Controls.hpp
    api/Canvas.hpp
    api/Table.hpp
    api/Theme.hpp
    api/Shortcut.hpp
    managers/ImageManager.hpp
    systems/ShortcutSystem.hpp
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
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/third_party/entt/single_include
)

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
    yogacore
    freetype
    harfbuzz
)

if(UI_RESOURCE_BACKEND STREQUAL "CMRC")
    target_link_libraries(ui PRIVATE ui_fonts)
endif()

target_link_libraries(ui PRIVATE
    EnTT::EnTT
)
target_link_libraries(ui PUBLIC
    asio::asio
    Eigen3::Eigen
    spdlog::spdlog_header_only
)

target_compile_definitions(ui PRIVATE UI_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets")

if(UI_FORCE_CPU_RENDER)
    target_compile_definitions(ui PRIVATE UI_FORCE_CPU_RENDER=1)
    message(STATUS "[UI] UI_FORCE_CPU_RENDER=ON: GPU backends disabled, using SDL_Renderer software fallback.")
endif()

if(UI_ENABLE_MULTITHREAD)
    target_compile_definitions(ui PUBLIC UI_ENABLE_MULTITHREAD=1)
    message(STATUS "[UI] UI_ENABLE_MULTITHREAD=ON: ThreadPool will spawn worker threads.")
else()
    target_compile_definitions(ui PUBLIC UI_ENABLE_MULTITHREAD=0)
    message(STATUS "[UI] UI_ENABLE_MULTITHREAD=OFF: ThreadPool runs all tasks inline (default).")
endif()

# ==================== 平台链接 ====================
# Windows: DWM 透明合成 + 窗口子类化
if(WIN32)
    target_link_libraries(ui PRIVATE dwmapi comctl32)
    # MinGW/GCC 会忽略 #pragma comment(lib, ...)，ASIO 的 ws2_32/mswsock
    # 以及 uxtheme 均需在此显式链接。
    if(MINGW)
        target_link_libraries(ui PRIVATE ws2_32 mswsock uxtheme)
    endif()
endif()

# Linux: X11 / Wayland 后端
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(_ui_linux_need_x11
        $<OR:$<STREQUAL:${UI_LINUX_WINDOW_SYSTEM},AUTO>,$<STREQUAL:${UI_LINUX_WINDOW_SYSTEM},X11>>)

    if(UI_LINUX_WINDOW_SYSTEM STREQUAL "AUTO" OR UI_LINUX_WINDOW_SYSTEM STREQUAL "X11")
        find_package(X11 QUIET)
        if(X11_FOUND)
            target_link_libraries(ui PRIVATE X11::X11)
            target_compile_definitions(ui PRIVATE UI_LINUX_X11=1)
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
        target_compile_definitions(ui PRIVATE UI_LINUX_WAYLAND=1)
        message(STATUS "[UI] Linux: Wayland backend enabled (via SDL3)")
    endif()
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


