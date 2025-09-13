#pragma once

#include <vulkan/vulkan.h>
#include <optional>
#include <vector>
#include <memory>
#include "Window.hpp"
#include "Instance.hpp"

namespace m1
{
	class Queue; // Forward declaration

    struct QueueFamilyIndices
    {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
    };

    struct SwapChainProperties
    {
        VkSurfaceCapabilitiesKHR capabilities;
        std::vector<VkSurfaceFormatKHR> formats;
        std::vector<VkPresentModeKHR> presentModes;
    };

    class Device
    {
    public:
        Device(const Window& window);
        ~Device();

        // Non-copyable, non-movable
        Device(const Device&) = delete;
        Device& operator=(const Device&) = delete;
        Device(Device&&) = delete;
        Device& operator=(Device&&) = delete;

        VkDevice getVkDevice() const { return _vkDevice; }
        QueueFamilyIndices getQueueFamilyIndices() const { return _queueFamilies; }
        const Queue& getGraphicsQueue() const { return *_graphicsQueue; }
        const Queue& getPresentQueue() const { return *_presentQueue; }
        VkSurfaceKHR getSurface() const { return _surface; }
        SwapChainProperties getSwapChainProperties() const { return getSwapChainProperties(_physicalDevice); };
        VkDeviceMemory allocateMemory(VkMemoryRequirements memRequirements, VkMemoryPropertyFlags properties) const;
        VkFormat findSupportedFormat(const std::vector<VkFormat>& candidates, VkImageTiling tiling, VkFormatFeatureFlags features) const;

    private:
        void createSurface(const Window& window);
        void pickPhysicalDevice();
        void createLogicalDevice();

        bool isDeviceSuitable(VkPhysicalDevice device);
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);
        SwapChainProperties getSwapChainProperties(const VkPhysicalDevice device) const;
        uint32_t findMemoryTypeIndex(uint32_t typeFilter, VkMemoryPropertyFlags properties) const;
        
        Instance _instance;
        VkSurfaceKHR _surface = VK_NULL_HANDLE;
        VkPhysicalDevice _physicalDevice = VK_NULL_HANDLE;
        VkDevice _vkDevice = VK_NULL_HANDLE;
        std::unique_ptr<Queue> _graphicsQueue;
        std::unique_ptr<Queue> _presentQueue;
        QueueFamilyIndices _queueFamilies;

        const std::vector<const char*> _requiredExtensions = { 
            VK_KHR_SWAPCHAIN_EXTENSION_NAME // Not all graphics cards are capable of presenting images
        }; 
    };
}
