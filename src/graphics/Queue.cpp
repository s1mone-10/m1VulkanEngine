#include "Queue.hpp"
#include "Device.hpp"
#include "log/Log.hpp"

namespace m1
{
    Queue::Queue(const Device& device, uint32_t familyIndex, uint32_t queueIndex)
        : _device(device)
    {
        Log::Get().Info("Creating queue");
        vkGetDeviceQueue(_device.getVkDevice(), familyIndex, queueIndex, &_queue);

        _commandPool = std::make_unique<CommandPool>(_device, familyIndex, VK_COMMAND_POOL_CREATE_TRANSIENT_BIT);
        _persistentCommandPool = std::make_unique<CommandPool>(_device, familyIndex, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);
    }

    Queue::~Queue()
    {
        Log::Get().Info("Destroying queue");
    }

    VkCommandBuffer Queue::beginOneTimeCommand() const
    {
        // Allocate info
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = _commandPool->getVkCommandPool();
        allocInfo.commandBufferCount = 1;

		// Allocate the command buffer
        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(_device.getVkDevice(), &allocInfo, &commandBuffer);

		// Command buffer begin info
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT; // one time submit

		// Begin the command buffer
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        return commandBuffer;
    }

    void Queue::endOneTimeCommand(VkCommandBuffer commandBuffer) const
    {
		// End recording the command buffer
        vkEndCommandBuffer(commandBuffer);

        // Submit the command buffer to the graphics queue
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        vkQueueSubmit(_queue, 1, &submitInfo, VK_NULL_HANDLE);

        // TODO: now one operation executed synchronously.
        // Combine these operations in a single command buffer and execute them asynchronously

		// Wait for the operations to finish
        vkQueueWaitIdle(_queue);

		// Free the command buffer
        vkFreeCommandBuffers(_device.getVkDevice(), _commandPool->getVkCommandPool(), 1, &commandBuffer);
    }
} // namespace m1
