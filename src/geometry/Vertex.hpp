#pragma once

//libs
#include "../graphics/glm_config.hpp"
#include <vulkan/vulkan.h>

// std
#include <array>
#include <functional>

namespace m1
{
	struct Vertex
	{
		glm::vec3 pos;
		glm::vec3 color;
		glm::vec3 normal;
		glm::vec2 texCoord;

		static VkVertexInputBindingDescription getBindingDescription();
		static std::array<VkVertexInputAttributeDescription, 4> getAttributeDescriptions();

		bool operator==(const Vertex& other) const
		{
			return pos == other.pos && color == other.color && normal == other.normal && texCoord == other.texCoord;
		}
	};
};

namespace std
{
	template <>
	struct hash<m1::Vertex>
	{
		size_t operator()(const m1::Vertex& vertex) const
		{
			size_t hashPos = hash<glm::vec3>()(vertex.pos);
			size_t hashColor = hash<glm::vec3>()(vertex.color);
			size_t hashTexCoord = hash<glm::vec2>()(vertex.texCoord);

			return ((hashPos ^ (hashColor << 1)) >> 1) ^ (hashTexCoord << 1);
		}
	};
} // namespace std