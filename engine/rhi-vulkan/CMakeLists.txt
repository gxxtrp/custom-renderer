find_package(Vulkan REQUIRED)

engine_add_module(
    NAME rhi-vulkan
    NAMESPACE engine
    PUBLIC_DEPS
        engine::core
        engine::rhi
    PRIVATE_DEPS
        Vulkan::Vulkan
        Tracy::TracyClient
        sdl::sdl
    PRIVATE_INCLUDES
        ${Vulkan_INCLUDE_DIRS}
        ${Vulkan_INCLUDE_DIRS}/vma
)

add_library(engine::rhi_vulkan ALIAS engine-rhi-vulkan)
