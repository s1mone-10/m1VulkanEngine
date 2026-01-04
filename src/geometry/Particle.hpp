#pragma once

//libs
#include "../graphics/glm_config.hpp"
#include <vulkan/vulkan.h>
#include <vector>

namespace m1
{
	struct Particle
	{
		glm::vec2 position;
		glm::vec2 velocity;
		glm::vec4 color;

		static VkVertexInputBindingDescription getVertexBindingDescription()
		{
			VkVertexInputBindingDescription bindingDescription{};
			bindingDescription.binding = 0;
			bindingDescription.stride = sizeof(Particle);
			bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

			return bindingDescription;
		}

		static std::vector<VkVertexInputAttributeDescription> getVertexAttributeDescriptions()
		{
			// no need of velocity in the vertex shader

			std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
			attributeDescriptions.reserve(2);

			VkVertexInputAttributeDescription attr{};

			attr.binding = 0;
			attr.location = 0;
			attr.format = VK_FORMAT_R32G32_SFLOAT;
			attr.offset = offsetof(Particle, position);
			attributeDescriptions.push_back(attr);

			attr.binding = 0;
			attr.location = 1;
			attr.format = VK_FORMAT_R32G32B32A32_SFLOAT;
			attr.offset = offsetof(Particle, color);
			attributeDescriptions.push_back(attr);

			return attributeDescriptions;
		}
	};
}