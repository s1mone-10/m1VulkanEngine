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
		explicit DescriptorSetManager(const Device& device);
		~DescriptorSetManager();

		// Non-copyable, non-movable
		DescriptorSetManager(const DescriptorSetManager&) = delete;
		DescriptorSetManager& operator=(const DescriptorSetManager&) = delete;
		DescriptorSetManager(DescriptorSetManager&&) = delete;
		DescriptorSetManager& operator=(DescriptorSetManager&&) = delete;

		[[nodiscard]] std::vector<VkDescriptorSet> allocateFrameDescriptorSets(uint32_t count) const;
		[[nodiscard]] std::vector<VkDescriptorSet> allocateEquirectToCubemapFrameDescriptorSets(uint32_t count) const;
		[[nodiscard]] std::vector<VkDescriptorSet> allocateSkyBoxDescriptorSets(uint32_t count) const;
		[[nodiscard]] std::vector<VkDescriptorSet> allocateMaterialDescriptorSets(uint32_t count, VkDescriptorSetLayout layout) const;

		[[nodiscard]] VkDescriptorSetLayout getFrameDescriptorSetLayout() const { return _descriptorSetLayout; }
		[[nodiscard]] VkDescriptorSetLayout getMaterialDescriptorSetLayout() const { return _materialDescriptorSetLayout; }
		[[nodiscard]] VkDescriptorSetLayout getMaterialPbrDescriptorSetLayout() const {	return _materialPbrDescriptorSetLayout;	}
		[[nodiscard]] VkDescriptorSetLayout getEquirectToCubemapFrameDescriptorSetLayout() const { return _equirectToCubemapDescriptorSetLayout; }
		[[nodiscard]] VkDescriptorSetLayout getSkyBoxDescriptorSetLayout() const { return _skyBoxDescriptorSetLayout; }

	private:
		const Device& _device;
		VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout _materialDescriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout _materialPbrDescriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout _equirectToCubemapDescriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout _skyBoxDescriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorPool _descriptorPool;

		void createFrameDescriptorSetLayout();
		void createMaterialDescriptorSetLayout();
		void createMaterialPbrDescriptorSetLayout();
		void createEquirectToCubemapDescriptorSetLayout();
		void createSkyBoxDescriptorSetLayout();
		void createDescriptorPool();
	};
}


