#pragma once

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtx/hash.hpp>

#include <vulkan/vulkan.h>
#include <array>
#include <functional>

namespace m1
{
	struct Vertex
	{
		glm::vec3 pos;
		glm::vec3 color;
		glm::vec2 texCoord;

		bool operator==(const Vertex& other) const
		{
			return pos == other.pos && color == other.color && texCoord == other.texCoord;
		}
	};

	struct UniformBufferObject
	{
		glm::mat4 model;
		glm::mat4 view;
		glm::mat4 proj;
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