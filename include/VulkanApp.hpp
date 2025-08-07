#pragma once

#include <vulkan/vulkan.h>
#include <optional>

#include "Window.hpp"

namespace va
{
struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        // more next

        bool isComplete(){
            return graphicsFamily.has_value();
        }
    };

    class VulkanApp
    {
        const uint32_t  WIDTH = 800;
        const uint32_t  HEIGHT = 600;
        Window window{WIDTH, HEIGHT, "Vulkan App"};

    public:
        void run();

    private:
        VkInstance instance;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE; // implicitly destroyed when the VkInstance is destroyed

        void initVulkan();
        void mainLoop();
        void cleanup();
        void createInstance();
        void pickPhysicalDevice();
        bool isDeviceSuitable(VkPhysicalDevice device);
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
    };
}