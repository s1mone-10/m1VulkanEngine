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

		void updateDescriptorSets(const std::vector<std::unique_ptr<Buffer>> &objectUboBuffers, const std::vector<std::unique_ptr<Buffer>> &
		                          frameUboBuffers, const Buffer &lightsUbo, const std::vector<std::unique_ptr<Buffer>> &particlesSsboBuffers);
		void updateMaterialDescriptorSets(const std::vector<std::unique_ptr<Buffer>> &materialDynUboBuffers,
		                                  const Texture &texture, VkDeviceSize materialUboAlignment);

		VkDescriptorSetLayout getDescriptorSetLayout() const { return _descriptorSetLayout; }
		VkDescriptorSet getDescriptorSet(uint32_t index) const { return _descriptorSets[index]; }
		VkDescriptorSetLayout getMaterialDescriptorSetLayout() const { return _materialDescriptorSetLayout; }
		VkDescriptorSet getMaterialDescriptorSet(uint32_t index) const { return _materialDescriptorSets[index]; }

	private:
		const Device& _device;
		VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorSetLayout _materialDescriptorSetLayout = VK_NULL_HANDLE;
		VkDescriptorPool _descriptorPool;
		std::vector<VkDescriptorSet> _descriptorSets;
		std::vector<VkDescriptorSet> _materialDescriptorSets;

		void createDescriptorSetLayout();

		void createMaterialDescriptorSetLayout();

		void createDescriptorPool();
		void createDescriptorSets();
	};
}


