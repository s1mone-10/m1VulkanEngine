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
}