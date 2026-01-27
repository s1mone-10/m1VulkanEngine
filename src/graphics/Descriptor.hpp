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

		std::vector<VkDescriptorSet> allocateFrameDescriptorSets(uint32_t count) const;
		std::vector<VkDescriptorSet> allocateMaterialDescriptorSets(uint32_t count) const;
		VkDescriptorSetLayout getFrameDescriptorSetLayout() const { return _descriptorSetLayout; }
		VkDescriptorSetLayout getMaterialDescriptorSetLayout() const { return _materialDescriptorSetLayout; }

	private:
		const Device& _device;
		VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout _materialDescriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorPool _descriptorPool;

		void createFrameDescriptorSetLayout();
		void createMaterialDescriptorSetLayout();
		void createDescriptorPool();
	};
}


