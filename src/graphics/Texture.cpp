#include "Texture.hpp"

#include <stdexcept>

#include "Buffer.hpp"
#include "Device.hpp"
#include "Sampler.hpp"
#include "Utils.hpp"
#include "Log.hpp"

namespace m1
{
    Texture::Texture(const Device& device, const TextureParams& params)
        : _device(device)
    {
        createTextureImage(params);
    	_sampler = std::make_shared<Sampler>(_device, params.samplerCreateInfo);
    }

    VkDescriptorImageInfo Texture::getVkDescriptorImageInfo() const
    {
	    return{
		    .sampler     = _sampler->getVkSampler(),
		    .imageView   = _image->getVkImageView(),
		    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
	    };
    }

    void Texture::createTextureImage(const TextureParams& textureParams)
    {
    	uint32_t mipLevels = computeMipLevels(textureParams.extent.width, textureParams.extent.height);

        ImageParams imageParams
        {
            .extent = textureParams.extent,
            .format = textureParams.format,
            .usage = getTextureImageUsageFlags(),
            .mipLevels = mipLevels,
        };

        _image = std::make_unique<Image>(_device, imageParams);
    }
} // namespace m1
