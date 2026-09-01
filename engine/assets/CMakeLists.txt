engine_add_module(
    NAME assets
    PUBLIC_DEPS
        engine::core
        engine::rhi
        meshoptimizer::meshoptimizer
        fastgltf::fastgltf
)
