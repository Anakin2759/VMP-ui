# UI module tests

# P0-2：add_executable 源文件存在性前置校验。
# 历史教训：test_ThreadPool.cpp 悬空引用曾导致 ENABLE_BUILD_TESTS=ON 时 configure/构建失败且错误不直观。
# 此函数在 configure 阶段校验所有引用的源文件存在，缺失时给出明确诊断，而不是留到编译期才暴露。
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

# 带存在性校验的 add_executable 包装：先校验后创建。
function(add_ui_test_executable target_name)
    ui_validate_sources_exist(${target_name} ${ARGN})
    add_executable(${target_name} ${ARGN})
endfunction()

function(configure_ui_test_target target_name)
    target_compile_features(${target_name} PRIVATE cxx_std_23)
    target_compile_definitions(${target_name} PRIVATE
        SDL_MAIN_HANDLED
    )
    target_include_directories(${target_name} PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/src
    )
    target_link_libraries(${target_name} PRIVATE
        EnTT::EnTT
        Eigen3::Eigen
        SDL3::SDL3
        ui
        GTest::gmock
        GTest::gmock_main
        GTest::gtest_main
    )

    target_compile_options(${target_name} PRIVATE
        # GCC
        $<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Debug>>:-Wall -Wextra -Wpedantic -O0 -g>
        $<$<AND:$<CXX_COMPILER_ID:GNU>,$<CONFIG:Release>>:-Wall -O3 -DNDEBUG>

        # Clang
        $<$<AND:$<CXX_COMPILER_ID:Clang>,$<NOT:$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>,$<CONFIG:Debug>>:-Wall -Wextra -Wpedantic -O0 -g>
        $<$<AND:$<CXX_COMPILER_ID:Clang>,$<NOT:$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>,$<CONFIG:Release>>:-Wall -O3 -DNDEBUG>

        # MSVC
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Debug>>:/W4 /Od /Zi /EHsc>
        $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2 /DNDEBUG /EHsc>

        # Clang-cl
        $<$<AND:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>,$<CONFIG:Debug>>:/EHsc /Zi /W4>
        $<$<AND:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>,$<CONFIG:Release>>:/EHsc /O2 /DNDEBUG>
    )
endfunction()

add_ui_test_executable(ui_unit_tests
    test_BatchManager.cpp
    test_FallbackRenderer.cpp
    test_ImageManager.cpp
    test_TextUtils.cpp
    test_ResourceProvider.cpp
    test_Log.cpp
    test_Logger.cpp
    test_result.cpp
    test_UiRuntime.cpp
    test_SystemManager.cpp
    test_ApplicationLifecycle.cpp
    test_DeviceClaimState.cpp
    test_GPUTextureOwner.cpp
    test_GpuInitializationTransaction.cpp
)
configure_ui_test_target(ui_unit_tests)
target_link_libraries(ui_unit_tests PRIVATE freetype harfbuzz)

add_ui_test_executable(ui_ecs_tests
    test_Anchors.cpp
    test_MainWindow.cpp
    test_TweenSystem.cpp
    test_AnimationLifecycle.cpp
    test_TaskChain.cpp
    test_RenderDirty.cpp
    test_Utils.cpp
    test_Visibility.cpp
    test_Hierarchy.cpp
    test_DragDrop.cpp
    test_ThemeSystem_Button.cpp
    test_ThemeSystem_Input.cpp
    test_ThemeSystem_DropDown.cpp
    test_ThemeSystem_Window.cpp
    test_FocusNavigation.cpp
    test_OverlaySystem.cpp
    test_Tooltip.cpp
    test_SwitchRadio.cpp
    test_TabView.cpp
    test_ContextMenuModal.cpp
    test_ListView.cpp
)
configure_ui_test_target(ui_ecs_tests)
target_link_libraries(ui_ecs_tests PRIVATE yogacore)

add_ui_test_executable(ui_api_tests
    test_UmbrellaHeader.cpp
    test_PublicLeafHeaders.cpp
    test_PublicEventApi.cpp
)
configure_ui_test_target(ui_api_tests)

add_ui_test_executable(ui_fallback_lifecycle_tests
    test_FallbackWindowLifecycle.cpp
)
configure_ui_test_target(ui_fallback_lifecycle_tests)

# 观测型并发压测（P0-4）：仅作为 lost-wakeup 复现/观测工具，不加入默认 ui_tests 聚合目标，
# 由开发者显式运行（例如 ctest -L stress）。严守「无实测不改无锁路径」门槛。
# P1-4：core::EventLoop 双模式帧调度（FIXED_RATE/EVENT_DRIVEN + 锁帧接口）也在此目标内验证。
add_ui_test_executable(ui_eventloop_stress_tests
    test_EventLoopStress.cpp
    test_CoreEventLoopScheduling.cpp
)
configure_ui_test_target(ui_eventloop_stress_tests)

