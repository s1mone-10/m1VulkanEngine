#include "Buffer.hpp"
#include "Device.hpp"
#include "log/Log.hpp"
#include <stdexcept>
#include <cstring>

namespace m1
{
	Buffer::Buffer(const Device& device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProps) : _device(device)
	{
		Log::Get().Info("Creating buffer of size " + std::to_string(size));
		_size = size;
		createBuffer(size, usage, memoryProps);
	}

	Buffer::~Buffer()
	{
		Log::Get().Info("Destroying buffer");
		unmapMemory();
		vkDestroyBuffer(_device.getVkDevice(), _vkBuffer, nullptr);
		vkFreeMemory(_device.getVkDevice(), _deviceMemory, nullptr);	
	}

	/// <summary>
	/// Maps the device memory into application address space
	/// </summary>
	void Buffer::mapMemory()
	{
		VkResult result = vkMapMemory(_device.getVkDevice(), _deviceMemory, 0, _size, 0, &_mappedMemory);
		if (result != VK_SUCCESS)
        {
            Log::Get().Error("Failed to map buffer memory!");
			throw std::runtime_error("Failed to map buffer memory!");
        }
	}

	/// <summary>
	/// Unmap the device memory
	/// </summary>
	void Buffer::unmapMemory()
	{
		if (_mappedMemory)
		{
			vkUnmapMemory(_device.getVkDevice(), _deviceMemory);
			_mappedMemory = nullptr;
		}
	}

	void Buffer::copyDataToBuffer(void* data)
	{
		bool mapped = _mappedMemory != nullptr;

		// map the GPU memory
		if (!mapped)
			mapMemory();

		// copy the data
		memcpy(_mappedMemory, data, (size_t)_size);

		// unmap the GPU memory
		if (!mapped)
			unmapMemory();
	}

	void Buffer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags memoryProps)
	{
		// Buffer Info
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage; // purpose of the data in the buffer
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // sharing mode between multiple queue families

		// Create the buffer
		if (vkCreateBuffer(_device.getVkDevice(), &bufferInfo, nullptr, &_vkBuffer) != VK_SUCCESS)
        {
            Log::Get().Error("failed to create buffer!");
			throw std::runtime_error("failed to create buffer!");
        }
		
		// Memory Info
		VkMemoryRequirements memRequirements; 
		vkGetBufferMemoryRequirements(_device.getVkDevice(), _vkBuffer, &memRequirements);

		// Allocate memory
		_deviceMemory = _device.allocateMemory(memRequirements, memoryProps);

		// Bind the buffer with the memory
		vkBindBufferMemory(_device.getVkDevice(), _vkBuffer, _deviceMemory, 0);
	}
}
