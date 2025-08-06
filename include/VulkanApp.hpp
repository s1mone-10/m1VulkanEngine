#pragma once

#include <vulkan/vulkan.h>

#include "Window.hpp"

namespace va
{
    class VulkanApp
    {
        const uint32_t  WIDTH = 800;
        const uint32_t  HEIGHT = 600;
        Window window{WIDTH, HEIGHT, "Vulkan App"};

    public:
        void run();

    private:
        VkInstance instance;

        void initVulkan();
        void mainLoop();
        void cleanup();
        void createInstance();
    };
}