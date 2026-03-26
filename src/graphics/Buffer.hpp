#pragma once

#include <graphics/Material.hpp>

// libs
#include "vk_mem_alloc.h"

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
		glm::mat4 lightViewProjMatrix;
		glm::vec4 camPos; // 3 meaningful value, vec4 for padding
		float iblIntensity;
		int shadowsEnabled;
	};

	struct ObjectUbo
	{
		glm::mat4 model;
		glm::mat3 normalMatrix;
	};

	struct MaterialPhongUbo
	{
		explicit MaterialPhongUbo(const Material& material) : shininess(material.shininess), diffuseColor(material.baseColor),
			specularColor(material.specularColor), ambientColor(material.ambientColor)
		{
		}

		float shininess;
		glm::vec3 diffuseColor;
		glm::vec3 specularColor;
		glm::vec3 ambientColor;
	};

	struct MaterialPbrUbo
	{
		explicit MaterialPbrUbo(const Material& material) : baseColor(material.baseColor), emissiveFactor(glm::vec4(material.emissiveFactor, 1.0f)),
			metallicFactor(material.metallicFactor), roughnessFactor(material.roughnessFactor) {}

		glm::vec4 baseColor;
		glm::vec4 emissiveFactor;
		float metallicFactor;
	    float roughnessFactor;
	};

	class Buffer
	{
	public:
		Buffer(const Device& device, VkDeviceSize size, VkBufferUsageFlags usage, VmaAllocationCreateFlags memoryProps = 0);
		~Buffer();

		// Non-copyable
		Buffer(const Buffer&) = delete;
		Buffer& operator=(const Buffer&) = delete;

		[[nodiscard]] VkBuffer getVkBuffer() const { return _vkBuffer; }
		void copyDataToBuffer(const void* data) const;
		[[nodiscard]] VkDeviceSize getSize() const { return _size; }
		[[nodiscard]] VkDescriptorBufferInfo getVkDescriptorBufferInfo() const;

	private:
		VkBuffer _vkBuffer;
		VmaAllocation _allocation;
		VkDeviceSize _size;
		const Device& _device;
		void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VmaAllocationCreateFlags memoryProps);
	};
}