#pragma once

#include <vulkan/vulkan.h>
#include <memory>
#include <string>

#include "Image.hpp"

namespace m1
{
    class Device;   // Forward declaration

	struct TextureParams
	{
		VkExtent2D extent;
		VkFormat format;
		bool mipmaps;
	};

    class Texture
    {
    public:
        Texture(const Device& device, const TextureParams& params);
        Texture(const Device& device, std::shared_ptr<Image> image, VkSampler sampler) :
    		_device(device), _image(std::move(image)), _sampler(sampler) {}

        ~Texture();

        // Non-copyable
        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;

        Image& getImage() const { return *_image; }
        VkSampler getSampler() const { return _sampler; }
    	VkExtent2D getExtent() const { return _image->getExtent();}
        uint32_t getWidth() const { return _image->getWidth(); }
        uint32_t getHeight() const { return _image->getHeight(); }

    private:
        void createTextureImage(const TextureParams& textureParams);
        void createSampler();

        const Device& _device;
        std::shared_ptr<Image> _image;
        VkSampler _sampler;
    };
    
} // namespace m1
