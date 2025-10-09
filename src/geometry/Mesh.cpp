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

        // Create a staging buffer accessible to CPU to upload the vertex data
        Buffer stagingBuffer{ device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };

        // Copy vertex data to the staging buffer
        stagingBuffer.copyDataToBuffer((void*)Vertices.data());

        // Create the actual vertex buffer with device local memory for better performance
        _vertexBuffer = std::make_unique<Buffer>(device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        Utils::copyBuffer(device, stagingBuffer, *_vertexBuffer, size);
    }

    void Mesh::createIndexBuffer(const Device& device)
    {
        VkDeviceSize size = sizeof(Indices[0]) * Indices.size();

        // Create a staging buffer accessible to CPU to upload the vertex data
        Buffer stagingBuffer{ device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };

        // Copy indices data to the staging buffer
        stagingBuffer.copyDataToBuffer((void*)Indices.data());

        // Create the actual index buffer with device local memory for better performance
        _indexBuffer = std::make_unique<Buffer>(device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        Utils::copyBuffer(device, stagingBuffer, *_indexBuffer, size);
    }
}