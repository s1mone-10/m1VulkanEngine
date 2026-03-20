#pragma once

// libs
#include <array>

#include "vk_mem_alloc.h"

namespace m1
{
    class Device;   // Forward declaration

    struct ImageParams
    {
        VkExtent2D extent;
        VkFormat format;
    	VkImageAspectFlags flags = 0;
        VkImageTiling tiling = VK_IMAGE_TILING_OPTIMAL;
        VkImageUsageFlags usage;
        VkImageAspectFlags aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        uint32_t mipLevels = 1;
    	uint32_t arrayLayers = 1;
		VkSampleCountFlagBits samples = VK_SAMPLE_COUNT_1_BIT;
    	VmaAllocationCreateFlags memoryProps = 0;
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

        [[nodiscard]] VkImage getVkImage() const { return _vkImage; }
        [[nodiscard]] VkImageView getVkImageView() const { return _imageView; }
        [[nodiscard]] VkImageView getLayerVkImageView(uint32_t layer) const { return _cubeMapViews[layer]; }
		[[nodiscard]] VkFormat getFormat() const { return _format; }
		[[nodiscard]] VkExtent2D getExtent() const { return _extent; }
		[[nodiscard]] uint32_t getWidth() const { return _extent.width; }
		[[nodiscard]] uint32_t getHeight() const { return _extent.height; }
		[[nodiscard]] uint32_t getMipLevels() const { return _mipLevels; }
		[[nodiscard]] uint32_t getArrayLayers() const { return _arrayLayers; }

    private:
        const Device& _device;
        VkImage _vkImage = VK_NULL_HANDLE;
    	VmaAllocation _allocation = VK_NULL_HANDLE;
        VkImageView _imageView = VK_NULL_HANDLE;
    	std::array<VkImageView, 6> _cubeMapViews {VK_NULL_HANDLE};
		VkFormat _format;
        VkExtent2D _extent;
        uint32_t _mipLevels;
    	uint32_t _arrayLayers;
    };

} // namespace m1
