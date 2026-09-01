add_executable(m2_clear_screen
    src/main.cpp
)

target_link_libraries(m2_clear_screen
    PRIVATE
        engine::core
        engine::platform
        engine::rhi
        engine::rhi-vulkan
        tests::common
)

set_target_properties(m2_clear_screen PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

engine_set_compiler_warnings(m2_clear_screen)