# P1-7：真实 SDL_PushEvent → event watch → EVENT_DRIVEN → InteractionSystem 集成矩阵。
# 独立目标隔离 SDL 全局事件队列，并与其他 video 测试串行执行。
add_ui_test_executable(ui_sdl_event_driven_tests
    test_SdlEventDrivenIntegration.cpp
)
configure_ui_test_target(ui_sdl_event_driven_tests)

# Backward-compatible aggregate target name for existing workflows.
add_custom_target(ui_tests DEPENDS
    ui_unit_tests
    ui_ecs_tests
    ui_api_tests
    ui_fallback_lifecycle_tests
    ui_sdl_event_driven_tests
)

add_library(ui_public_header_check OBJECT
    ${CMAKE_SOURCE_DIR}/tests/support/PublicUmbrellaHeaderCheck.cpp
)
target_compile_features(ui_public_header_check PRIVATE cxx_std_23)
target_include_directories(ui_public_header_check PRIVATE ${CMAKE_SOURCE_DIR}/include)
target_compile_options(ui_public_header_check PRIVATE
    # clang++ 直接接收 Clang 警告选项；/clang: 是 clang-cl 专用的转发语法。
    $<$<AND:$<CXX_COMPILER_ID:Clang>,$<NOT:$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>>:-Wno-pragma-once-outside-header>
    $<$<AND:$<CXX_COMPILER_ID:Clang>,$<CXX_COMPILER_FRONTEND_VARIANT:MSVC>>:/clang:-Wno-pragma-once-outside-header>
)
add_dependencies(ui_api_tests ui_public_header_check)

add_library(ui_public_math_types_header_check OBJECT
    ${CMAKE_SOURCE_DIR}/tests/support/PublicMathTypesHeaderCheck.cpp
)
target_compile_features(ui_public_math_types_header_check PRIVATE cxx_std_23)
target_include_directories(ui_public_math_types_header_check PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_dependencies(ui_api_tests ui_public_math_types_header_check)

add_library(ui_public_utils_header_check OBJECT
    ${CMAKE_SOURCE_DIR}/tests/support/PublicUtilsHeaderCheck.cpp
)
target_compile_features(ui_public_utils_header_check PRIVATE cxx_std_23)
target_include_directories(ui_public_utils_header_check PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_dependencies(ui_api_tests ui_public_utils_header_check)

add_library(ui_public_application_header_check OBJECT
    ${CMAKE_SOURCE_DIR}/tests/support/PublicApplicationHeaderCheck.cpp
)
target_compile_features(ui_public_application_header_check PRIVATE cxx_std_23)
target_include_directories(ui_public_application_header_check PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_library(ui_public_factory_header_check OBJECT
    ${CMAKE_SOURCE_DIR}/tests/support/PublicFactoryHeaderCheck.cpp
)
target_compile_features(ui_public_factory_header_check PRIVATE cxx_std_23)
target_include_directories(ui_public_factory_header_check PRIVATE ${CMAKE_SOURCE_DIR}/include)

add_library(ui_public_factory_windows_macro_check OBJECT
    ${CMAKE_SOURCE_DIR}/tests/support/PublicFactoryWindowsMacroCheck.cpp
)
target_compile_features(ui_public_factory_windows_macro_check PRIVATE cxx_std_23)
target_include_directories(ui_public_factory_windows_macro_check PRIVATE ${CMAKE_SOURCE_DIR}/include)
add_dependencies(ui_api_tests
    ui_public_application_header_check
    ui_public_factory_header_check
    ui_public_factory_windows_macro_check
)

include(GoogleTest)
gtest_discover_tests(ui_unit_tests
    DISCOVERY_TIMEOUT 60
    PROPERTIES
    LABELS "unit"
    RUN_SERIAL TRUE
    RESOURCE_LOCK ui_sdl_video
    TIMEOUT 900
)
gtest_discover_tests(ui_ecs_tests
    DISCOVERY_TIMEOUT 60
    PROPERTIES LABELS "ecs"
)
gtest_discover_tests(ui_api_tests
    DISCOVERY_TIMEOUT 60
    PROPERTIES LABELS "api"
)
gtest_discover_tests(ui_fallback_lifecycle_tests
    DISCOVERY_TIMEOUT 60
    PROPERTIES
    LABELS "integration;sdl;lifecycle"
    ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
    RUN_SERIAL TRUE
    RESOURCE_LOCK ui_sdl_video
    TIMEOUT 60
)
gtest_discover_tests(ui_eventloop_stress_tests
    DISCOVERY_TIMEOUT 60
    PROPERTIES
    LABELS "stress;concurrency"
    RUN_SERIAL TRUE
    TIMEOUT 300
)
gtest_discover_tests(ui_sdl_event_driven_tests
    DISCOVERY_TIMEOUT 60
    PROPERTIES
    LABELS "event-driven"
    ENVIRONMENT "SDL_VIDEODRIVER=offscreen"
    RUN_SERIAL TRUE
    RESOURCE_LOCK ui_sdl_video
    TIMEOUT 60
)
