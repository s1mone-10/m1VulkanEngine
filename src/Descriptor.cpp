#include "Descriptor.hpp"
#include "Device.hpp"
#include "Engine.hpp"
#include <stdexcept>
#include <iostream>

namespace m1
{
	Descritor::Descritor(const Device& device) : _device(device)
    {
		createDescriptorSetLayout();
        createDescriptorPool();
        createDescriptorSets();
    }

    Descritor::~Descritor()
    {
        // descriptor set are automatically freed when the pool is destroyed
        vkDestroyDescriptorPool(_device.getVkDevice(), _descriptorPool, nullptr);

        vkDestroyDescriptorSetLayout(_device.getVkDevice(), _descriptorSetLayout, nullptr);
        std::cout << "Descriptor destroyed" << std::endl;
    }

    void Descritor::createDescriptorSetLayout()
    {
		// Blueprint for the pipeline to know which resources are going to be accessed by the shaders

        // Uniform buffer layout binding
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0; // binding number. Correspond number used in the shaders
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.descriptorCount = 1; // number of descriptors in the binding, for arrays
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // which shader stages will access this binding
        uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

		// Sampler layout binding
        VkDescriptorSetLayoutBinding samplerLayoutBinding{};
        samplerLayoutBinding.binding = 1;
        samplerLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        samplerLayoutBinding.descriptorCount = 1;
        samplerLayoutBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
        samplerLayoutBinding.pImmutableSamplers = nullptr;

        // DescriptorSet Info
		std::array<VkDescriptorSetLayoutBinding, 2> bindings = { uboLayoutBinding, samplerLayoutBinding };
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = static_cast<uint32_t>(bindings.size());
        layoutInfo.pBindings = bindings.data();

        // Create the DescriptorSet
        if (vkCreateDescriptorSetLayout(_device.getVkDevice(), &layoutInfo, nullptr, &_descriptorSetLayout) != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor set layout!");
    }

    void Descritor::createDescriptorPool()
    {
		// Pool sizes
        std::array<VkDescriptorPoolSize, 2> poolSizes{};
        poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSizes[0].descriptorCount = static_cast<uint32_t>(Engine::FRAMES_IN_FLIGHT);
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

    void Descritor::createDescriptorSets()
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

    void Descritor::updateDescriotorSets(const std::vector<std::unique_ptr<Buffer>>& buffers, const Texture& texture)
    {
        // populate each DescriptorSet
        for (size_t i = 0; i < Engine::FRAMES_IN_FLIGHT; i++)
        {
            // Uniform Buffer Info
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = buffers[i]->getVkBuffer();
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject); // or VK_WHOLE_SIZE

			// Buffer Descriptor Write
            VkWriteDescriptorSet bufferDescriptorWrite{};
            bufferDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            bufferDescriptorWrite.dstSet = _descriptorSets[i];
            bufferDescriptorWrite.dstBinding = 0;
            bufferDescriptorWrite.dstArrayElement = 0;
            bufferDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            bufferDescriptorWrite.descriptorCount = 1;
            // set the struct that actually configure the descriptors
            bufferDescriptorWrite.pBufferInfo = &bufferInfo;

			// Texture Image Info
            VkDescriptorImageInfo imageInfo{};
            imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            imageInfo.imageView = texture.getImage().getVkImageView();
            imageInfo.sampler = texture.getSampler();

			// Texture Descriptor Write
            VkWriteDescriptorSet textureDescriptorWrite{};
            textureDescriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            textureDescriptorWrite.dstSet = _descriptorSets[i];
            textureDescriptorWrite.dstBinding = 1;
            textureDescriptorWrite.dstArrayElement = 0;
            textureDescriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            textureDescriptorWrite.descriptorCount = 1;
            textureDescriptorWrite.pImageInfo = &imageInfo;

			std::array<VkWriteDescriptorSet, 2> descriptorWrites = { bufferDescriptorWrite, textureDescriptorWrite };

            vkUpdateDescriptorSets(_device.getVkDevice(), static_cast<uint32_t>(descriptorWrites.size()), descriptorWrites.data(), 0, nullptr);
        }
    }
}