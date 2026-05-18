# FindDXC.cmake - Find DirectX Shader Compiler (dxc)
#
# This module defines:
#  DXC_FOUND - System has dxc
#  DXC_EXECUTABLE - Path to dxc executable
#  DXC_VERSION - Version of dxc

# 优先搜索支持 SPIR-V 的 Vulkan SDK 版本 DXC，再兜底 Windows Kits（不支持 SPIR-V）
find_program(DXC_EXECUTABLE
    NAMES dxc dxc.exe
    HINTS
        # Windows: Vulkan SDK 常见安装路径（支持 SPIR-V + DXIL）
        "D:/vulkan/Bin"
        "D:/Vulkan/Bin"
        "D:/VulkanSDK/Bin"
        "C:/VulkanSDK/Bin"
        ENV DXC_DIR
        ENV VULKAN_SDK
        # Linux: 发行版包管理器 (apt: directx-shader-compiler) 或 Vulkan SDK
        /usr/bin
        /usr/local/bin
        /opt/vulkan-sdk/bin
        # 兜底：Windows Kits 自带 DXC（仅 DXIL，无 SPIR-V）
        "C:/Program Files (x86)/Windows Kits/10/bin"
    PATH_SUFFIXES
        bin
        Bin
        x64
        Bin32
        10.0.22621.0/x64
        10.0.22000.0/x64
        10.0.19041.0/x64
        arm64
    DOC "DirectX Shader Compiler (dxc)"
)

# 获取版本 + 检测 SPIR-V 能力（实际试编译，帮助文本不可靠）
# 注意：Windows SDK 自带 dxc（Windows Kits 路径）的帮助文本里有 -spirv 字样，
#       但 SPIRV CodeGen 没有编译进去，运行时报 "SPIR-V CodeGen not available"。
#       因此必须用试编译来检测，不能靠帮助文本字符串匹配。
if(DXC_EXECUTABLE)
    execute_process(
        COMMAND ${DXC_EXECUTABLE} -?
        OUTPUT_VARIABLE _DXC_HELP
        ERROR_VARIABLE _DXC_HELP
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )

    if(_DXC_HELP MATCHES "dxc version ([0-9]+\\.[0-9]+)")
        set(DXC_VERSION "${CMAKE_MATCH_1}")
    endif()
    unset(_DXC_HELP)

    set(_DXC_TEST_HLSL "${CMAKE_BINARY_DIR}/CMakeFiles/_dxc_cap_test.hlsl")
    # 使用包含 register space 语法的测试 HLSL（与实际着色器保持一致）
    # 部分旧版 DXC（如 Windows Kits 10.0.19041）对简单 HLSL 能通过，
    # 但遇到 cbuffer register spaces 时内部 DLL 函数缺失（0x80070459）
    file(WRITE "${_DXC_TEST_HLSL}"
        "cbuffer UiConstants : register(b0, space1) { float4 data; };\n"
        "float4 main() : SV_Target { return data; }\n"
    )

    # 检测 SPIR-V 支持（帮助文本不可靠；Windows Kits DXC 有 -spirv 字样但 CodeGen 未编译进去）
    set(_DXC_TEST_SPV "${CMAKE_BINARY_DIR}/CMakeFiles/_dxc_cap_test.spv")
    execute_process(
        COMMAND ${DXC_EXECUTABLE} -spirv -T ps_6_0 -E main
            -Fo "${_DXC_TEST_SPV}" "${_DXC_TEST_HLSL}"
        RESULT_VARIABLE _DXC_SPIRV_RESULT
        OUTPUT_QUIET ERROR_QUIET
    )
    if(_DXC_SPIRV_RESULT EQUAL 0)
        set(DXC_SUPPORTS_SPIRV TRUE)
    else()
        set(DXC_SUPPORTS_SPIRV FALSE)
    endif()
    file(REMOVE "${_DXC_TEST_SPV}")

    # 检测 DXIL / Shader Model 6.0 支持（用 sm6.0 而非 sm6.6；
    # sm6.6 需要 D3D12 Agility SDK，Windows 10 无 Agility SDK 时 PSO 创建会报 E_INVALIDARG）
    set(_DXC_TEST_DXIL "${CMAKE_BINARY_DIR}/CMakeFiles/_dxc_cap_test.dxil")
    execute_process(
        COMMAND ${DXC_EXECUTABLE} -T ps_6_0 -E main
            -Fo "${_DXC_TEST_DXIL}" "${_DXC_TEST_HLSL}"
        RESULT_VARIABLE _DXC_DXIL_RESULT
        OUTPUT_QUIET ERROR_QUIET
    )
    if(_DXC_DXIL_RESULT EQUAL 0)
        set(DXC_SUPPORTS_DXIL TRUE)
    else()
        set(DXC_SUPPORTS_DXIL FALSE)
    endif()
    file(REMOVE "${_DXC_TEST_HLSL}" "${_DXC_TEST_DXIL}")

    if(DXC_SUPPORTS_SPIRV AND DXC_SUPPORTS_DXIL)
        message(STATUS "DXC capability: SPIR-V + DXIL (Vulkan SDK)")
    elseif(DXC_SUPPORTS_DXIL)
        message(STATUS "DXC capability: DXIL only (no SPIR-V; Windows Kits or partial build)")
    elseif(DXC_SUPPORTS_SPIRV)
        message(STATUS "DXC capability: SPIR-V only")
    else()
        message(STATUS "DXC found (${DXC_EXECUTABLE}) but cannot compile SPIR-V or DXIL sm6.6 "
            "(too old, e.g. Windows Kits 10.0.19041). Will use precompiled shader binaries. "
            "Install Vulkan SDK: https://vulkan.lunarg.com")
    endif()

    unset(_DXC_SPIRV_RESULT)
    unset(_DXC_DXIL_RESULT)
    unset(_DXC_TEST_SPV)
    unset(_DXC_TEST_DXIL)
    unset(_DXC_TEST_HLSL)
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(DXC
    REQUIRED_VARS DXC_EXECUTABLE
    VERSION_VAR DXC_VERSION
)

