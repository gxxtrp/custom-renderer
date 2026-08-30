cmake_minimum_required(VERSION 3.25)

project(CustomEngine
    VERSION 0.1.0
    DESCRIPTION "High-performance GPU-driven real-time 3D rendering engine"
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Add custom CMake module directory
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(EngineModule)
include(CompilerWarnings)

# Global Options
option(ENGINE_PROFILING_ENABLE "Enable Tracy profiling" OFF)
option(ENGINE_ENABLE_ASAN "Enable AddressSanitizer" OFF)

if(ENGINE_PROFILING_ENABLE)
    add_compile_definitions(ENGINE_PROFILING_ENABLE TRACY_ENABLE)
endif()

if(ENGINE_ENABLE_ASAN)
    if(MSVC)
        add_compile_options(/fsanitize=address)
        add_compile_definitions(
            _DISABLE_CONTAINER_ANNOTATION=1
            _DISABLE_STRING_ANNOTATION=1
            _DISABLE_VECTOR_ANNOTATION=1
            _DISABLE_OPTIONAL_ANNOTATION=1
            _DISABLE_DEQUE_ANNOTATION=1
            _DISABLE_LIST_ANNOTATION=1
            _DISABLE_TREE_ANNOTATION=1
            _DISABLE_MAP_ANNOTATION=1
            _DISABLE_UNORDERED_MAP_ANNOTATION=1
        )
        get_filename_component(CLANG_DIR "${CMAKE_CXX_COMPILER}" DIRECTORY)
        file(GLOB ASAN_LIB_DIRS "${CLANG_DIR}/../lib/clang/*/lib/windows")
        if(ASAN_LIB_DIRS)
            list(GET ASAN_LIB_DIRS 0 ASAN_LIB_DIR)
            link_directories("${ASAN_LIB_DIR}")
            link_libraries(clang_rt.asan_dynamic-x86_64.lib clang_rt.asan_dynamic_runtime_thunk-x86_64.lib)
        endif()
    else()
        add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
        add_link_options(-fsanitize=address)
    endif()
endif()

# Find dependencies from Conan
find_package(SDL3 REQUIRED)
find_package(spdlog REQUIRED)
find_package(Tracy REQUIRED)

# Subdirectories
add_subdirectory(engine)
add_subdirectory(tests)
