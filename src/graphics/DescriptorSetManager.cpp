#include "DescriptorSetManager.hpp"
#include "Device.hpp"
#include "Engine.hpp"
#include "Buffer.hpp"
#include "Utils.hpp"
#include "Log.hpp"
#include "Particle.hpp"

// std
#include <array>
#include <ranges>

namespace m1
{
	DescriptorSetManager::DescriptorSetManager(const Device& device) : _device(device)
    {
	    // DescriptorSet is a blueprint for the pipeline to know which resources are going to be accessed by the shaders

	    // create different sets based on update frequency
	    // set = 0 -> Ubo, Ssbo (per frame/object update)
	    // set = 1 -> Material

	    // Most frequently updated resources of each set must be first in binding order for performance optimization

	    createFrameDescriptorSetLayout();
	    createMaterialDescriptorSetLayout();
	    createMaterialPbrDescriptorSetLayout();
		createEquirectToCubemapDescriptorSetLayout();
		createSkyBoxDescriptorSetLayout();
		createParticleDescriptorSetLayout();
	    createDescriptorPool();
    }

    DescriptorSetManager::~DescriptorSetManager()
    {
        // descriptor set are automatically freed when the pool is destroyed
        vkDestroyDescriptorPool(_device.getVkDevice(), _descriptorPool, nullptr);

		for (auto layout: _descriptorSetLayouts | std::views::values)
			vkDestroyDescriptorSetLayout(_device.getVkDevice(), layout, nullptr);

        Log::Get().Info("Descriptor destroyed");
    }

    void DescriptorSetManager::createFrameDescriptorSetLayout()
    {
	    // Most frequently updated resources of each set must be first in binding order for performance optimization

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

		// Shadow map sampler
		VkDescriptorSetLayoutBinding shadowMapSamplerBinding
		{
			.binding = 3,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr
		};

		// Irradiance Map Sampler
		VkDescriptorSetLayoutBinding irradianceSamplerBinding
		{
			.binding = 4,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr
		};

	    // DescriptorSet Info
	    std::array bindings =
	    {
		    objectUboLayoutBinding,
		    frameUboLayoutBinding,
	    	lightsUboLayoutBinding,
			shadowMapSamplerBinding,
	    	irradianceSamplerBinding
	    };

	    VkDescriptorSetLayoutCreateInfo layoutInfo
		{
		    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
		    .bindingCount = static_cast<uint32_t>(bindings.size()),
		    .pBindings = bindings.data()
	    };

	    // Create the DescriptorSet
		VkDescriptorSetLayout descriptorSetLayout;
	    VK_CHECK(vkCreateDescriptorSetLayout(_device.getVkDevice(), &layoutInfo, nullptr, &descriptorSetLayout));
		_descriptorSetLayouts.emplace(DescriptorSetLayoutType::Frame, descriptorSetLayout);
    }

	void DescriptorSetManager::createMaterialDescriptorSetLayout()
    {
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

		VkDescriptorSetLayout descriptorSetLayout;
		VK_CHECK(vkCreateDescriptorSetLayout(_device.getVkDevice(), &layoutInfo, nullptr, &descriptorSetLayout));
		_descriptorSetLayouts.emplace(DescriptorSetLayoutType::MaterialPhong, descriptorSetLayout);
    }

	void DescriptorSetManager::createMaterialPbrDescriptorSetLayout()
	{
		// Materials DynUbo
		VkDescriptorSetLayoutBinding materialsDynUboLayoutBinding
		{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr
		};

		// Albedo Map Sampler
		VkDescriptorSetLayoutBinding albedoSamplerLayoutBinding
		{
			.binding = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr
		};

		// Normal Map Sampler
		VkDescriptorSetLayoutBinding normalSamplerLayoutBinding
		{
			.binding = 2,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr
		};

		// MetallicRoughness Map Sampler
		VkDescriptorSetLayoutBinding metallicRoughnessSamplerLayoutBinding
		{
			.binding = 3,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr
		};

		// Ambient Occlusion Map Sampler
		VkDescriptorSetLayoutBinding ambOcclusionSamplerLayoutBinding
		{
			.binding = 4,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr
		};

		// Emissive Map Sampler
		VkDescriptorSetLayoutBinding emissiveSamplerLayoutBinding
		{
			.binding = 5,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr
		};

		// DescriptorSet Info
		std::array bindings =
		{
			materialsDynUboLayoutBinding, albedoSamplerLayoutBinding, normalSamplerLayoutBinding,
			metallicRoughnessSamplerLayoutBinding, ambOcclusionSamplerLayoutBinding, emissiveSamplerLayoutBinding
		};

		VkDescriptorSetLayoutCreateInfo layoutInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = static_cast<uint32_t>(bindings.size()),
			.pBindings = bindings.data()
		};

