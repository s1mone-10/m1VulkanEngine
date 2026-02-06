#pragma once

#include <graphics/Material.hpp>

// libs
#include <vulkan/vulkan.h>

namespace m1
{
	class Device; // Forward declaration

	#define MAX_LIGHTS 10

	struct Light
	{
		glm::vec4 posDir;		// w=0 directional, w=1 point
		glm::vec4 color;		// rgb = color, a = intensity
		glm::vec4 attenuation;	// x = constant, y = linear, z = quadratic
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
		glm::vec3 camPos;
	};

	struct ObjectUbo
	{
		glm::mat4 model;
		glm::mat3 normalMatrix;
	};

	struct MaterialUbo
	{
		MaterialUbo(const Material& material)
		{
			diffuseColor = material.diffuseColor;
			specularColor = material.specularColor;
			ambientColor = material.ambientColor;
			shininess = material.shininess;
			opacity = material.opacity;
		}

		float shininess;
		float opacity;
		glm::vec3 diffuseColor;
		glm::vec3 specularColor;
		glm::vec3 ambientColor;
	};

	class Buffer
	{
	public:
		Buffer(const Device& device, VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties);
		~Buffer();

		// Non-copyable
		Buffer(const Buffer&) = delete;
		Buffer& operator=(const Buffer&) = delete;

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