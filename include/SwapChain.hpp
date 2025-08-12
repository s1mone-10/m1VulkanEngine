#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace va {
    class Device; // Forward declaration
    class Window;

    class SwapChain {
    public:
        struct SwapChainSupportDetails {
            VkSurfaceCapabilitiesKHR capabilities;
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> presentModes;
        };

        SwapChain(const Device& device, const Window& window);
        ~SwapChain();

        // Non-copyable, non-movable
        SwapChain(const SwapChain&) = delete;
        SwapChain& operator=(const SwapChain&) = delete;
        SwapChain(SwapChain&&) = delete;
        SwapChain& operator=(SwapChain&&) = delete;

        VkSwapchainKHR get() const { return _swapChain; }
        VkFormat getImageFormat() const { return _swapChainImageFormat; }
        VkExtent2D getExtent() const { return _swapChainExtent; }
        const std::vector<VkImageView>& getImageViews() const { return _swapChainImageViews; }
        size_t getImageCount() const { return _swapChainImages.size(); }

        static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface);

    private:
        void createSwapChain(const Device& device, const Window& window);
        void createImageViews(const Device& device);

        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, const Window& window);

        VkSwapchainKHR _swapChain = VK_NULL_HANDLE;
        VkFormat _swapChainImageFormat;
        VkExtent2D _swapChainExtent;
        std::vector<VkImage> _swapChainImages;
        std::vector<VkImageView> _swapChainImageViews;

        const Device& _device;
    };
}
