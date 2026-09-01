engine_add_module(
    NAME platform
    NAMESPACE engine
    PUBLIC_DEPS
        engine::core
    PRIVATE_DEPS
        sdl::sdl
)
