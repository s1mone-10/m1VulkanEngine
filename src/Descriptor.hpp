#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace m1
{
	class Device; // Forward declaration
	class Buffer;

	class Descritor
	{
	public:
		Descritor(const Device& device);
		~Descritor();

		// Non-copyable, non-movable
		Descritor(const Descritor&) = delete;
		Descritor& operator=(const Descritor&) = delete;
		Descritor(Descritor&&) = delete;
		Descritor& operator=(Descritor&&) = delete;

		void updateDescriotorSets(const std::vector<std::unique_ptr<Buffer>>& buffers);
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


