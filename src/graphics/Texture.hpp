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

        // Non-copyable
        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;

        Image& getImage() const { return *_image; }
        VkSampler getSampler() const { return _sampler; }
        uint32_t getWidth() const { return _image->getWidth(); }
        uint32_t getHeight() const { return _image->getHeight(); }

    private:
        void createTextureImage(uint32_t width, uint32_t height);
        void createSampler();

        const Device& _device;
        std::unique_ptr<Image> _image;
        VkSampler _sampler;
    };
    
} // namespace m1
