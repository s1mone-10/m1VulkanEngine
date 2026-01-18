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
        VkFormat getImageFormat() const { return _imageFormat; }
        VkExtent2D getExtent() const { return _extent; }
        float getAspectRatio() const { return _extent.width / (float) _extent.height; }
        const std::vector<VkImageView>& getImageViews() const { return _imageViews; }
        size_t getImageCount() const { return _images.size(); }
		VkRenderPass getRenderPass() const { return _renderPass; }
        VkFramebuffer getFrameBuffer(int index) { return _framebuffers[index]; }
		VkSampleCountFlagBits getSamples() const { return _samples; }

    private:
        void createSwapChain(const Window& window, VkSwapchainKHR oldSpawChain);
        void createImages();
        void createRenderPass();
        void createFramebuffers();
        void createColorImage();
        void createDepthImage();
        bool hasStencilComponent(VkFormat format);

        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, const Window& window);
        
        VkSwapchainKHR _vkSwapChain = VK_NULL_HANDLE;
        VkFormat _imageFormat;
        VkExtent2D _extent;
        VkSampleCountFlagBits _samples;

        std::vector<VkImage> _images;
        std::vector<VkImageView> _imageViews;
        std::vector<VkFramebuffer> _framebuffers;
        
        std::unique_ptr<Image> _colorImage; // for msaa only
        std::unique_ptr<Image> _depthImage;
        
        VkRenderPass _renderPass = VK_NULL_HANDLE;

        const Device& _device;
    };
}
