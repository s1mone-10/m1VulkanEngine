#pragma once

// libs
#include "vk_mem_alloc.h"

namespace m1
{
    class Device;   // Forward declaration

    struct ImageParams
    {
        VkExtent2D extent;
        VkFormat format;
        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
        VkImageUsageFlags usage;
        VmaAllocationCreateFlags memoryProps = 0;
        VkImageAspectFlags aspectFlags = VK_IMAGE_ASPECT_COLOR_BIT;
        uint32_t mipLevels = 1;
		VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    };

    class Image
    {
    public:
        Image(const Device& device, const ImageParams& params);
        ~Image();

        // Non-copyable, non-movable
        Image(const Image&) = delete;
        Image& operator=(const Image&) = delete;
        Image(Image&&) = delete;
        Image& operator=(Image&&) = delete;

        VkImage getVkImage() const { return _vkImage; }
        VkImageView getVkImageView() const { return _imageView; }
		VkFormat getFormat() const { return _format; }
		uint32_t getWidth() const { return _extent.width; }
		uint32_t getHeight() const { return _extent.height; }
		uint32_t getMipLevels() const { return _mipLevels; }

    private:
        const Device& _device;
        VkImage _vkImage = VK_NULL_HANDLE;
    	VmaAllocation _allocation;
        VkImageView _imageView = VK_NULL_HANDLE;
		VkFormat _format;
        VkExtent2D _extent;
        uint32_t _mipLevels;
    };

} // namespace m1
