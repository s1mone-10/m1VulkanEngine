#include "Buffer.hpp"
#include "Device.hpp"
#include <stdexcept>

namespace va
{
	Buffer::Buffer(const Device& device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties) : _device(device)
	{
		createBuffer(size, usage, properties);
	}

	Buffer::~Buffer()
	{
		vkDestroyBuffer(_device.getVkDevice(), _vkBuffer, nullptr);
		vkFreeMemory(_device.getVkDevice(), _deviceMemory, nullptr);	
	}

	void Buffer::copyDataToBuffer(void* data, VkDeviceSize size)
	{
		// map the GPU memory
		void* mappedData;
		VkResult result = vkMapMemory(_device.getVkDevice(), _deviceMemory, 0, size, 0, &mappedData);
		if (result != VK_SUCCESS)
			throw std::runtime_error("Failed to map buffer memory!");

		// copy the data
		memcpy(mappedData, data, (size_t)size);

		// unmap the GPU memory
		vkUnmapMemory(_device.getVkDevice(), _deviceMemory);
	}

	void Buffer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties)
	{
		// Buffer Info
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage; // purpose of the data in the buffer
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // sharing mode between multiple queue families

		// Create the buffer
		if (vkCreateBuffer(_device.getVkDevice(), &bufferInfo, nullptr, &_vkBuffer) != VK_SUCCESS)
			throw std::runtime_error("failed to create vertex buffer!");
		
		// Memory Info
		VkMemoryRequirements memRequirements; 
		vkGetBufferMemoryRequirements(_device.getVkDevice(), _vkBuffer, &memRequirements);

		VkMemoryAllocateInfo allocInfo{};
		allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
		allocInfo.allocationSize = memRequirements.size; // maybe different from bufferInfo.size
		allocInfo.memoryTypeIndex = findMemoryTypeIndex(memRequirements.memoryTypeBits, properties);

		// Allocate the memory
		if (vkAllocateMemory(_device.getVkDevice(), &allocInfo, nullptr, &_deviceMemory) != VK_SUCCESS)
			throw std::runtime_error("failed to allocate vertex buffer memory!");

		// Bind the buffer with the memory
		vkBindBufferMemory(_device.getVkDevice(), _vkBuffer, _deviceMemory, 0);
	}

	/// <summary>
	/// Finds a suitable memory type index based on a type filter and desired memory properties.
	/// </summary>
	/// <param name="typeFilter">A bitmask specifying the acceptable memory types.</param>
	/// <param name="properties">Flags specifying the desired memory properties.</param>
	/// <returns>The index of a suitable memory type that matches the filter and properties.</returns>
	uint32_t Buffer::findMemoryTypeIndex(uint32_t typeFilter, VkMemoryPropertyFlags properties)
	{
		// GPU can offer different types of memory, each type varies in terms of allowed operations and performance characteristics

		// Get the memory properties of the physical device
		VkPhysicalDeviceMemoryProperties memProperties = _device.getMemoryProperties();

		// Find a memory type that matches the type filter and has the desired properties
		for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++)
		{
			if ((typeFilter & (1 << i))	&& (memProperties.memoryTypes[i].propertyFlags & properties) == properties)
			{
				return i;
			}
		}

		throw std::runtime_error("failed to find suitable memory type!");
	}
}
