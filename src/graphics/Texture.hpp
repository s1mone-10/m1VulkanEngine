#pragma once

#include <vulkan/vulkan.h>
#include <memory>

#include "Image.hpp"

namespace m1
{
    class Device;   // Forward declaration
	class Sampler;

	struct TextureParams
	{
		VkExtent2D extent;
		VkFormat format;
		const VkSamplerCreateInfo* samplerCreateInfo = nullptr; // optional, if not provided, a default sampler will be created
	};

    class Texture
    {
    public:
        Texture(const Device& device, const TextureParams& params);
        Texture(const Device& device, std::shared_ptr<Image> image, std::shared_ptr<Sampler> sampler) :
    		_device(device), _image(std::move(image)), _sampler(std::move(sampler)) {}

        ~Texture() = default; // nothing to destroy, is just a container of Image and Sampler

        // Non-copyable
        Texture(const Texture&) = delete;
        Texture& operator=(const Texture&) = delete;
    	Texture(Texture&&) = delete;
    	Texture& operator=(Texture&&) = delete;

        [[nodiscard]] Image& getImage() const { return *_image; }
        [[nodiscard]] Sampler& getSampler() const { return *_sampler; }
    	[[nodiscard]] VkExtent2D getExtent() const { return _image->getExtent();}
        [[nodiscard]] uint32_t getWidth() const { return _image->getWidth(); }
        [[nodiscard]] uint32_t getHeight() const { return _image->getHeight(); }
    	[[nodiscard]] VkDescriptorImageInfo getVkDescriptorImageInfo() const;

    private:
        void createTextureImage(const TextureParams& textureParams);

        const Device& _device;
        std::shared_ptr<Image> _image;
        std::shared_ptr<Sampler> _sampler;
    };
    
} // namespace m1
