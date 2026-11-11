add_executable(test_structure test_structure.cpp)
target_link_libraries(test_structure PRIVATE moltui_core)
add_test(NAME test_structure COMMAND test_structure)

add_executable(test_rasterizer test_rasterizer.cpp)
target_link_libraries(test_rasterizer PRIVATE moltui_core)
add_test(NAME test_rasterizer COMMAND test_rasterizer)

add_executable(test_loader test_loader.cpp)
target_link_libraries(test_loader PRIVATE moltui_core)
add_test(NAME test_loader COMMAND test_loader)
set_tests_properties(test_loader PROPERTIES WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}")
