#include "Vertex.hpp"
#include "Mesh.hpp"
#include "../graphics/Buffer.hpp"
#include "../graphics/Utils.hpp"
#include "../log/Log.hpp"

// std
#include <memory>


namespace m1
{
	Mesh::Mesh()
	{
		Log::Get().Info("Creating mesh");
	}

	Mesh::~Mesh()
	{
		Log::Get().Info("Destroying mesh");
	}

    void Mesh::compile(const Device& device)
    {
        createVertexBuffer(device);
        createIndexBuffer(device);
    }

    void Mesh::draw(VkCommandBuffer commandBuffer) const
    {
        // bind the vertex buffer
        VkBuffer vertexBuffers[] = { _vertexBuffer->getVkBuffer() };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        // bind the index buffer
        vkCmdBindIndexBuffer(commandBuffer, _indexBuffer->getVkBuffer(), 0, VK_INDEX_TYPE_UINT32);

        // draw command
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(Indices.size()), 1, 0, 0, 0);
    }

    void Mesh::createVertexBuffer(const Device& device)
    {
        VkDeviceSize size = sizeof(Vertices[0]) * Vertices.size();

        // Create the actual vertex buffer with device local memory for better performance
        _vertexBuffer = std::make_unique<Buffer>(device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

        // upload vertex data to buffer
        Utils::uploadToDeviceBuffer(device, *_vertexBuffer, size, Vertices.data());
    }

    void Mesh::createIndexBuffer(const Device& device)
    {
        VkDeviceSize size = sizeof(Indices[0]) * Indices.size();

        // Create the actual index buffer with device local memory for better performance
        _indexBuffer = std::make_unique<Buffer>(device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

        // upload indices data to buffer
        Utils::uploadToDeviceBuffer(device, *_indexBuffer, size, Indices.data());
    }

	std::unique_ptr<Mesh> Mesh::createCube(const glm::vec3& color)
	{
		auto mesh = std::make_unique<Mesh>();

		// Define 8 unique cube vertices (positions, normals, colors, texCoords)
		// Cube is centered at origin, size 1
		const glm::vec3 positions[8] =
		{
			{-0.5f, -0.5f, -0.5f}, // 0
			{ 0.5f, -0.5f, -0.5f}, // 1
			{ 0.5f,  0.5f, -0.5f}, // 2
			{-0.5f,  0.5f, -0.5f}, // 3
			{-0.5f, -0.5f,  0.5f}, // 4
			{ 0.5f, -0.5f,  0.5f}, // 5
			{ 0.5f,  0.5f,  0.5f}, // 6
			{-0.5f,  0.5f,  0.5f}  // 7
		};
		const glm::vec3 colors[8] =
		{
			color, color, color, color,
			color, color, color, color
		};
		const glm::vec2 texCoords[4] =
		{
			{0.0f, 0.0f},
			{1.0f, 0.0f},
			{1.0f, 1.0f},
			{0.0f, 1.0f}
		};
		// Each face has its own normal
		const glm::vec3 normals[6] = {
			{ 0, 1, 0}, // back
			{ 0, -1, 0}, // front
			{ 0, 0, -1}, // bottom
			{ 0, 0, 1}, // top
			{-1, 0, 0}, // left
			{ 1, 0, 0}  // right
		};

		// 6 faces, 2 triangles per face, 3 vertices per triangle = 36 vertices
		// We'll duplicate vertices for correct normals/texCoords
		struct Face {
			int v[4]; // indices into positions/colors
			int normalIdx;
		};
		const Face faces[6] =
		{
			{{2, 3, 7, 6}, 0}, // back
			{{0, 1, 5, 4}, 1}, // front
			{{3, 2, 1, 0}, 2}, // bottom
			{{4, 5, 6, 7}, 3}, // top
			{{3, 0, 4, 7}, 4}, // left
			{{1, 2, 6, 5}, 5}  // right
		};

		for (int f = 0; f < 6; ++f)
		{
			const Face& face = faces[f];
			// 2 triangles per face
			int idx[4] = {face.v[0], face.v[1], face.v[2], face.v[3]};
			glm::vec3 normal = normals[face.normalIdx];
			// Triangle 1
			mesh->Vertices.push_back({positions[idx[0]], colors[idx[0]], normal, texCoords[0]});
			mesh->Vertices.push_back({positions[idx[1]], colors[idx[1]], normal, texCoords[1]});
			mesh->Vertices.push_back({positions[idx[2]], colors[idx[2]], normal, texCoords[2]});
			// Triangle 2
			mesh->Vertices.push_back({positions[idx[2]], colors[idx[2]], normal, texCoords[2]});
			mesh->Vertices.push_back({positions[idx[3]], colors[idx[3]], normal, texCoords[3]});
			mesh->Vertices.push_back({positions[idx[0]], colors[idx[0]], normal, texCoords[0]});
			// Indices (sequential)
			int base = f * 6;
			mesh->Indices.push_back(base + 0);
			mesh->Indices.push_back(base + 1);
			mesh->Indices.push_back(base + 2);
			mesh->Indices.push_back(base + 3);
			mesh->Indices.push_back(base + 4);
			mesh->Indices.push_back(base + 5);
		}

		return mesh;
	}

	std::unique_ptr<Mesh> Mesh::createQuad(const glm::vec3& color)
	{
		auto mesh = std::make_unique<Mesh>();

		const glm::vec3 positions[4] =
		{
			{-10.f, -10.f, -.8f}, // 0
			{ 10.f, -10.f, -.8f}, // 1
			{ 10.f,  10.f, -.8f}, // 2
			{-10.f,  10.f, -.8f}, // 3
		};
		const glm::vec3 colors[4] =
		{
			color, color, color, color,
		};
		const glm::vec2 texCoords[4] =
		{
			{0.0f, 0.0f},
			{1.0f, 0.0f},
			{1.0f, 1.0f},
			{0.0f, 1.0f}
		};

		const glm::vec3 normal = { 0, 0, 1};

		mesh->Vertices.push_back({positions[0], colors[0], normal, texCoords[0]});
		mesh->Vertices.push_back({positions[1], colors[1], normal, texCoords[1]});
		mesh->Vertices.push_back({positions[2], colors[2], normal, texCoords[2]});
		mesh->Vertices.push_back({positions[3], colors[3], normal, texCoords[3]});

		mesh->Indices.push_back(0);
		mesh->Indices.push_back(1);
		mesh->Indices.push_back(2);
		mesh->Indices.push_back(2);
		mesh->Indices.push_back(3);
		mesh->Indices.push_back(0);

		return mesh;
	}
}