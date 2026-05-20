# UI module tests

add_executable(ui_tests
    test_MainWindow.cpp
    test_TweenSystem.cpp
    test_BatchManager.cpp
    test_TextUtils.cpp
    test_ResourceProviderCoverage.cpp
    test_TaskChain.cpp
    test_UiRuntime.cpp
    test_UtilsCoverage.cpp
    test_VisibilityCoverage.cpp
    test_HierarchyCoverage.cpp
    test_result.cpp
)
target_compile_features(ui_tests PRIVATE cxx_std_23)

target_include_directories(ui_tests PRIVATE
    ${CMAKE_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/src
)
target_compile_definitions(ui_tests PRIVATE
    ASIO_STANDALONE         # 告诉 Asio 不使用 Boost
    SDL_MAIN_HANDLED       # 防止 SDL 篡改 main 入口

)
target_link_libraries(ui_tests PRIVATE
    EnTT::EnTT
    asio::asio
    SDL3::SDL3
    ui
    GTest::gmock
    GTest::gmock_main  # 如果你自己没写 main 函数，用这个
    GTest::gtest_main
)
target_compile_options(ui_tests PRIVATE
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

include(GoogleTest)
gtest_discover_tests(ui_tests)
