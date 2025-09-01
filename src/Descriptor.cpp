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

        // Descriptors binding
        VkDescriptorSetLayoutBinding uboLayoutBinding{};
        uboLayoutBinding.binding = 0; // binding number. Correspond number used in the shaders
        uboLayoutBinding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		uboLayoutBinding.descriptorCount = 1; // number of descriptors in the binding, for arrays
		uboLayoutBinding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT; // which shader stages will access this binding
        uboLayoutBinding.pImmutableSamplers = nullptr; // Optional

        // DescriptorSet Info
        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &uboLayoutBinding;

        // Create the DescriptorSet
        if (vkCreateDescriptorSetLayout(_device.getVkDevice(), &layoutInfo, nullptr, &_descriptorSetLayout) != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor set layout!");
    }

    void Descritor::createDescriptorPool()
    {
        // DescriptorPool Info
        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;

        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        poolSize.descriptorCount = static_cast<uint32_t>(Engine::FRAMES_IN_FLIGHT);
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = static_cast<uint32_t>(Engine::FRAMES_IN_FLIGHT);

        if (vkCreateDescriptorPool(_device.getVkDevice(), &poolInfo, nullptr, &_descriptorPool) != VK_SUCCESS)
            throw std::runtime_error("failed to create descriptor pool!");
    }

    void Descritor::createDescriptorSets()
    {
        // DescriptorSet Info
        std::vector<VkDescriptorSetLayout> layouts(Engine::FRAMES_IN_FLIGHT, _descriptorSetLayout);
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = _descriptorPool;
        allocInfo.descriptorSetCount = static_cast<uint32_t>(Engine::FRAMES_IN_FLIGHT);
        allocInfo.pSetLayouts = layouts.data();

        // create DescriptorSets
        _descriptorSets.resize(Engine::FRAMES_IN_FLIGHT);
        if (vkAllocateDescriptorSets(_device.getVkDevice(), &allocInfo, _descriptorSets.data()) != VK_SUCCESS)
            throw std::runtime_error("failed to allocate descriptor sets!");
    }

    void Descritor::updateDescriotorSets(const std::vector<std::unique_ptr<Buffer>>& buffers)
    {
        // populate each DescriptorSet
        for (size_t i = 0; i < Engine::FRAMES_IN_FLIGHT; i++)
        {
            // Buffer Info
            VkDescriptorBufferInfo bufferInfo{};
            bufferInfo.buffer = buffers[i]->getVkBuffer();
            bufferInfo.offset = 0;
            bufferInfo.range = sizeof(UniformBufferObject); // or VK_WHOLE_SIZE 


            VkWriteDescriptorSet descriptorWrite{};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = _descriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            // set the struct that actually configure the descriptors
            descriptorWrite.pBufferInfo = &bufferInfo;
            descriptorWrite.pImageInfo = nullptr; // Optional
            descriptorWrite.pTexelBufferView = nullptr; // Optional

            vkUpdateDescriptorSets(_device.getVkDevice(), 1, &descriptorWrite, 0, nullptr);
        }
    }
}