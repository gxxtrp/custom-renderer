add_executable(m1_window
    src/main.cpp
)

target_link_libraries(m1_window
    PRIVATE
        engine::core
        engine::platform
        tests::common
)

set_target_properties(m1_window PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

engine_set_compiler_warnings(m1_window)
