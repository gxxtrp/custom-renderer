engine_add_module(
    NAME scene
    NAMESPACE engine
    PUBLIC_DEPS
        engine::core
        engine::ecs
        engine::assets
        engine::rhi
)
