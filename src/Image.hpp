#pragma once

#include <vulkan/vulkan.h>

namespace m1
{
    class Device;   // Forward declaration

    class Image
    {
    public:
        Image(const Device& device, uint32_t width, uint32_t height, uint32_t mipLevels, VkFormat format, VkImageTiling tiling, VkImageUsageFlags usage, VkMemoryPropertyFlags properties);
        ~Image();

        // Non-copyable, non-movable
        Image(const Image&) = delete;
        Image& operator=(const Image&) = delete;
        Image(Image&&) = delete;
        Image& operator=(Image&&) = delete;

        VkImage getVkImage() const { return _vkImage; }
        VkImageView getVkImageView() const { return _imageView; }
		VkFormat getFormat() const { return _format; }
		uint32_t getWidth() const { return _width; }
		uint32_t getHeight() const { return _height; }
		uint32_t getMipLevels() const { return _mipLevels; }

    private:
        const Device& _device;
        VkImage _vkImage = VK_NULL_HANDLE;
        VkDeviceMemory _deviceMemory = VK_NULL_HANDLE;
        VkImageView _imageView = VK_NULL_HANDLE;
		VkFormat _format;
		uint32_t _width;
		uint32_t _height;
        uint32_t _mipLevels;
    };

} // namespace m1
