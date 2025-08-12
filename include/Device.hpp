#pragma once

#include <vulkan/vulkan.h>
#include <optional>
#include <vector>
#include "Window.hpp"

namespace va {

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
    };

    class Instance; // Forward declaration

    class Device {
    public:
        Device(const Instance& instance, const Window& window);
        ~Device();

        // Non-copyable, non-movable
        Device(const Device&) = delete;
        Device& operator=(const Device&) = delete;
        Device(Device&&) = delete;
        Device& operator=(Device&&) = delete;

        VkDevice get() const { return _device; }
        VkPhysicalDevice getPhysicalDevice() const { return _physicalDevice; }
        QueueFamilyIndices getQueueFamilyIndices() const { return _qfIndices; }
        VkQueue getGraphicsQueue() const { return _graphicsQueue; }
        VkQueue getPresentQueue() const { return _presentQueue; }
        VkSurfaceKHR getSurface() const { return _surface; }


    private:
        void createSurface(const Window& window);
        void pickPhysicalDevice();
        void createLogicalDevice();

        bool isDeviceSuitable(VkPhysicalDevice device);
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

        const Instance& _instance;
        VkDevice _device = VK_NULL_HANDLE;
        VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
        VkQueue _graphicsQueue = VK_NULL_HANDLE;
        VkQueue _presentQueue = VK_NULL_HANDLE;
        VkSurfaceKHR _surface = VK_NULL_HANDLE;
        QueueFamilyIndices _qfIndices;

        const std::vector<const char*> _deviceExtensions = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    };
}
