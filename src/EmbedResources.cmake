# EmbedResources.cmake - 跨平台大文件嵌入模块
include_guard()

# 选项设置
option(EMBED_RESOURCES_VERBOSE "显示嵌入资源详细信息" OFF)

# 主函数：嵌入文件
function(embed_file)
    set(oneValueArgs TARGET NAMESPACE SYMBOL OUTPUT_HEADER)
    set(multiValueArgs FILES)
    cmake_parse_arguments(ARG "" "${oneValueArgs}" "${multiValueFiles}" ${ARGN})
    
    if(NOT ARG_TARGET)
        message(FATAL_ERROR "必须指定 TARGET 参数")
    endif()
    
    if(NOT ARG_FILES)
        message(FATAL_ERROR "必须指定要嵌入的文件")
    endif()
    
    # 为每个平台选择合适的嵌入策略
    if(WIN32)
        _embed_file_windows(${ARG_TARGET} "${ARG_FILES}" "${ARG_NAMESPACE}" "${ARG_SYMBOL}" "${ARG_OUTPUT_HEADER}")
    elseif(APPLE)
        _embed_file_apple(${ARG_TARGET} "${ARG_FILES}" "${ARG_NAMESPACE}" "${ARG_SYMBOL}" "${ARG_OUTPUT_HEADER}")
    else()
        _embed_file_linux(${ARG_TARGET} "${ARG_FILES}" "${ARG_NAMESPACE}" "${ARG_SYMBOL}" "${ARG_OUTPUT_HEADER}")
    endif()
endfunction()

