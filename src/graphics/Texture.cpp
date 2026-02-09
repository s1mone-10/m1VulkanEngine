#include "Texture.hpp"

#include <stdexcept>

#include "Buffer.hpp"
#include "Device.hpp"
#include "Utils.hpp"
#include "log/Log.hpp"

namespace m1
{
    Texture::Texture(const Device& device, uint32_t width, uint32_t height)
        : _device(device)
    {
        createTextureImage(width, height);
        createSampler();
    }

    Texture::~Texture()
    {
        Log::Get().Info("Destroying texture");
        vkDestroySampler(_device.getVkDevice(), _sampler, nullptr);
    }

    void Texture::createTextureImage(uint32_t width, uint32_t height)
    {
        auto mipLevels = static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;

        ImageParams params
        {
            .extent = {width, height},
            .format = VK_FORMAT_R8G8B8A8_SRGB,
            .usage = VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, // source (for creating mipmaps) and destination of transfer and shader read
            .mipLevels = mipLevels,
        };

        _image = std::make_unique<Image>(_device, params);
    }

    void Texture::createSampler()
    {
        // Sampler Info
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_TRUE;
        samplerInfo.maxAnisotropy = 8.0f; // TODO query the device for maxSamplerAnisotropy 
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK; // for clamp to border address mode
        samplerInfo.unnormalizedCoordinates = VK_FALSE; // unnormalized => range [0, widt). Normalized => range [0,1)
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;

        // mipmapping
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = VK_LOD_CLAMP_NONE; // all available mipmap levels will be sampled

        // Create sampler
        VK_CHECK(vkCreateSampler(_device.getVkDevice(), &samplerInfo, nullptr, &_sampler));
    }
    
} // namespace m1
