#pragma once

// libs
#include <vulkan/vulkan_core.h>

namespace m1
{
	class Device;   // Forward declaration

	class Sampler
	{
	public:
		explicit Sampler(const Device& device, const VkSamplerCreateInfo* createInfo = nullptr);
		~Sampler();

		// Non-copyable
		// Non-copyable, non-movable
		Sampler(const Sampler&) = delete;
		Sampler& operator=(const Sampler&) = delete;
		Sampler(Sampler&&) = delete;
		Sampler& operator=(Sampler&&) = delete;

		[[nodiscard]] VkSampler getVkSampler() const { return _vkSampler; }

	private:
		const Device& _device;
		VkSampler _vkSampler = VK_NULL_HANDLE;

		static VkSamplerCreateInfo getDefaultCreateInfo();
	};
} // namespace m1
