engine_add_module(
    NAME renderer
    NAMESPACE engine
    PUBLIC_DEPS
        engine::core
        engine::rhi
        engine::scene
        engine::assets
)
