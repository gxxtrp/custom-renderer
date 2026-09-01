#include "vulkan_instance.hpp"

#include <cstring>
#include <utility>

#include <SDL3/SDL_vulkan.h>

namespace engine::rhi_vulkan {

namespace {

VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
                                             VkDebugUtilsMessageTypeFlagsEXT /*messageTypes*/,
                                             const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                                             void* /*pUserData*/) {
    if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        ENGINE_LOG_ERROR("[Vulkan Validation] {}", pCallbackData->pMessage);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        ENGINE_LOG_WARN("[Vulkan Validation] {}", pCallbackData->pMessage);
    } else if (messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        ENGINE_LOG_INFO("[Vulkan Validation] {}", pCallbackData->pMessage);
    } else {
        ENGINE_LOG_TRACE("[Vulkan Validation] {}", pCallbackData->pMessage);
    }
    return VK_FALSE;
}

bool checkValidationLayerSupport(const char* layerName) {
    core::u32 layerCount = 0;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    for (const auto& layerProperties : availableLayers) {
        if (std::strcmp(layerName, layerProperties.layerName) == 0) {
            return true;
        }
    }
    return false;
}

}  // namespace

std::expected<VulkanInstance, std::string> VulkanInstance::create(const VulkanInstanceDesc& desc) {
    ENGINE_LOG_INFO("Initializing Vulkan 1.3 Instance...");

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Custom Engine";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "Custom Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.apiVersion = VK_API_VERSION_1_3;

    std::vector<const char*> enabledExtensions;
    std::vector<const char*> enabledLayers;

    // Query extensions from SDL3
    core::u32 sdlExtensionCount = 0;
    char const* const* sdlExtensions = SDL_Vulkan_GetInstanceExtensions(&sdlExtensionCount);
    if (sdlExtensions != nullptr && sdlExtensionCount > 0) {
        for (core::u32 i = 0; i < sdlExtensionCount; ++i) {
            enabledExtensions.push_back(sdlExtensions[i]);
        }
    } else {
        // Fallback default surface extensions on Windows
        enabledExtensions.push_back("VK_KHR_surface");
        enabledExtensions.push_back("VK_KHR_win32_surface");
    }

    constexpr const char* validationLayer = "VK_LAYER_KHRONOS_validation";
    bool validationActive = false;

    if (desc.enableValidation) {
        if (checkValidationLayerSupport(validationLayer)) {
            enabledLayers.push_back(validationLayer);
            enabledExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            validationActive = true;
            ENGINE_LOG_INFO("Vulkan validation layer enabled ({})", validationLayer);
        } else {
            ENGINE_LOG_WARN("Validation layer {} requested but not available on this system", validationLayer);
        }
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<core::u32>(enabledExtensions.size());
    createInfo.ppEnabledExtensionNames = enabledExtensions.data();
    createInfo.enabledLayerCount = static_cast<core::u32>(enabledLayers.size());
    createInfo.ppEnabledLayerNames = enabledLayers.data();

    VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};
    if (validationActive) {
        debugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugCreateInfo.pfnUserCallback = debugCallback;
        createInfo.pNext = &debugCreateInfo;
    }

    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    if (result != VK_SUCCESS) {
        std::string err = std::string("Failed to create Vulkan instance: ") + std::string(vkResultToString(result));
        ENGINE_LOG_ERROR("{}", err);
        return std::unexpected(std::move(err));
    }

    VkDebugUtilsMessengerEXT debugMessenger = VK_NULL_HANDLE;
    if (validationActive) {
        auto createDebugUtilsMessenger = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(instance, "vkCreateDebugUtilsMessengerEXT"));
        if (createDebugUtilsMessenger != nullptr) {
            result = createDebugUtilsMessenger(instance, &debugCreateInfo, nullptr, &debugMessenger);
            if (result != VK_SUCCESS) {
                ENGINE_LOG_WARN("Failed to create Vulkan debug utils messenger: {}", vkResultToString(result));
            } else {
                ENGINE_LOG_INFO("Vulkan debug messenger successfully attached");
            }
        }
    }

    ENGINE_LOG_INFO("Vulkan instance created successfully (API 1.3)");
    return VulkanInstance(instance, debugMessenger, validationActive);
}

VulkanInstance::VulkanInstance(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, bool validationEnabled)
    : m_instance(instance), m_debugMessenger(debugMessenger), m_validationEnabled(validationEnabled) {}

VulkanInstance::~VulkanInstance() {
    if (m_debugMessenger != VK_NULL_HANDLE) {
        auto destroyDebugUtilsMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
            vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
        if (destroyDebugUtilsMessenger != nullptr) {
            destroyDebugUtilsMessenger(m_instance, m_debugMessenger, nullptr);
        }
        m_debugMessenger = VK_NULL_HANDLE;
    }

    if (m_instance != VK_NULL_HANDLE) {
        vkDestroyInstance(m_instance, nullptr);
        m_instance = VK_NULL_HANDLE;
        ENGINE_LOG_INFO("Vulkan instance destroyed cleanly");
    }
}

VulkanInstance::VulkanInstance(VulkanInstance&& other) noexcept
    : m_instance(std::exchange(other.m_instance, VK_NULL_HANDLE)),
      m_debugMessenger(std::exchange(other.m_debugMessenger, VK_NULL_HANDLE)),
      m_validationEnabled(std::exchange(other.m_validationEnabled, false)) {}

VulkanInstance& VulkanInstance::operator=(VulkanInstance&& other) noexcept {
    if (this != &other) {
        if (m_debugMessenger != VK_NULL_HANDLE) {
            auto destroyDebugUtilsMessenger = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
                vkGetInstanceProcAddr(m_instance, "vkDestroyDebugUtilsMessengerEXT"));
            if (destroyDebugUtilsMessenger != nullptr) {
                destroyDebugUtilsMessenger(m_instance, m_debugMessenger, nullptr);
            }
        }
        if (m_instance != VK_NULL_HANDLE) {
            vkDestroyInstance(m_instance, nullptr);
        }
        m_instance = std::exchange(other.m_instance, VK_NULL_HANDLE);
        m_debugMessenger = std::exchange(other.m_debugMessenger, VK_NULL_HANDLE);
        m_validationEnabled = std::exchange(other.m_validationEnabled, false);
    }
    return *this;
}

}  // namespace engine::rhi_vulkan
