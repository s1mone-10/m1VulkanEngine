#include "SwapChain.hpp"
#include "Device.hpp"
#include "Window.hpp"
#include <stdexcept>
#include <algorithm>
#include <limits>
#include <iostream>

namespace va
{
    SwapChain::SwapChain(const Device& device, const Window& window, VkSwapchainKHR oldSwapChain) : _device(device)
    {
        createSwapChain(device, window, oldSwapChain);
        createImageViews(device);
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
        std::cout << "SwapChain destroyed" << std::endl;
    }

    void SwapChain::createSwapChain(const Device& device, const Window& window, VkSwapchainKHR oldSwapChain = VK_NULL_HANDLE)
    {
        SwapChainProperties swapChainProperties = device.getSwapChainProperties();

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
        createInfo.surface = device.getSurface(); // tie to the surface
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

        QueueFamilyIndices indices = device.getQueueFamilyIndices();
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
        if (vkCreateSwapchainKHR(device.getVkDevice(), &createInfo, nullptr, &_vkSwapChain) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create swap chain!");
        }

        // getVkSwapChain images from the swap chain
        vkGetSwapchainImagesKHR(device.getVkDevice(), _vkSwapChain, &imageCount, nullptr);
        _images.resize(imageCount);
        vkGetSwapchainImagesKHR(device.getVkDevice(), _vkSwapChain, &imageCount, _images.data());
    }

    void SwapChain::createImageViews(const Device& device)
    {
        _imageViews.resize(_images.size());

        for (size_t i = 0; i < _images.size(); i++)
        {
			// ImageView Info
            VkImageViewCreateInfo viewInfo{};
            {
                // type and format
                viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                viewInfo.image = _images[i]; // bind the image to the view
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
                viewInfo.format = _imageFormat;

				// swizzle color components
                viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
                viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
                
                //image purpose is and which part should be accessed
                viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                viewInfo.subresourceRange.baseMipLevel = 0;
                viewInfo.subresourceRange.levelCount = 1;
                viewInfo.subresourceRange.baseArrayLayer = 0;
                viewInfo.subresourceRange.layerCount = 1;
            }

			// create the ImageView
            if (vkCreateImageView(device.getVkDevice(), &viewInfo, nullptr, &_imageViews[i]) != VK_SUCCESS)
            {
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
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = _imageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // no multisampling
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // operation to perform on the attachment before rendering
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // operation to perform on the attachment after rendering
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // image to be presented in the swap chain

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        // A single render pass can consist of multiple subpasses. For example a sequence of post-processing effects
        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        //The index of the attachment in this array is directly referenced from the fragment shader with the layout(location = 0) out vec4 outColor directive!
        subpass.pColorAttachments = &colorAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        // render pass info
        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
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

        _framebuffers.resize(getImageCount());

        for (size_t i = 0; i < _framebuffers.size(); i++)
        {
            VkImageView attachments[] = { _imageViews[i] };

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = _renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments; // objects that should be bound to the respective renderPass pAttachment array
            framebufferInfo.width = _extent.width;
            framebufferInfo.height = _extent.height;
            framebufferInfo.layers = 1; // as in the swap chain

            if (vkCreateFramebuffer(_device.getVkDevice(), &framebufferInfo, nullptr, &_framebuffers[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create framebuffer!");
            }
        }
    }

} // namespace va