		VkDescriptorSetLayout descriptorSetLayout;
		VK_CHECK(vkCreateDescriptorSetLayout(_device.getVkDevice(), &layoutInfo, nullptr, &descriptorSetLayout));
		_descriptorSetLayouts.emplace(DescriptorSetLayoutType::MaterialPbr, descriptorSetLayout);
	}

	void DescriptorSetManager::createEquirectToCubemapDescriptorSetLayout()
	{
		VkDescriptorSetLayoutBinding layoutBinding
		{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr
		};

		VkDescriptorSetLayoutCreateInfo layoutInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = 1,
			.pBindings = &layoutBinding
		};

		VkDescriptorSetLayout descriptorSetLayout;
		VK_CHECK(vkCreateDescriptorSetLayout(_device.getVkDevice(), &layoutInfo, nullptr, &descriptorSetLayout));
		_descriptorSetLayouts.emplace(DescriptorSetLayoutType::EquirectToCubemap, descriptorSetLayout);
	}

	void DescriptorSetManager::createSkyBoxDescriptorSetLayout()
	{
		VkDescriptorSetLayoutBinding layoutBinding
		{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
			.pImmutableSamplers = nullptr
		};

		VkDescriptorSetLayoutCreateInfo layoutInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = 1,
			.pBindings = &layoutBinding
		};

		VkDescriptorSetLayout descriptorSetLayout;
		VK_CHECK(vkCreateDescriptorSetLayout(_device.getVkDevice(), &layoutInfo, nullptr, &descriptorSetLayout));
		_descriptorSetLayouts.emplace(DescriptorSetLayoutType::SkyBox, descriptorSetLayout);
	}

	void DescriptorSetManager::createParticleDescriptorSetLayout()
	{
		// Particles SSBO layout binding (two bindings are required due to multiple frame-in flights,
		// as I need to read from the previous frame and write to the current one).
		VkDescriptorSetLayoutBinding particleSsboInLayoutBinding
		{
			.binding = 0,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr
		};

		VkDescriptorSetLayoutBinding particleSsboOutLayoutBinding
		{
			.binding = 1,
			.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
			.descriptorCount = 1,
			.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT,
			.pImmutableSamplers = nullptr
		};

		std::array bindings =
		{
			particleSsboInLayoutBinding,
			particleSsboOutLayoutBinding,
		};

		VkDescriptorSetLayoutCreateInfo layoutInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
			.bindingCount = static_cast<uint32_t>(bindings.size()),
			.pBindings = bindings.data()
		};

		// Create the DescriptorSet
		VkDescriptorSetLayout descriptorSetLayout;
		VK_CHECK(vkCreateDescriptorSetLayout(_device.getVkDevice(), &layoutInfo, nullptr, &descriptorSetLayout));
		_descriptorSetLayouts.emplace(DescriptorSetLayoutType::ComputeParticles, descriptorSetLayout);
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
        poolSizes[2].descriptorCount = static_cast<uint32_t>(1000); // sampler, one for each material + shadow map sampler
		poolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		poolSizes[3].descriptorCount = static_cast<uint32_t>(Engine::FRAMES_IN_FLIGHT) * 2; // *2 => prev and current frame SSBO

        // DescriptorPool Info
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(1000);

		// Create the descriptor pool
        VK_CHECK(vkCreateDescriptorPool(_device.getVkDevice(), &poolInfo, nullptr, &_descriptorPool));
    }

	std::vector<VkDescriptorSet> DescriptorSetManager::allocateDescriptorSets(DescriptorSetLayoutType layoutType, uint32_t count) const
	{
		// TODO I should probably use a pool with VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT for materials

		// Material Allocate Info
		std::vector layouts(count, _descriptorSetLayouts.at(layoutType));
		VkDescriptorSetAllocateInfo materialAllocInfo
		{
			.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			.descriptorPool = _descriptorPool,
			.descriptorSetCount = count,
			.pSetLayouts = layouts.data()
		};

		// create material descriptor sets
		auto descriptorSets = std::vector<VkDescriptorSet>(count);
        VK_CHECK(vkAllocateDescriptorSets(_device.getVkDevice(), &materialAllocInfo, descriptorSets.data()));

		return descriptorSets;
    }
}