# Windows 实现：使用 .rc 资源文件
function(_embed_file_windows TARGET FILES NAMESPACE SYMBOL OUTPUT_HEADER)
    # 创建 .rc 文件
    set(RC_CONTENT "")
    set(INDEX 0)
    
    foreach(FILE_PATH ${FILES})
        # 转换为绝对路径
        get_filename_component(ABS_FILE_PATH ${FILE_PATH} ABSOLUTE)
        
        # 生成资源ID
        math(EXPR RESOURCE_ID "100 + ${INDEX}")
        
        # 添加到 .rc 文件
        string(APPEND RC_CONTENT
            "${SYMBOL}_${INDEX} RCDATA \"${ABS_FILE_PATH}\"\n"
        )
        
        list(APPEND RESOURCE_IDS ${RESOURCE_ID})
        list(APPEND RESOURCE_PATHS ${ABS_FILE_PATH})
        math(EXPR INDEX "${INDEX} + 1")
    endforeach()
    
    # 写入 .rc 文件
    set(RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_resources.rc")
    file(WRITE ${RC_FILE} ${RC_CONTENT})
    
    # 编译资源文件
    set(RES_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET}_resources.res")
    add_custom_command(
        OUTPUT ${RES_FILE}
        COMMAND ${CMAKE_RC_COMPILER} 
                /nologo
                /fo ${RES_FILE}
                ${RC_FILE}
        DEPENDS ${RC_FILE}
        COMMENT "编译Windows资源文件"
    )
    
    # 生成头文件
    if(OUTPUT_HEADER)
        _generate_windows_header(${TARGET} "${RESOURCE_PATHS}" "${RESOURCE_IDS}" 
                                ${NAMESPACE} ${SYMBOL} ${OUTPUT_HEADER})
    endif()
    
    # 添加到目标
    target_sources(${TARGET} PRIVATE ${RES_FILE})
endfunction()

# Linux/Unix 实现：使用 objcopy
function(_embed_file_linux TARGET FILES NAMESPACE SYMBOL OUTPUT_HEADER)
    # 检查 objcopy 是否存在
    find_program(OBJCOPY_EXECUTABLE objcopy)
    if(NOT OBJCOPY_EXECUTABLE)
        message(WARNING "objcopy not found, falling back to xxd")
        _embed_file_xxd(${TARGET} "${FILES}" ${NAMESPACE} ${SYMBOL} ${OUTPUT_HEADER})
        return()
    endif()
    
    set(OBJECT_FILES "")
    set(INDEX 0)
    
    foreach(FILE_PATH ${FILES})
        get_filename_component(ABS_FILE_PATH ${FILE_PATH} ABSOLUTE)
        get_filename_component(FILE_NAME ${FILE_PATH} NAME)
        
        # 清理文件名用作符号名
        string(MAKE_C_IDENTIFIER ${FILE_NAME} FILE_SYMBOL)
        string(TOUPPER ${FILE_SYMBOL} FILE_SYMBOL)
        
        # 生成目标文件
        set(OBJ_FILE "${CMAKE_CURRENT_BINARY_DIR}/${FILE_SYMBOL}_${INDEX}.o")
        add_custom_command(
            OUTPUT ${OBJ_FILE}
            COMMAND ${OBJCOPY_EXECUTABLE}
                    -I binary
                    -O ${CMAKE_SYSTEM_PROCESSOR}-${CMAKE_SYSTEM_NAME_LOWER}
                    -B ${CMAKE_SYSTEM_PROCESSOR}
                    --redefine-sym _binary_${FILE_SYMBOL}_start=_binary_${FILE_SYMBOL}_${INDEX}_start
                    --redefine-sym _binary_${FILE_SYMBOL}_end=_binary_${FILE_SYMBOL}_${INDEX}_end
                    --redefine-sym _binary_${FILE_SYMBOL}_size=_binary_${FILE_SYMBOL}_${INDEX}_size
                    ${ABS_FILE_PATH}
                    ${OBJ_FILE}
            DEPENDS ${ABS_FILE_PATH}
            COMMENT "嵌入文件: ${FILE_NAME}"
        )
        
        list(APPEND OBJECT_FILES ${OBJ_FILE})
        list(APPEND SYMBOL_NAMES ${FILE_SYMBOL}_${INDEX})
        math(EXPR INDEX "${INDEX} + 1")
    endforeach()
    
    # 创建静态库
    add_library(${TARGET}_embedded STATIC ${OBJECT_FILES})
    
    # 生成头文件
    if(OUTPUT_HEADER)
        _generate_unix_header(${TARGET} "${FILES}" "${SYMBOL_NAMES}" 
                            ${NAMESPACE} ${SYMBOL} ${OUTPUT_HEADER})
    endif()
    
    # 链接到目标
    target_link_libraries(${TARGET} PRIVATE ${TARGET}_embedded)
endfunction()

# macOS 实现：使用 xxd 或 linker sections
function(_embed_file_apple TARGET FILES NAMESPACE SYMBOL OUTPUT_HEADER)
    # macOS 可以使用 __DATA 段
    find_program(XXD_EXECUTABLE xxd)
    if(XXD_EXECUTABLE)
        _embed_file_xxd(${TARGET} "${FILES}" ${NAMESPACE} ${SYMBOL} ${OUTPUT_HEADER})
    else()
        # 回退到通用的 xxd 实现
        _embed_file_generic(${TARGET} "${FILES}" ${NAMESPACE} ${SYMBOL} ${OUTPUT_HEADER})
    endif()
endfunction()

# 通用的 xxd 实现（跨平台后备）
function(_embed_file_xxd TARGET FILES NAMESPACE SYMBOL OUTPUT_HEADER)
    find_program(XXD_EXECUTABLE xxd)
    if(NOT XXD_EXECUTABLE)
        # 如果没有 xxd，使用 Python 实现类似功能
        _embed_file_python(${TARGET} "${FILES}" ${NAMESPACE} ${SYMBOL} ${OUTPUT_HEADER})
        return()
    endif()
    
    set(GENERATED_FILES "")
    set(DECLARATIONS "")
    set(IMPLEMENTATIONS "")
    
    foreach(FILE_PATH ${FILES})
        get_filename_component(ABS_FILE_PATH ${FILE_PATH} ABSOLUTE)
        get_filename_component(FILE_NAME ${FILE_PATH} NAME)
        string(MAKE_C_IDENTIFIER ${FILE_NAME} VAR_NAME)
        
        # 生成 C 文件
        set(C_FILE "${CMAKE_CURRENT_BINARY_DIR}/${VAR_NAME}.c")
        add_custom_command(
            OUTPUT ${C_FILE}
            COMMAND ${XXD_EXECUTABLE} -i ${ABS_FILE_PATH} ${C_FILE}.tmp
            COMMAND ${CMAKE_COMMAND} -E rename ${C_FILE}.tmp ${C_FILE}
            DEPENDS ${ABS_FILE_PATH}
            COMMENT "使用 xxd 嵌入: ${FILE_NAME}"
        )
        
        list(APPEND GENERATED_FILES ${C_FILE})
        list(APPEND VAR_NAMES ${VAR_NAME})
    endforeach()
    
    # 创建库
    add_library(${TARGET}_embedded STATIC ${GENERATED_FILES})
    
    # 生成头文件
    if(OUTPUT_HEADER)
        _generate_xxd_header(${TARGET} "${FILES}" "${VAR_NAMES}" 
                           ${NAMESPACE} ${SYMBOL} ${OUTPUT_HEADER})
    endif()
    
    target_link_libraries(${TARGET} PRIVATE ${TARGET}_embedded)
endfunction()

# Python 实现（无外部依赖）
function(_embed_file_python TARGET FILES NAMESPACE SYMBOL OUTPUT_HEADER)
    set(PYTHON_SCRIPT "${CMAKE_CURRENT_BINARY_DIR}/embed_resources.py")
    
    # 创建 Python 脚本
    file(WRITE ${PYTHON_SCRIPT}
"#!/usr/bin/env python3
import sys
import os

def embed_file(input_file, output_file, var_name):
    with open(input_file, 'rb') as f:
        data = f.read()
    
    with open(output_file, 'w') as f:
        f.write('#include <cstddef>\\n\\n')
        f.write(f'namespace {sys.argv[1]} {{\\n')
        f.write(f'    extern const unsigned char {var_name}_data[];\\n')
        f.write(f'    extern const size_t {var_name}_size;\\n')
        f.write(f'}}\\n\\n')
        
        f.write(f'const unsigned char {var_name}_data[] = {{\\n')
        for i in range(0, len(data), 16):
            chunk = data[i:i+16]
            hex_values = ', '.join(f'0x{b:02x}' for b in chunk)
            f.write(f'    {hex_values},\\n')
        f.write('};\n\n')
        
        f.write(f'const size_t {var_name}_size = {len(data)};\\n')
    
    print(f'Embedded {input_file} -> {output_file}')

if __name__ == '__main__':
    if len(sys.argv) < 5:
        sys.exit(1)
    
    namespace = sys.argv[1]
    input_file = sys.argv[2]
    output_file = sys.argv[3]
    var_name = sys.argv[4]
    
    embed_file(input_file, output_file, var_name)
")
    
    set(GENERATED_FILES "")
    set(VAR_NAMES "")
    
    foreach(FILE_PATH ${FILES})
        get_filename_component(ABS_FILE_PATH ${FILE_PATH} ABSOLUTE)
        get_filename_component(FILE_NAME ${FILE_PATH} NAME)
        string(MAKE_C_IDENTIFIER ${FILE_NAME} VAR_NAME)
        
        # 生成 C++ 文件
        set(CPP_FILE "${CMAKE_CURRENT_BINARY_DIR}/${VAR_NAME}_embedded.cpp")
        add_custom_command(
            OUTPUT ${CPP_FILE}
            COMMAND ${CMAKE_COMMAND} -E env PYTHONIOENCODING=utf-8
                    python3 ${PYTHON_SCRIPT}
                    ${NAMESPACE}
                    ${ABS_FILE_PATH}
                    ${CPP_FILE}
                    ${VAR_NAME}
            DEPENDS ${ABS_FILE_PATH} ${PYTHON_SCRIPT}
            COMMENT "嵌入文件: ${FILE_NAME}"
        )
        
        list(APPEND GENERATED_FILES ${CPP_FILE})
        list(APPEND VAR_NAMES ${VAR_NAME})
    endforeach()
    
    # 创建库
    add_library(${TARGET}_embedded STATIC ${GENERATED_FILES})
    target_link_libraries(${TARGET} PRIVATE ${TARGET}_embedded)
endfunction()

# 生成 Windows 头文件
function(_generate_windows_header TARGET FILES RESOURCE_IDS NAMESPACE SYMBOL OUTPUT_HEADER)
    set(HEADER_CONTENT 
"#pragma once
#include <cstddef>
#include <cstdint>
#include <windows.h>

#if defined(_WIN32)
    #define EMBED_RESOURCES_API __declspec(dllimport)
#else
    #define EMBED_RESOURCES_API
#endif

namespace ${NAMESPACE} {
")

    set(INDEX 0)
    foreach(FILE_PATH ${FILES} RESOURCE_ID ${RESOURCE_IDS})
        get_filename_component(FILE_NAME ${FILE_PATH} NAME)
        string(MAKE_C_IDENTIFIER ${FILE_NAME} VAR_NAME)
        
        string(APPEND HEADER_CONTENT 
"    // ${FILE_NAME}
    EMBED_RESOURCES_API const uint8_t* get_${VAR_NAME}_data();
    EMBED_RESOURCES_API size_t get_${VAR_NAME}_size();
    EMBED_RESOURCES_API bool extract_${VAR_NAME}(const char* output_path);
    
")
        math(EXPR INDEX "${INDEX} + 1")
    endforeach()
    
    string(APPEND HEADER_CONTENT "}\n")
    
    # 生成实现文件
    set(IMPL_FILE "${CMAKE_CURRENT_BINARY_DIR}/${SYMBOL}_impl.cpp")
    set(IMPL_CONTENT 
"#include \"${OUTPUT_HEADER}\"
#include <fstream>

namespace ${NAMESPACE} {
")
    
    set(INDEX 0)
    foreach(FILE_PATH ${FILES} RESOURCE_ID ${RESOURCE_IDS})
        get_filename_component(FILE_NAME ${FILE_PATH} NAME)
        string(MAKE_C_IDENTIFIER ${FILE_NAME} VAR_NAME)
        
        string(APPEND IMPL_CONTENT 
"    const uint8_t* get_${VAR_NAME}_data() {
        HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(${RESOURCE_ID}), RT_RCDATA);
        if (!hRes) return nullptr;
        
        HGLOBAL hData = LoadResource(NULL, hRes);
        if (!hData) return nullptr;
        
        return static_cast<const uint8_t*>(LockResource(hData));
    }
    
    size_t get_${VAR_NAME}_size() {
        HRSRC hRes = FindResource(NULL, MAKEINTRESOURCE(${RESOURCE_ID}), RT_RCDATA);
        if (!hRes) return 0;
        
        return SizeofResource(NULL, hRes);
    }
    
    bool extract_${VAR_NAME}(const char* output_path) {
        auto data = get_${VAR_NAME}_data();
        auto size = get_${VAR_NAME}_size();
        
        if (!data || size == 0) return false;
        
        std::ofstream file(output_path, std::ios::binary);
        if (!file) return false;
        
        file.write(reinterpret_cast<const char*>(data), size);
        return file.good();
    }
    
")
        math(EXPR INDEX "${INDEX} + 1")
    endforeach()
    
    string(APPEND IMPL_CONTENT "}\n")
    
    # 写入文件
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_HEADER}" ${HEADER_CONTENT})
    file(WRITE ${IMPL_FILE} ${IMPL_CONTENT})
    
    # 添加到目标
    target_sources(${TARGET} PRIVATE ${IMPL_FILE})
    target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
endfunction()

# 生成 Unix 头文件
function(_generate_unix_header TARGET FILES SYMBOL_NAMES NAMESPACE SYMBOL OUTPUT_HEADER)
    set(HEADER_CONTENT 
"#pragma once
#include <cstddef>
#include <cstdint>

extern \"C\" {
")

    foreach(SYMBOL_NAME ${SYMBOL_NAMES})
        string(APPEND HEADER_CONTENT 
"    extern const uint8_t _binary_${SYMBOL_NAME}_start[];
    extern const uint8_t _binary_${SYMBOL_NAME}_end[];
    extern const size_t _binary_${SYMBOL_NAME}_size;
")
    endforeach()
    
    string(APPEND HEADER_CONTENT 
"}
    
namespace ${NAMESPACE} {
")
    
    set(INDEX 0)
    foreach(FILE_PATH ${FILES})
        get_filename_component(FILE_NAME ${FILE_PATH} NAME)
        string(MAKE_C_IDENTIFIER ${FILE_NAME} VAR_NAME)
        
        list(GET SYMBOL_NAMES ${INDEX} SYMBOL_NAME)
        
        string(APPEND HEADER_CONTENT 
"    // ${FILE_NAME}
    inline const uint8_t* get_${VAR_NAME}_data() {
        return _binary_${SYMBOL_NAME}_start;
    }
    
    inline size_t get_${VAR_NAME}_size() {
        return _binary_${SYMBOL_NAME}_size;
    }
    
    inline bool extract_${VAR_NAME}(const char* output_path) {
        std::ofstream file(output_path, std::ios::binary);
        if (!file) return false;
        
        file.write(reinterpret_cast<const char*>(get_${VAR_NAME}_data()), 
                  get_${VAR_NAME}_size());
        return file.good();
    }
    
")
        math(EXPR INDEX "${INDEX} + 1")
    endforeach()
    
    string(APPEND HEADER_CONTENT "}\n")
    
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/${OUTPUT_HEADER}" ${HEADER_CONTENT})
    target_include_directories(${TARGET} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
endfunction()