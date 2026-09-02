add_executable(m6_5_task_graph
    src/main.cpp
)

target_link_libraries(m6_5_task_graph
    PRIVATE
        engine::core
        engine::platform
        tests::common
)

set_target_properties(m6_5_task_graph PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

engine_set_compiler_warnings(m6_5_task_graph)
