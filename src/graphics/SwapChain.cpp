#include "SwapChain.hpp"
#include "Device.hpp"
#include "Window.hpp"
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
		if (_samples != VK_SAMPLE_COUNT_1_BIT)
		    createColorImage();
		createDepthImage();
        createRenderPass();
		createFramebuffers();
    }

    SwapChain::~SwapChain()
    {
        for (auto framebuffer : _framebuffers)
            vkDestroyFramebuffer(_device.getVkDevice(), framebuffer, nullptr);

        vkDestroyRenderPass(_device.getVkDevice(), _renderPass, nullptr);

        for (auto imageView : _imageViews)
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
        _imageFormat = surfaceFormat.format;
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
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT; // rendering direct on the images
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
        if (vkCreateSwapchainKHR(_device.getVkDevice(), &createInfo, nullptr, &_vkSwapChain) != VK_SUCCESS)
        {
            Log::Get().Error("failed to create swap chain!");
            throw std::runtime_error("failed to create swap chain!");
        }
    }

    void SwapChain::createImages()
    {
        // get images from the swap chain
        uint32_t imageCount;
        vkGetSwapchainImagesKHR(_device.getVkDevice(), _vkSwapChain, &imageCount, nullptr);
        _images.assign(imageCount, VK_NULL_HANDLE);
        vkGetSwapchainImagesKHR(_device.getVkDevice(), _vkSwapChain, &imageCount, _images.data());

        // creates images view
        _imageViews.assign(imageCount, VK_NULL_HANDLE);

        for (size_t i = 0; i < imageCount; i++)
        {
			// ImageView Info
            VkImageViewCreateInfo viewInfo{};
            {
                // image, type and format
                viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewInfo.image = _images[i]; // bind the image to the view
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewInfo.format = _imageFormat;

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
            if (vkCreateImageView(_device.getVkDevice(), &viewInfo, nullptr, &_imageViews[i]) != VK_SUCCESS)
            {
                Log::Get().Error("failed to create image views!");
                throw std::runtime_error("failed to create image views!");
            }
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

    /// <summary>
    /// Set information about the framebuffer attachments (images linked to a framebuffer where rendering outputs go)
    /// </summary>
    void SwapChain::createRenderPass()
    {
        /* Method overview
        - define the attachments that will be used during rendering (color, depth, stencil, etc.)
        - define the subpasses that will be used in the render pass
        - define any dependencies between the subpasses and external operations
        - create the render pass object
        */


        /* Layouts overview
        - An image layout describes how the GPU should treat the memory of an image (layout of the pixels in memory).
        - Images need to be transitioned to specific layouts that are suitable for the operation that they're going to be involved in next.
        - RenderPass and Subpasses automatically take care of image layout transitions.
        - attachment.initialLayout => layout before the render pass begins
        - attachment.finalLayout => layout to automatically transition to when the render pass ends
        - attachmentRef.layout => layout during the subpass
        */

		bool msaaEnabled = (_samples != VK_SAMPLE_COUNT_1_BIT);

        // attachments array
        std::vector<VkAttachmentDescription> attachments;

        // color attachment
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = _imageFormat;
        colorAttachment.samples = _samples;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // operation to perform on the attachment before rendering
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // operation to perform on the attachment after rendering
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = msaaEnabled
            ? VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL // MSAA -> needs resolve
            : VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // no MSAA -> render directly to swapchain
        attachments.push_back(colorAttachment);

        // color attachment reference
        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0; // index in the attachmentsArray
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // depth attachment
        VkAttachmentDescription depthAttachment{};
        depthAttachment.format = _depthImage->getFormat();
        depthAttachment.samples = _samples;
        depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
        attachments.push_back(depthAttachment);

        // depth attachment reference
        VkAttachmentReference depthAttachmentRef{};
        depthAttachmentRef.attachment = 1;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        // resolve attachment (only if MSAA > 1)
        VkAttachmentReference resolveAttachmentRef{};
        if (msaaEnabled)
        {
            VkAttachmentDescription colorResolveAttachment{};
            colorResolveAttachment.format = _imageFormat;
            colorResolveAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
            colorResolveAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorResolveAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            colorResolveAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            colorResolveAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            colorResolveAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            colorResolveAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // image to be presented in the swap chain
            attachments.push_back(colorResolveAttachment);

            resolveAttachmentRef.attachment = 2;
            resolveAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        }
        
        // A single render pass can consist of multiple subpasses. For example, a sequence of post-processing effects
        // Every subpass can reference one or more of the attachments

        // define subpass
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        //The index of the attachment in this array is directly referenced from the fragment shader with the layout(location = 0) out vec4 outColor directive!
        subpass.pColorAttachments = &colorAttachmentRef;
        subpass.pResolveAttachments = msaaEnabled ? &resolveAttachmentRef : nullptr;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
        
        // define subpass dependencies: to handle/synchronize image layout transitions
        VkSubpassDependency dependency
        {
            // indices of source and destination subpasses
            .srcSubpass = VK_SUBPASS_EXTERNAL, // subpass before or after the render pass depending on whether it's used as a source or destination
            .dstSubpass = 0, // index of the subpass in the render pass

            // stages in which these access operations occur
            .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,

            // memory access operations to wait on
            .srcAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        };

        // render pass info
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
        renderPassInfo.pAttachments = attachments.data();
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        // create the render pass
        if (vkCreateRenderPass(_device.getVkDevice(), &renderPassInfo, nullptr, &_renderPass) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create render pass!");
        }
    }

    void SwapChain::createFramebuffers()
    {
        // The attachments specified during render pass creation are bound by wrapping them into a VkFramebuffer object. 
        // A framebuffer object references all of the VkImageView objects that represent the attachments
        _framebuffers.assign(getImageCount(), VK_NULL_HANDLE);

        for (size_t i = 0; i < _framebuffers.size(); i++)
        {
            // objects that should be bound to the respective renderPass pAttachment array
            std::vector<VkImageView> attachments;
            if(_samples == VK_SAMPLE_COUNT_1_BIT)
                attachments = { _imageViews[i], _depthImage->getVkImageView()}; // no MSAA -> render directly into swap chain image
			else
                attachments = { _colorImage->getVkImageView(), _depthImage->getVkImageView(), _imageViews[i]}; // MSAA -> render into multisampled image first

			// Framebuffer info
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = _renderPass;
            framebufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = _extent.width;
            framebufferInfo.height = _extent.height;
            framebufferInfo.layers = 1; // as in the swap chain

			// create Framebuffer
            if (vkCreateFramebuffer(_device.getVkDevice(), &framebufferInfo, nullptr, &_framebuffers[i]) != VK_SUCCESS)
            {
                Log::Get().Error("failed to create framebuffer!");
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

    void SwapChain::createColorImage()
    {
        ImageParams params
        {
            .extent = _extent,
            .format = _imageFormat,
            .usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        	.memoryProps = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, // dedicated allocation for special, big resources, like fullscreen images used as attachments
			.samples = _samples,
        };

		_colorImage = std::make_unique<Image>(_device, params);
    }

    void SwapChain::createDepthImage()
    {
        // find format that support specified tiling and flags
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
