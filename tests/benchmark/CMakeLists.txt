# GPU 窗口生命周期压力基准（Google Benchmark）
# 与单元测试分离，避免 100 次真实窗口创建/销毁阻塞 ui_unit_tests。

# P0-2：add_executable 源文件存在性前置校验（与 tests/unittest 相同策略），
# 避免悬空引用在 configure 阶段才暴露（历史：test_ThreadPool.cpp）。
function(ui_validate_sources_exist target_name)
    foreach(source_file ${ARGN})
        set(absolute_path "${CMAKE_CURRENT_SOURCE_DIR}/${source_file}")
        if(NOT EXISTS "${absolute_path}")
            message(FATAL_ERROR
                "[P0-2] 目标 ${target_name} 引用的源文件不存在: ${absolute_path}\n"
                "请修正 ${CMAKE_CURRENT_LIST_FILE}：删除悬空引用，或补回缺失文件。")
        endif()
    endforeach()
endfunction()

function(add_ui_test_executable target_name)
    ui_validate_sources_exist(${target_name} ${ARGN})
    add_executable(${target_name} ${ARGN})
endfunction()

add_ui_test_executable(ui_gpu_lifecycle_benchmark
    gpu_window_lifecycle_benchmark.cpp
)
target_compile_features(ui_gpu_lifecycle_benchmark PRIVATE cxx_std_23)
target_compile_definitions(ui_gpu_lifecycle_benchmark PRIVATE
    SDL_MAIN_HANDLED
)
target_include_directories(ui_gpu_lifecycle_benchmark PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_link_libraries(ui_gpu_lifecycle_benchmark PRIVATE
    EnTT::EnTT
    Eigen3::Eigen
    SDL3::SDL3
    ui
    benchmark::benchmark_main
)

target_compile_options(ui_gpu_lifecycle_benchmark PRIVATE
    # MSVC
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/W4 /Od /Zi /EHsc>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2 /DNDEBUG /EHsc>

    # Clang-cl
    $<$<AND:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>,$<CONFIG:Debug>>:/EHsc /Zi /W4>
    $<$<AND:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>,$<CONFIG:Release>>:/EHsc /O2 /DNDEBUG>
)
