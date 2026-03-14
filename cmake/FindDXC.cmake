# FindDXC.cmake - Find DirectX Shader Compiler (dxc)
#
# This module defines:
#  DXC_FOUND - System has dxc
#  DXC_EXECUTABLE - Path to dxc executable
#  DXC_VERSION - Version of dxc

# Try to find dxc in common locations
find_program(DXC_EXECUTABLE
    NAMES dxc dxc.exe
    HINTS
        "D:/vulkan/Bin"
        "D:/Vulkan/Bin"
        "D:/VulkanSDK/Bin"
        ENV DXC_DIR
        ENV VULKAN_SDK
        "C:/Program Files (x86)/Windows Kits/10/bin"
    PATH_SUFFIXES
        bin
        Bin
        x64
        Bin32
        10.0.22621.0/x64
        10.0.22000.0/x64
        10.0.19041.0/x64
    DOC "DirectX Shader Compiler (dxc)"
)

# Get version if found
if(DXC_EXECUTABLE)
    execute_process(
        COMMAND ${DXC_EXECUTABLE} -?
        OUTPUT_VARIABLE DXC_VERSION_OUTPUT
        ERROR_VARIABLE DXC_VERSION_OUTPUT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )
    
    if(DXC_VERSION_OUTPUT MATCHES "dxc version ([0-9]+\\.[0-9]+)")
        set(DXC_VERSION "${CMAKE_MATCH_1}")
    endif()
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
