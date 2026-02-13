#pragma once

#include <vulkan/vulkan.h>

#include <vector>
#include <memory>

#include "Image.hpp"

namespace m1
{
    class Device; // Forward declaration
    class Window;
	class Image;

	struct SwapChainConfig
	{
		VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
		VkSwapchainKHR oldSwapChain = VK_NULL_HANDLE;
	};

    class SwapChain
    {
    public:
        SwapChain(const Device& device, const Window& window, const SwapChainConfig& config);
        ~SwapChain();

        // Non-copyable, non-movable
        SwapChain(const SwapChain&) = delete;
        SwapChain& operator=(const SwapChain&) = delete;
        SwapChain(SwapChain&&) = delete;
        SwapChain& operator=(SwapChain&&) = delete;

        VkSwapchainKHR getVkSwapChain() const { return _vkSwapChain; }
        VkFormat getSwapChainImageFormat() const { return _swapChainImageFormat; }
    	Image& getColorImage() const { return *_colorImage; }
    	Image& getMsaaColorImage() const { return *_msaaColorImage; }
    	Image& getDepthImage() const { return *_depthImage; }
        VkExtent2D getExtent() const { return _extent; }
        float getAspectRatio() const { return _extent.width / static_cast<float>(_extent.height); }
        VkImage getSwapChainImage(uint32_t index) const { return _swapChainImages[index]; }
        VkImageView getSwapChainImageView(uint32_t index) const { return _swapChainImageViews[index]; }
        size_t getImageCount() const { return _swapChainImages.size(); }
		VkSampleCountFlagBits getSamples() const { return _samples; }

    private:
        void createSwapChain(const Window& window, VkSwapchainKHR oldSpawChain);
        void createImages();
        void createColorImage();
        void createMsaaImage();
        void createDepthImage();
        bool hasStencilComponent(VkFormat format);

        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, const Window& window);
        
        VkSwapchainKHR _vkSwapChain = VK_NULL_HANDLE;
        VkFormat _swapChainImageFormat;
        VkExtent2D _extent;
        VkSampleCountFlagBits _samples;

        std::vector<VkImage> _swapChainImages;
        std::vector<VkImageView> _swapChainImageViews;
        
        std::unique_ptr<Image> _colorImage, _msaaColorImage, _depthImage;

        const Device& _device;
    };
}