mark_as_advanced(DXC_EXECUTABLE DXC_VERSION)

# Function to compile HLSL shaders
function(compile_hlsl_shader)
    set(options "")
    set(oneValueArgs TARGET SOURCE OUTPUT_DIR)
    set(multiValueArgs TARGETS ENTRY_POINTS PROFILES FORMATS)
    cmake_parse_arguments(SHADER "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT DXC_FOUND)
        message(WARNING "DXC not found, shaders will not be compiled")
        return()
    endif()

    get_filename_component(SHADER_NAME ${SHADER_SOURCE} NAME_WE)
    set(OUTPUTS "")

    list(LENGTH SHADER_TARGETS NUM_TARGETS)
    math(EXPR NUM_ITERATIONS "${NUM_TARGETS} - 1")

    foreach(IDX RANGE ${NUM_ITERATIONS})
        list(GET SHADER_TARGETS ${IDX} TARGET_TYPE)
        list(GET SHADER_ENTRY_POINTS ${IDX} ENTRY_POINT)
        list(GET SHADER_PROFILES ${IDX} PROFILE)
        list(GET SHADER_FORMATS ${IDX} FORMAT)

        if(FORMAT STREQUAL "spv")
            set(OUTPUT_FILE "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.${FORMAT}")
            set(COMPILE_ARGS -spirv -T ${PROFILE} -E ${ENTRY_POINT} -fspv-target-env=vulkan1.3 -Fo ${OUTPUT_FILE} ${SHADER_SOURCE})
        elseif(FORMAT STREQUAL "dxil")
            set(OUTPUT_FILE "${SHADER_OUTPUT_DIR}/${SHADER_NAME}.${FORMAT}")
            set(COMPILE_ARGS -T ${PROFILE} -E ${ENTRY_POINT} -Fo ${OUTPUT_FILE} ${SHADER_SOURCE})
        else()
            message(FATAL_ERROR "Unsupported shader format: ${FORMAT}")
        endif()

        add_custom_command(
            OUTPUT ${OUTPUT_FILE}
            COMMAND ${DXC_EXECUTABLE} ${COMPILE_ARGS}
            DEPENDS ${SHADER_SOURCE}
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            COMMENT "Compiling ${TARGET_TYPE} shader: ${SHADER_NAME}.${FORMAT}"
            VERBATIM
        )

        list(APPEND OUTPUTS ${OUTPUT_FILE})
    endforeach()

    add_custom_target(${SHADER_TARGET}
        DEPENDS ${OUTPUTS}
        COMMENT "Building shader target: ${SHADER_TARGET}"
    )
endfunction()
