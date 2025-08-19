#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace va
{
    class Instance
    {
    public:
        Instance();
        ~Instance();

        // Non-copyable, non-movable
        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;
        Instance(Instance&&) = delete;
        Instance& operator=(Instance&&) = delete;

        VkInstance get() const { return _vkInstance; }

        const std::vector<const char*> validationLayers = { "VK_LAYER_KHRONOS_validation" }; // standard diagnostics layers provided by the Vulkan SDK

        // enable validationLayers only at debug time
        static const bool enableValidationLayers =
#ifdef NDEBUG
            false;
#else
            true;
#endif

    private:
        void createInstance();
        void setupDebugMessenger();

        bool checkValidationLayerSupport();
        std::vector<const char*> getRequiredExtensions();
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

        VkInstance _vkInstance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT _debugMessenger = VK_NULL_HANDLE;
    };
}
