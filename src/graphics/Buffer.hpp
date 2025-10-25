#pragma once

#include <geometry/Vertex.hpp>
#include <geometry/Material.hpp>

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

	struct alignas(16) MaterialUbo
	{
	public:
		MaterialUbo(const Material& material)
		{
			diffuseColor = material.getDiffuseColor();
			specularColor = material.getSpecularColor();
			ambientColor = material.getAmbientColor();
			shininess = material.getShininess();
			opacity = material.getOpacity();
		}

		glm::vec3 diffuseColor;
		glm::vec3 specularColor;
		glm::vec3 ambientColor;
		float shininess;
		float opacity;
		float padding[2]; // pad to 16 bytes
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
		VkDeviceSize getSize() const { return _size; }

	private:
		VkBuffer _vkBuffer;
		VkDeviceMemory _deviceMemory;
		void* _mappedMemory = nullptr;
		VkDeviceSize _size;
		const Device& _device;
		void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
	};
}