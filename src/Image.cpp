#include "Image.hpp"
#include "Buffer.hpp"
#include "Device.hpp"
#include "log/Log.hpp"

#include <stdexcept>

namespace m1
{
    m1::Image::Image(const Device& device, uint32_t width, uint32_t height, uint32_t mipLevels,
                 VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties, VkImageAspectFlags aspectFlags)
		: _device(device), _format(format), _width(width), _height(height), _mipLevels(mipLevels)
    {
        Log::Get().Info("Creating image from scratch");

        // Image info
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.extent.width = width;
        imageInfo.extent.height = height;
        imageInfo.extent.depth = 1;
        imageInfo.mipLevels = mipLevels;
        imageInfo.arrayLayers = 1;
        imageInfo.format = format;
        imageInfo.tiling = tiling;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED; // only two possible options: UNDEFINED or PREINITIALIZED
        imageInfo.usage = usage;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT; // no multisampling
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // not shared between multiple queue families

        // Create the Image
        if (vkCreateImage(_device.getVkDevice(), &imageInfo, nullptr, &_vkImage) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create image!");
        }

        // Allocate and bind device memory for the image
        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(_device.getVkDevice(), _vkImage, &memRequirements);
        _deviceMemory = _device.allocateMemory(memRequirements, properties);
        vkBindImageMemory(_device.getVkDevice(), _vkImage, _deviceMemory, 0);

		// ImageView info
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = _vkImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.aspectMask = aspectFlags;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = mipLevels;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

		// Create the Image View
        if (vkCreateImageView(_device.getVkDevice(), &viewInfo, nullptr, &_imageView) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to create image view!");
        }
    }
    
    Image::~Image()
    {
        Log::Get().Info("Destroying image");
        vkDestroyImageView(_device.getVkDevice(), _imageView, nullptr);
        vkDestroyImage(_device.getVkDevice(), _vkImage, nullptr);
        vkFreeMemory(_device.getVkDevice(), _deviceMemory, nullptr);
    }
} // namespace m1
