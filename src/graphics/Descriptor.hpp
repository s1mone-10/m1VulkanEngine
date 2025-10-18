#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace m1
{
	class Device; // Forward declaration
	class Buffer;
	class Texture;

	class Descriptor
	{
	public:
		Descriptor(const Device& device);
		~Descriptor();

		// Non-copyable, non-movable
		Descriptor(const Descriptor&) = delete;
		Descriptor& operator=(const Descriptor&) = delete;
		Descriptor(Descriptor&&) = delete;
		Descriptor& operator=(Descriptor&&) = delete;

		void updateDescriptorSets(const std
		                          ::vector<std::unique_ptr<Buffer>> &objectUboBuffers, const std::vector<std::unique_ptr<Buffer>>& frameUboBuffers, const Texture& texture, const Buffer& lightsUbo);
		VkDescriptorSetLayout getDescriptorSetLayout() { return _descriptorSetLayout; }
		VkDescriptorSet getDescriptorSet(uint32_t index) { return _descriptorSets[index]; }

	private:
		const Device& _device;
		VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorPool _descriptorPool;
		std::vector<VkDescriptorSet> _descriptorSets;

		void createDescriptorSetLayout();
		void createDescriptorPool();
		void createDescriptorSets();
	};
}


