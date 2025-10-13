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

    void Mesh::draw(VkCommandBuffer commandBuffer)
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
        _vertexBuffer = std::make_unique<Buffer>(device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        // upload vertex data to buffer
        Utils::uploadToDeviceBuffer(device, *_vertexBuffer, size, (void*)Vertices.data());
    }

    void Mesh::createIndexBuffer(const Device& device)
    {
        VkDeviceSize size = sizeof(Indices[0]) * Indices.size();

        // Create the actual index buffer with device local memory for better performance
        _indexBuffer = std::make_unique<Buffer>(device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        // upload indices data to buffer
        Utils::uploadToDeviceBuffer(device, *_indexBuffer, size, (void*)Indices.data());
    }
}