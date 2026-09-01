engine_add_module(
    NAME core
    NAMESPACE engine
    PUBLIC_DEPS
        Tracy::TracyClient
    PRIVATE_DEPS
        spdlog::spdlog
)
