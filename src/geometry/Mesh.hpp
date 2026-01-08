#pragma once

#include "Vertex.hpp"

//libs
#include "graphics/glm_config.hpp"

//std
#include <memory>
#include <string>
#include <vector>

namespace m1 
{
	class Buffer;
	class Device;

	class Mesh 
	{
	public:
		Mesh();
		~Mesh();

		static std::unique_ptr<Mesh> createCube(const glm::vec3& color = glm::vec3(1.0f, 0.0f, 0.0f));

		void setMaterialName(const std::string& materialName) { _materialName = materialName; }
		const std::string& getMaterialName() const { return _materialName; }
		void compile(const Device& device);
		void draw(VkCommandBuffer commandBuffer) const;

		std::vector<Vertex> Vertices;
		std::vector<uint32_t> Indices;
	private:
		void createVertexBuffer(const Device& device);
		void createIndexBuffer(const Device& device);

		std::unique_ptr<Buffer> _vertexBuffer;
		std::unique_ptr<Buffer> _indexBuffer;

		std::string _materialName;
	};
}