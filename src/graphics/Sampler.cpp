#include "Sampler.hpp"
#include "Utils.hpp"
#include "log/Log.hpp"

namespace m1
{
	Sampler::Sampler(const Device& device, const VkSamplerCreateInfo* createInfo) : _device(device)
	{
		Log::Get().Trace("Creating sampler");

		if (createInfo == nullptr)
		{
			auto ci = getDefaultCreateInfo();
			createInfo = &ci;
		};

		// Create sampler
		VK_CHECK(vkCreateSampler(device.getVkDevice(), createInfo, nullptr, &_vkSampler));
	}

	Sampler::~Sampler()
	{
		Log::Get().Trace("Destroying sampler");
		vkDestroySampler(_device.getVkDevice(), _vkSampler, nullptr);
	}

	VkSamplerCreateInfo Sampler::getDefaultCreateInfo()
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

		return samplerInfo;
	}
} // namespace m1
