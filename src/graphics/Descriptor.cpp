#include "Descriptor.hpp"
#include "Device.hpp"
#include "Engine.hpp"
#include "../log/Log.hpp"

// std
#include <stdexcept>
#include <iostream>

namespace m1
{
	Descriptor::Descriptor(const Device& device) : _device(device)
    {
		createDescriptorSetLayout();
        createDescriptorPool();
        createDescriptorSets();
    }

    Descriptor::~Descriptor()
    {
        // descriptor set are automatically freed when the pool is destroyed
        vkDestroyDescriptorPool(_device.getVkDevice(), _descriptorPool, nullptr);

        vkDestroyDescriptorSetLayout(_device.getVkDevice(), _descriptorSetLayout, nullptr);
        Log::Get().Info("Descriptor destroyed");
    }

    void Descriptor::createDescriptorSetLayout()
    {
	    // Blueprint for the pipeline to know which resources are going to be accessed by the shaders

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
		    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
		    .pImmutableSamplers = nullptr
	    };

	    // Sampler layout binding
	    VkDescriptorSetLayoutBinding samplerLayoutBinding
		{
		    .binding = 2,
		    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
		    .descriptorCount = 1,
		    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		    .pImmutableSamplers = nullptr
	    };

	    // Lights Ubo layout binding
	    VkDescriptorSetLayoutBinding lightsUboLayoutBinding
		{
		    .binding = 3,
		    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
		    .descriptorCount = 1,
		    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
		    .pImmutableSamplers = nullptr
	    };

	    // DescriptorSet Info
	    std::array<VkDescriptorSetLayoutBinding, 4> bindings =
	    {
		    objectUboLayoutBinding,
		    frameUboLayoutBinding,
		    samplerLayoutBinding,
		    lightsUboLayoutBinding
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
    }

    void Descriptor::createDescriptorPool()
    {
	    // Pool sizes
	    std::array<VkDescriptorPoolSize, 2> poolSizes{};
	    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(Engine::FRAMES_IN_FLIGHT * 3); // *3 => frame, object and lights UBO
        poolSizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSizes[1].descriptorCount = static_cast<uint32_t>(Engine::FRAMES_IN_FLIGHT);

        // DescriptorPool Info
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = static_cast<uint32_t>(Engine::FRAMES_IN_FLIGHT);

        if (vkCreateDescriptorPool(_device.getVkDevice(), &poolInfo, nullptr, &_descriptorPool) != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor pool!");
    }

    void Descriptor::createDescriptorSets()
    {
        // DescriptorSet Info
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = _descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(Engine::FRAMES_IN_FLIGHT);
        std::vector<VkDescriptorSetLayout> layouts(Engine::FRAMES_IN_FLIGHT, _descriptorSetLayout);
        allocInfo.pSetLayouts = layouts.data();

        // create DescriptorSets
        _descriptorSets.resize(Engine::FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(_device.getVkDevice(), &allocInfo, _descriptorSets.data()) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate descriptor sets!");
    }

    void Descriptor::updateDescriptorSets(const std::vector<std::unique_ptr<Buffer> > &objectUboBuffers,
                                          const std::vector<std::unique_ptr<Buffer> > &frameUboBuffers,
                                          const Texture &texture, const Buffer &lightsUbo)
    {
	    // LightUbo Info
	    VkDescriptorBufferInfo lightUboInfo
		{
		    .buffer = lightsUbo.getVkBuffer(),
		    .offset = 0,
		    .range = sizeof(LightsUbo)
	    };

	    // populate each DescriptorSet
	    for (size_t i = 0; i < Engine::FRAMES_IN_FLIGHT; i++)
	    {
		    // ObjectUbo Info
		    VkDescriptorBufferInfo objectUboInfo
	    	{
			    .buffer = objectUboBuffers[i]->getVkBuffer(),
			    .offset = 0,
			    .range = sizeof(ObjectUbo)
		    };

		    // FrameUbo Info
		    VkDescriptorBufferInfo frameUboInfo
	    	{
			    .buffer = frameUboBuffers[i]->getVkBuffer(),
			    .offset = 0,
			    .range = sizeof(FrameUbo)
		    };

		    // Texture Image Info
		    VkDescriptorImageInfo imageInfo
	    	{
		    	.sampler = texture.getSampler(),
		    	.imageView = texture.getImage().getVkImageView(),
			    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
		    };

		    // ObjectUbo Descriptor Write
		    VkWriteDescriptorSet objectUboWrite
	    	{
			    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			    .dstSet = _descriptorSets[i],
			    .dstBinding = 0,
			    .dstArrayElement = 0,
	    		.descriptorCount = 1,
			    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			    .pBufferInfo = &objectUboInfo
		    };

		    // FrameUbo Descriptor Write
		    VkWriteDescriptorSet frameUboWrite
	    	{
			    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			    .dstSet = _descriptorSets[i],
			    .dstBinding = 1,
			    .dstArrayElement = 0,
	    		.descriptorCount = 1,
			    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			    .pBufferInfo = &frameUboInfo
		    };

		    // Texture Descriptor Write
		    VkWriteDescriptorSet textureDescriptorWrite
	    	{
			    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			    .dstSet = _descriptorSets[i],
			    .dstBinding = 2,
			    .dstArrayElement = 0,
	    		.descriptorCount = 1,
			    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
			    .pImageInfo = &imageInfo
		    };

		    // Lights Ubo Descriptor Write
		    VkWriteDescriptorSet lightsDescriptorWrite
	    	{
			    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			    .dstSet = _descriptorSets[i],
			    .dstBinding = 3,
			    .dstArrayElement = 0,
	    		.descriptorCount = 1,
			    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			    .pBufferInfo = &lightUboInfo
		    };

		    std::array<VkWriteDescriptorSet, 4> descriptorWrites =
		    {
			    objectUboWrite, frameUboWrite, textureDescriptorWrite, lightsDescriptorWrite
		    };

		    vkUpdateDescriptorSets(_device.getVkDevice(), static_cast<uint32_t>(descriptorWrites.size()),
		                           descriptorWrites.data(), 0, nullptr);
	    }
    }
}
