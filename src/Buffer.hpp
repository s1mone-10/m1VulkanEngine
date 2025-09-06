#pragma once

#include <vulkan/vulkan.h>
#include <geometry/Vertex.hpp>
#include <vector>

namespace m1
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
		void mapMemory();
		void unmapMemory();
		void copyDataToBuffer(void* data);

	private:
		VkBuffer _vkBuffer;
		VkDeviceMemory _deviceMemory;
		void* _mappedMemory = nullptr;
		VkDeviceSize _size;
		const Device& _device;
		void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
	};
}