#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <string>

#include "Image.hpp"

namespace m1
{
    class Device;   // Forward declaration

    class Texture
    {
    public:
        Texture(const Device& device, uint32_t width, uint32_t height);
        ~Texture();

        // Non-copyable, non-movable
        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;
        Texture(Texture&&) = delete;
        Texture& operator=(Texture&&) = delete;

        VkImage getImage() const { return _image->getVkImage(); }
        VkImageView getImageView() const { return _image->getVkImageView(); }
        VkSampler getSampler() const { return _sampler; }

    private:
        void createTextureImage(uint32_t width, uint32_t height);
        void createSampler();

        const Device& _device;
        std::unique_ptr<Image> _image;
        VkSampler _sampler;
    };
    
} // namespace m1
