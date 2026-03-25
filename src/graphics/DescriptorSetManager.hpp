#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <unordered_map>

namespace m1
{
	class Device; // Forward declaration
	class Buffer;
	class Texture;

	enum class DescriptorSetLayoutType
	{
		Frame,
		MaterialPhong,
		MaterialPbr,
		ComputeParticles,
		OneSampler,
	};

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

		[[nodiscard]] std::vector<VkDescriptorSet> allocateDescriptorSets(DescriptorSetLayoutType layoutType, uint32_t count) const;
		[[nodiscard]] VkDescriptorSetLayout getDescriptorSetLayout(DescriptorSetLayoutType layoutType) const { return _descriptorSetLayouts.at(layoutType); }

	private:
		const Device& _device;
		std::unordered_map<DescriptorSetLayoutType, VkDescriptorSetLayout> _descriptorSetLayouts;
		VkDescriptorPool _descriptorPool;

		void createFrameDescriptorSetLayout();
		void createMaterialDescriptorSetLayout();
		void createMaterialPbrDescriptorSetLayout();
		void createOneSamplerDescriptorSetLayout();
		void createParticleDescriptorSetLayout();
		void createDescriptorPool();
	};
}


