#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace va {
    class Window; // Forward declaration

    class Instance {
    public:
        Instance(const Window& window);
        ~Instance();

        // Non-copyable, non-movable
        Instance(const Instance&) = delete;
        Instance& operator=(const Instance&) = delete;
        Instance(Instance&&) = delete;
        Instance& operator=(Instance&&) = delete;

        VkInstance get() const { return _instance; }

        static const std::vector<const char*> validationLayers;
        static bool areValidationLayersEnabled();

    private:
        void createInstance(const Window& window);
        void setupDebugMessenger();

        bool checkValidationLayerSupport();
        std::vector<const char*> getRequiredExtensions(const Window& window);
        void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);

        VkInstance _instance = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT _debugMessenger = VK_NULL_HANDLE;
    };
}
