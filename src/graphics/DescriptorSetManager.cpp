#include "DescriptorSetManager.hpp"
#include "Device.hpp"
#include "Engine.hpp"
#include "Buffer.hpp"
#include "../log/Log.hpp"
#include "geometry/Particle.hpp"

// std
#include <array>
#include <stdexcept>

namespace m1
{
	DescriptorSetManager::DescriptorSetManager(const Device& device) : _device(device)
    {
		createFrameDescriptorSetLayout();
        createDescriptorPool();
    }

    DescriptorSetManager::~DescriptorSetManager()
    {
        // descriptor set are automatically freed when the pool is destroyed
        vkDestroyDescriptorPool(_device.getVkDevice(), _descriptorPool, nullptr);

        vkDestroyDescriptorSetLayout(_device.getVkDevice(), _descriptorSetLayout, nullptr);
        vkDestroyDescriptorSetLayout(_device.getVkDevice(), _materialDescriptorSetLayout, nullptr);
        Log::Get().Info("Descriptor destroyed");
    }

    void DescriptorSetManager::createFrameDescriptorSetLayout()
    {
	    // Blueprint for the pipeline to know which resources are going to be accessed by the shaders

		// create different sets based on update frequency
		// set = 0 -> Ubo, Ssbo (per frame/object update)
		// set = 1 -> Material

	    // Most frequently updated UBO first in binding order for performance optimization

	    // Object Uniform buffer layout binding
	    VkDescriptorSetLayoutBinding objectUboLayoutBinding
		{
		    .binding = 0, // binding number. Correspond number used in the shaders
		    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		    .descriptorCount = 1, // number of descriptors in the binding, for arrays
		    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT, // which shader stages will access this binding
		    .pImmutableSamplers = nullptr
	    };

	    // Frame Uniform buffer layout binding
	    VkDescriptorSetLayoutBinding frameUboLayoutBinding
		{
		    .binding = 1,
		    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		    .descriptorCount = 1,
		    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
		    .pImmutableSamplers = nullptr
	    };

	    // Lights Ubo layout binding
	    VkDescriptorSetLayoutBinding lightsUboLayoutBinding
		{
		    .binding = 2,
		    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		    .descriptorCount = 1,
		    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		    .pImmutableSamplers = nullptr
	    };

		// Particles SSBO layout binding (two bindings are required due to multiple frame-in flights,
		// as I need to read from the previous frame and write to the current one).
		VkDescriptorSetLayoutBinding particleSsboInLayoutBinding
		{
			.binding = 3,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr
		};

		VkDescriptorSetLayoutBinding particleSsboOutLayoutBinding
		{
			.binding = 4,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr
		};

	    // DescriptorSet Info
	    std::array bindings =
	    {
		    objectUboLayoutBinding,
		    frameUboLayoutBinding,
	    	lightsUboLayoutBinding,
	    	particleSsboInLayoutBinding,
	    	particleSsboOutLayoutBinding
	    };

	    VkDescriptorSetLayoutCreateInfo layoutInfo
		{
		    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		    .bindingCount = static_cast<uint32_t>(bindings.size()),
		    .pBindings = bindings.data()
	    };

	    // Create the DescriptorSet
	    if (vkCreateDescriptorSetLayout(_device.getVkDevice(), &layoutInfo, nullptr, &_descriptorSetLayout) !=	VK_SUCCESS)
		    throw std::runtime_error("failed to create descriptor set layout!");

		createMaterialDescriptorSetLayout();
    }

	void DescriptorSetManager::createMaterialDescriptorSetLayout()
    {
	    // Blueprint for the pipeline to know which resources are going to be accessed by the shaders

	    // Most frequently updated UBO first in binding order for performance optimization

		// Materials DynUbo
		VkDescriptorSetLayoutBinding materialsDynUboLayoutBinding
		{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr
		};

	    // Diffuse Map Sampler
	    VkDescriptorSetLayoutBinding diffuseSamplerLayoutBinding
		{
		    .binding = 1,
		    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		    .descriptorCount = 1,
		    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		    .pImmutableSamplers = nullptr
	    };

		// Specular Map Sampler
		VkDescriptorSetLayoutBinding specularSamplerLayoutBinding
		{
			.binding = 2,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr
		};

	    // DescriptorSet Info
	    std::array bindings =
	    {
		    materialsDynUboLayoutBinding, diffuseSamplerLayoutBinding, specularSamplerLayoutBinding
	    };

	    VkDescriptorSetLayoutCreateInfo layoutInfo
		{
		    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		    .bindingCount = static_cast<uint32_t>(bindings.size()),
		    .pBindings = bindings.data()
	    };

	    // Create the DescriptorSet
	    if (vkCreateDescriptorSetLayout(_device.getVkDevice(), &layoutInfo, nullptr, &_materialDescriptorSetLayout) !=	VK_SUCCESS)
		    throw std::runtime_error("failed to create descriptor set layout!");
    }

    void DescriptorSetManager::createDescriptorPool()
    {
	    // Pool sizes
	    std::array<VkDescriptorPoolSize, 4> poolSizes{};
	    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(Engine::FRAMES_IN_FLIGHT * 3); // *3 => frame, object and lights UBO
		poolSizes[1].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		poolSizes[1].descriptorCount = static_cast<uint32_t>(Engine::FRAMES_IN_FLIGHT); // materials dyn ubo (each buffer contains all materials data)
        poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[2].descriptorCount = static_cast<uint32_t>(1000); // sampler, one for each material
		poolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		poolSizes[3].descriptorCount = static_cast<uint32_t>(Engine::FRAMES_IN_FLIGHT) * 2; // *2 => prev and current frame SSBO

        // DescriptorPool Info
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(1000);

        if (vkCreateDescriptorPool(_device.getVkDevice(), &poolInfo, nullptr, &_descriptorPool) != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor pool!");
    }

	std::vector<VkDescriptorSet> DescriptorSetManager::allocateFrameDescriptorSets(uint32_t count) const
	{
		// DescriptorSet Info
		VkDescriptorSetAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
		allocInfo.descriptorPool = _descriptorPool;
		allocInfo.descriptorSetCount = count;
		std::vector<VkDescriptorSetLayout> layouts(count, _descriptorSetLayout);
		allocInfo.pSetLayouts = layouts.data();

		// create DescriptorSets
		auto descriptorSets = std::vector<VkDescriptorSet>(count);
		if (vkAllocateDescriptorSets(_device.getVkDevice(), &allocInfo, descriptorSets.data()) != VK_SUCCESS)
			throw std::runtime_error("failed to allocate descriptor sets!");

		return descriptorSets;
	}

	std::vector<VkDescriptorSet> DescriptorSetManager::allocateMaterialDescriptorSets(uint32_t count) const
	{
		// TODO I should probably use a pool with VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT for materials

		// Material Allocate Info
		std::vector<VkDescriptorSetLayout> materialLayouts(count, _materialDescriptorSetLayout);
		VkDescriptorSetAllocateInfo materialAllocInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = _descriptorPool,
			.descriptorSetCount = (count),
			.pSetLayouts = materialLayouts.data()
		};

		// create material descriptor sets
		auto descriptorSets = std::vector<VkDescriptorSet>(count);
		auto vk_allocate_descriptor_sets = vkAllocateDescriptorSets(_device.getVkDevice(), &materialAllocInfo, descriptorSets.data());
		if (vk_allocate_descriptor_sets != VK_SUCCESS)
			throw std::runtime_error("failed to allocate material descriptor sets!");

		return descriptorSets;
    }
}
