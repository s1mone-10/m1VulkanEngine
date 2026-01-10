#include "Utils.hpp"
#include "Queue.hpp"

namespace m1
{
    void Utils::copyBuffer(const Device& device, const Buffer& srcBuffer, const Buffer& dstBuffer, VkDeviceSize size)
    {
        // Memory transfer operations are executed using command buffers.
        
        // Begin one-time command
        VkCommandBuffer commandBuffer = device.getGraphicsQueue().beginOneTimeCommand();

        // Copy buffer
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0; // Optional
        copyRegion.dstOffset = 0; // Optional
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer.getVkBuffer(), dstBuffer.getVkBuffer(), 1, &copyRegion);

        // Execute the command
        device.getGraphicsQueue().endOneTimeCommand(commandBuffer);
    }

    void Utils::uploadToDeviceBuffer(const Device& device, const Buffer& dstBuffer, VkDeviceSize size, void* data)
    {
        // Create a staging buffer accessible to CPU to upload the data
        Buffer stagingBuffer{ device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };

        // Copy data to the staging buffer
        stagingBuffer.copyDataToBuffer(data);

        // Copy staging buffer to destination buffer
        copyBuffer(device, stagingBuffer, dstBuffer, size);
    }
}