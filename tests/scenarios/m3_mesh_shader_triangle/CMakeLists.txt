add_executable(m3_mesh_shader_triangle
    src/main.cpp
)

target_link_libraries(m3_mesh_shader_triangle
    PRIVATE
        engine::core
        engine::platform
        engine::rhi
        engine::rhi-vulkan
        engine::renderer
        tests::common
)

engine_target_shaders(m3_mesh_shader_triangle
    SHADERS
        "shaders/visibility/mesh_triangle.slang" task taskMain "mesh_triangle.task.spv"
        "shaders/visibility/mesh_triangle.slang" mesh meshMain "mesh_triangle.mesh.spv"
        "shaders/visibility/mesh_triangle.slang" fragment fragMain "mesh_triangle.frag.spv"
)

set_target_properties(m3_mesh_shader_triangle PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

engine_set_compiler_warnings(m3_mesh_shader_triangle)
