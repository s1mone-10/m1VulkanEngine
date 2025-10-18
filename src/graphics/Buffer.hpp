#pragma once

#include <geometry/Vertex.hpp>

// libs
#include <vulkan/vulkan.h>

namespace m1
{
	class Device; // Forward declaration

	#define MAX_LIGHTS 10

	struct Light
	{
		glm::vec4 position;  // w=0 directional, w=1 point
		glm::vec4 color;     // rgb = color, a = intensity
	};

	struct LightsUbo
	{
		glm::vec4 ambient;   // rgb = ambient color, a = intensity
		Light lights[MAX_LIGHTS];
		int numLights;
	};

	struct FrameUbo
	{
		glm::mat4 view;
		glm::mat4 proj;
	};

	struct ObjectUbo
	{
		glm::mat4 model;
		glm::mat3 normalMatrix;
	};

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