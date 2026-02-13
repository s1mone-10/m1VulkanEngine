#include "SwapChain.hpp"
#include "Device.hpp"
#include "Window.hpp"
#include "Utils.hpp"
#include "log/Log.hpp"

#include <stdexcept>
#include <algorithm>
#include <limits>
#include <iostream>
#include <array>

namespace m1
{
	SwapChain::SwapChain(const Device& device, const Window& window, const SwapChainConfig& config) : _device(device), _samples(config.samples)
    {
        Log::Get().Info("Creating swap chain");
        createSwapChain(window, config.oldSwapChain);
        createImages();

		createColorImage();
		createDepthImage();
		if (_samples > VK_SAMPLE_COUNT_1_BIT)
			createMsaaImage();
    }

    SwapChain::~SwapChain()
    {
        for (auto imageView : _swapChainImageViews)
            vkDestroyImageView(_device.getVkDevice(), imageView, nullptr);

        // _images are automatically cleaned up once the swap chain has been destroyed
        vkDestroySwapchainKHR(_device.getVkDevice(), _vkSwapChain, nullptr);
        Log::Get().Info("SwapChain destroyed");
    }

    void SwapChain::createSwapChain(const Window& window, VkSwapchainKHR oldSwapChain = VK_NULL_HANDLE)
    {
        Log::Get().Info("Creating swap chain implementation");
        SwapChainProperties swapChainProperties = _device.getSwapChainProperties();

        // Format, present mode, extent
        VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainProperties.formats); // rgb format
        _swapChainImageFormat = surfaceFormat.format;
        VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainProperties.presentModes);
        _extent = chooseSwapExtent(swapChainProperties.capabilities, window); // resolution of the swap chain images

        // recommended to request at least one more image than the minimum
        // to avoid waitin for the driver to complete internal operations before we can acquire another image to render to
        uint32_t imageCount = swapChainProperties.capabilities.minImageCount + 1;

		// if the maximum number of images is specified, we must not exceed it
        if (imageCount > swapChainProperties.capabilities.maxImageCount &&
            swapChainProperties.capabilities.maxImageCount > 0) // 0 is a special value that means that there is no maximum
        {
            imageCount = swapChainProperties.capabilities.maxImageCount; 
        }

        // SwapChain Info
        VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = _device.getSurface(); // tie to the surface
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = _extent;
        createInfo.imageArrayLayers = 1; // This is always 1 unless you are developing a stereoscopic 3D application
        createInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT // rendering is done on a color image, then copied to the swap chain image
									| VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // TODO: is that right? validation layer error if I don't set also this
		createInfo.preTransform = swapChainProperties.capabilities.currentTransform; // to specify no transformation, simply set the current transformation
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR; // no blending with other windows
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE; // don't care about pixels that are not visible to the user, for example because another window is in front of them
        createInfo.oldSwapchain = oldSwapChain; // existing non-retired swapchain currently associated with surface. May aid in the resource reuse.

        QueueFamilyIndices indices = _device.getQueueFamilyIndices();
        uint32_t queueFamilyIndices[] = { indices.graphicsFamily.value(), indices.presentFamily.value() };

		// specify how the images will be shared between queues family (generally graphics and present family are the same)
        if (indices.graphicsFamily != indices.presentFamily)
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
            createInfo.queueFamilyIndexCount = 2;
            createInfo.pQueueFamilyIndices = queueFamilyIndices;
        }
        else
        {
            createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE; // An image is owned by one queue family at a time. Best performance
            createInfo.queueFamilyIndexCount = 0; // Optional
            createInfo.pQueueFamilyIndices = nullptr; // Optional
        }

        // create SwapChain
        VK_CHECK(vkCreateSwapchainKHR(_device.getVkDevice(), &createInfo, nullptr, &_vkSwapChain));
    }

    void SwapChain::createImages()
    {
        // get images from the swap chain
        uint32_t imageCount;
        vkGetSwapchainImagesKHR(_device.getVkDevice(), _vkSwapChain, &imageCount, nullptr);
        _swapChainImages.assign(imageCount, VK_NULL_HANDLE);
        vkGetSwapchainImagesKHR(_device.getVkDevice(), _vkSwapChain, &imageCount, _swapChainImages.data());

        // creates images view
        _swapChainImageViews.assign(imageCount, VK_NULL_HANDLE);

        for (size_t i = 0; i < imageCount; i++)
        {
			// ImageView Info
            VkImageViewCreateInfo viewInfo{};
            {
                // image, type and format
                viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewInfo.image = _swapChainImages[i]; // bind the image to the view
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewInfo.format = _swapChainImageFormat;

				// swizzle color components
                viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
                viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
                
                //image purpose and which part should be accessed
                viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                viewInfo.subresourceRange.baseMipLevel = 0;
                viewInfo.subresourceRange.levelCount = 1;
                viewInfo.subresourceRange.baseArrayLayer = 0;
                viewInfo.subresourceRange.layerCount = 1;
            }

			// create the ImageView
            VK_CHECK(vkCreateImageView(_device.getVkDevice(), &viewInfo, nullptr, &_swapChainImageViews[i]));
        }
    }

    VkSurfaceFormatKHR SwapChain::chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats)
    {
        for (const auto& availableFormat : availableFormats)
        {
            if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB && availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
            {
                return availableFormat;
            }
        }
        // eventually add logic to choose a different format if the preferred one is not available
        return availableFormats[0];
    }

    VkPresentModeKHR SwapChain::chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes)
    {
        // https://vulkan-tutorial.com/en/Drawing_a_triangle/Presentation/Swap_chain
        // it represents the actual conditions for showing images to the screen
        for (const auto& availablePresentMode : availablePresentModes)
        {
            if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR)
            {
                return availablePresentMode;
            }
        }
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    VkExtent2D SwapChain::chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, const Window& window)
    {
        if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max())
        {
			return capabilities.currentExtent; // equals to the window size
        }
        else
        {
            int width, height;
            window.getFramebufferSize(&width, &height);

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }
    }

    void SwapChain::createColorImage()
    {
        ImageParams params
        {
            .extent = _extent,
            .format = _swapChainImageFormat,
            .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | // copied on the swap chain image
            	VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // rendering target
        	.memoryProps = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, // dedicated allocation for special, big resources, like fullscreen images used as attachments
			.samples = VK_SAMPLE_COUNT_1_BIT,
        };

		_colorImage = std::make_unique<Image>(_device, params);
    }

	void SwapChain::createMsaaImage()
	{
		ImageParams params
		{
			.extent = _extent,
			.format = _swapChainImageFormat,
			.usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |  //
				VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT, // rendering target
			.memoryProps = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, // dedicated allocation for special, big resources, like fullscreen images used as attachments
			.samples = _samples,
		};

		_msaaColorImage = std::make_unique<Image>(_device, params);
	}

    void SwapChain::createDepthImage()
    {
        // find a format that supports specified tiling and flags
        VkFormat depthFormat = _device.findSupportedFormat(
            { VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
            VK_IMAGE_TILING_OPTIMAL,
            VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT
        );

        ImageParams params
        {
            .extent = _extent,
            .format = depthFormat,
            .usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
        	.memoryProps = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, // dedicated allocation for special, big resources, like fullscreen images used as attachments
            .aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT,
			.samples = _samples,
        };

        // create the depth image
        _depthImage = std::make_unique<Image>(_device, params);
    }

    bool SwapChain::hasStencilComponent(VkFormat format)
    {
        // S8 -> 8 bit component for stencil
        return format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT;
    }

} // namespace m1
