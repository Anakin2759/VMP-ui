add_executable(ui_sample_demo
	main.cpp
)

target_compile_features(ui_sample_demo PRIVATE cxx_std_23)
target_link_libraries(ui_sample_demo PRIVATE ui)
target_compile_definitions(ui_sample_demo PRIVATE SDL_MAIN_HANDLED)
