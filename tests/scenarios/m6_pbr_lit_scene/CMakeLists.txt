add_executable(m6_pbr_lit_scene
    src/main.cpp
)

target_link_libraries(m6_pbr_lit_scene
    PRIVATE
        engine::core
        engine::ecs
        engine::scene
        engine::platform
        engine::assets
        engine::rhi
        engine::rhi-vulkan
        engine::renderer
        tests::common
        fastgltf::fastgltf
        meshoptimizer::meshoptimizer
)

engine_target_shaders(m6_pbr_lit_scene
    SHADERS
        "shaders/culling/cull_reset.slang" compute computeMain "cull_reset.comp.spv"
        "shaders/culling/instance_cull.slang" compute computeMain "instance_cull.comp.spv"
        "shaders/culling/meshlet_cull.slang" compute computeMain "meshlet_cull.comp.spv"
        "shaders/visibility/meshlet_vis_gpu_scene.slang" task taskMain "meshlet_vis_gpu_scene.task.spv"
        "shaders/visibility/meshlet_vis_gpu_scene.slang" mesh meshMain "meshlet_vis_gpu_scene.mesh.spv"
        "shaders/visibility/meshlet_vis_gpu_scene.slang" fragment fragMain "meshlet_vis_gpu_scene.frag.spv"
        "shaders/shading/pbr_deferred.slang" mesh fullscreenMesh "pbr_deferred.mesh.spv"
        "shaders/shading/pbr_deferred.slang" fragment fragMain "pbr_deferred.frag.spv"
)

set_target_properties(m6_pbr_lit_scene PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

engine_set_compiler_warnings(m6_pbr_lit_scene)
