add_executable(m4_vis_buffer_mesh
    src/main.cpp
)

target_link_libraries(m4_vis_buffer_mesh
    PRIVATE
        engine::core
        engine::platform
        engine::assets
        engine::rhi
        engine::rhi-vulkan
        engine::renderer
        tests::common
        fastgltf::fastgltf
        meshoptimizer::meshoptimizer
)

engine_target_shaders(m4_vis_buffer_mesh
    SHADERS
        "shaders/visibility/meshlet_vis.slang" task taskMain "meshlet_vis.task.spv"
        "shaders/visibility/meshlet_vis.slang" mesh meshMain "meshlet_vis.mesh.spv"
        "shaders/visibility/meshlet_vis.slang" fragment fragMain "meshlet_vis.frag.spv"
        "shaders/shading/deferred_texturing.slang" mesh fullscreenMesh "deferred_texturing.mesh.spv"
        "shaders/shading/deferred_texturing.slang" fragment fragMain "deferred_texturing.frag.spv"
)

set_target_properties(m4_vis_buffer_mesh PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

engine_set_compiler_warnings(m4_vis_buffer_mesh)
