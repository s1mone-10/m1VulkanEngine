#pragma once

#include <vulkan/vulkan.h>
#include <geometry/Vertex.hpp>
#include <vector>

namespace va
{
	class Device; // Forward declaration

	class Buffer
	{
	public:
		Buffer(const Device& device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
		~Buffer();

		// Non-copyable, non-movable
		Buffer(const Buffer&) = delete;
		Buffer& operator=(const Buffer&) = delete;
		Buffer(Buffer&&) = delete;
		Buffer& operator=(Buffer&&) = delete;

		VkBuffer getVkBuffer() const { return _vkBuffer; }
		void copyDataToBuffer(void* data, VkDeviceSize size);

	private:
		VkBuffer _vkBuffer;
		VkDeviceMemory _deviceMemory;
		const Device& _device;
		void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
		uint32_t findMemoryTypeIndex(uint32_t typeFilter, VkMemoryPropertyFlags properties);
	};
}