cmake_minimum_required(VERSION 3.24)
project(moltui VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(GNUInstallDirs)

option(MOLTUI_FETCH_DEPS "Fetch missing dependencies (gemmi, ftxui, glm, cli11) via FetchContent" OFF)
option(MOLTUI_BUILD_TESTS "Build MolTUI unit tests" ON)

if(MOLTUI_FETCH_DEPS)
    include(FetchContent)
    set(_moltui_saved_build_testing "${BUILD_TESTING}")
    set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

    find_package(gemmi QUIET)
    if(NOT gemmi_FOUND)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        set(USE_PYTHON OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            gemmi
            GIT_REPOSITORY https://github.com/project-gemmi/gemmi.git
            GIT_TAG v0.7.5
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(gemmi)
        if(TARGET gemmi_cpp AND NOT TARGET gemmi::gemmi_cpp)
            add_library(gemmi::gemmi_cpp ALIAS gemmi_cpp)
        endif()
        if(TARGET headers AND NOT TARGET gemmi::headers)
            add_library(gemmi::headers ALIAS headers)
        endif()
    endif()

    find_package(ftxui QUIET)
    if(NOT ftxui_FOUND)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
        set(FTXUI_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(FTXUI_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(FTXUI_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(FTXUI_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            ftxui
            GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI.git
            GIT_TAG v7.0.3
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(ftxui)
    endif()

    find_package(glm QUIET)
    if(NOT glm_FOUND)
        set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            glm
            GIT_REPOSITORY https://github.com/g-truc/glm.git
            GIT_TAG 1.0.3
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(glm)
    endif()

    find_package(CLI11 QUIET)
    if(NOT CLI11_FOUND)
        set(CLI11_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(CLI11_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(CLI11_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        FetchContent_Declare(
            CLI11
            GIT_REPOSITORY https://github.com/CLIUtils/CLI11.git
            GIT_TAG v2.7.2
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(CLI11)
    endif()
    set(BUILD_TESTING "${_moltui_saved_build_testing}" CACHE BOOL "" FORCE)
else()
    # Find packages provided by Pixi / Conda-Forge / Homebrew / system
    find_package(gemmi REQUIRED)
    find_package(ftxui REQUIRED)
    find_package(glm REQUIRED)
    find_package(CLI11 REQUIRED)
endif()

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
    src/render/view_utils.cpp
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
target_compile_definitions(moltui PRIVATE MOLTUI_VERSION="${PROJECT_VERSION}")

# Installation
install(TARGETS moltui RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES LICENSE README.md DESTINATION ${CMAKE_INSTALL_DOCDIR})

# Tests
if(MOLTUI_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
