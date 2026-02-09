#include "Buffer.hpp"
#include "Device.hpp"
#include "log/Log.hpp"
#include <stdexcept>
#include <cstring>

namespace m1
{
	Buffer::Buffer(const Device& device, VkDeviceSize size, VkBufferUsageFlags usage, VmaAllocationCreateFlags memoryProps) : _device(device)
	{
		Log::Get().Info("Creating buffer of size " + std::to_string(size));
		_size = size;
		createBuffer(size, usage, memoryProps);
	}

	Buffer::~Buffer()
	{
		Log::Get().Info("Destroying buffer");
		vmaDestroyBuffer(_device.getMemoryAllocator(), _vkBuffer, _allocation);
	}

	void Buffer::copyDataToBuffer(const void* data) const
	{
		// automatically maps the Vulkan memory temporarily (if not already mapped)
		vmaCopyMemoryToAllocation(_device.getMemoryAllocator(), data, _allocation, 0, _size);
	}

	void Buffer::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaAllocationCreateFlags memoryProps)
	{
		// Buffer Info
		VkBufferCreateInfo bufferInfo{};
		bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
		bufferInfo.size = size;
		bufferInfo.usage = usage; // purpose of the data in the buffer
		bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // sharing mode between multiple queue families

		// allocation info
		VmaAllocationCreateInfo allocInfo = {};
		allocInfo.usage = VMA_MEMORY_USAGE_AUTO; // best memory type selected automatically based on usage
		allocInfo.flags = memoryProps;

		// create the buffer
		vmaCreateBuffer(_device.getMemoryAllocator(), &bufferInfo, &allocInfo, &_vkBuffer, &_allocation, nullptr);
	}
}
