cmake_minimum_required(VERSION 3.24)
project(moltui VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Find packages provided by Pixi / Conda-Forge
find_package(gemmi REQUIRED)
find_package(ftxui REQUIRED)
find_package(glm REQUIRED)
find_package(CLI11 REQUIRED)
find_package(Threads REQUIRED)

# moltui core library
add_library(moltui_core
    src/core/elements.cpp
    src/core/structure.cpp
    src/core/bonding.cpp
    src/core/pbc_unwrapper.cpp
    src/io/loader.cpp
    src/render/camera.cpp
    src/render/framebuffer.cpp
    src/render/rasterizer.cpp
    src/render/protocol.cpp
    src/ui/app.cpp
    src/ui/viewport.cpp
)

target_include_directories(moltui_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(moltui_core PUBLIC
    gemmi::gemmi_cpp
    gemmi::headers
    ftxui::screen
    ftxui::dom
    ftxui::component
    glm::glm
    CLI11::CLI11
    Threads::Threads
)

target_compile_options(moltui_core PRIVATE
    -Wall
    -Wextra
    -Wpedantic
    -O3
)

# CLI Binary
add_executable(moltui src/main.cpp)
target_link_libraries(moltui PRIVATE moltui_core)

# Tests
enable_testing()
add_subdirectory(tests)
