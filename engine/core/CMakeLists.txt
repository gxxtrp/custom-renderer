engine_add_module(
    NAME core
    NAMESPACE engine
    PUBLIC_DEPS
        spdlog::spdlog
        Tracy::TracyClient
)
