#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <memory>

namespace m1
{
	class Device; // Forward declaration
	class Buffer;
	class Texture;

	class DescriptorSetManager
	{
	public:
		DescriptorSetManager(const Device& device);
		~DescriptorSetManager();

		// Non-copyable, non-movable
		DescriptorSetManager(const DescriptorSetManager&) = delete;
		DescriptorSetManager& operator=(const DescriptorSetManager&) = delete;
		DescriptorSetManager(DescriptorSetManager&&) = delete;
		DescriptorSetManager& operator=(DescriptorSetManager&&) = delete;

		[[nodiscard]] std::vector<VkDescriptorSet> allocateFrameDescriptorSets(uint32_t count) const;
		[[nodiscard]] std::vector<VkDescriptorSet> allocateMaterialDescriptorSets(uint32_t count) const;
		[[nodiscard]] VkDescriptorSetLayout getFrameDescriptorSetLayout() const { return _descriptorSetLayout; }
		[[nodiscard]] VkDescriptorSetLayout getMaterialDescriptorSetLayout() const { return _materialDescriptorSetLayout; }
		[[nodiscard]] VkDescriptorSetLayout getMaterialPbrDescriptorSetLayout() const {	return _materialPbrDescriptorSetLayout;	}

	private:
		const Device& _device;
		VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout _materialDescriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout _materialPbrDescriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorPool _descriptorPool;

		void createFrameDescriptorSetLayout();
		void createMaterialDescriptorSetLayout();
		void createMaterialPbrDescriptorSetLayout();
		void createDescriptorPool();
	};
}


