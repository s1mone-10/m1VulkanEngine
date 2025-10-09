#pragma once

#include "Vertex.hpp"

//std
#include <memory>
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

		void compile(const Device& device);
		void draw(VkCommandBuffer commandBuffer);

		std::vector<Vertex> Vertices;
		std::vector<uint32_t> Indices;
	private:
		void createVertexBuffer(const Device& device);
		void createIndexBuffer(const Device& device);

		std::unique_ptr<Buffer> _vertexBuffer;
		std::unique_ptr<Buffer> _indexBuffer;
	};
}