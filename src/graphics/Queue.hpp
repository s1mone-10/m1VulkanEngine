#pragma once

#include "CommandPool.hpp"
#include <vulkan/vulkan.h>
#include <memory>

namespace m1
{
    class Device;

    class Queue
    {
    public:
        Queue(const Device& device, uint32_t familyIndex, uint32_t queueIndex);
        ~Queue();

        // Non-copyable, non-movable
        Queue(const Queue&) = delete;
        Queue& operator=(const Queue&) = delete;
        Queue(Queue&&) = delete;
        Queue& operator=(Queue&&) = delete;

        VkQueue getVkQueue() const { return _queue; }
        const CommandPool& getCommandPool() const { return *_commandPool; }
        const CommandPool& getPersistentCommandPool() const { return *_persistentCommandPool; }
        VkCommandBuffer beginOneTimeCommand() const;
        void endOneTimeCommand(VkCommandBuffer commandBuffer) const;

    private:
        VkQueue _queue = VK_NULL_HANDLE;
        std::unique_ptr<CommandPool> _commandPool;
        std::unique_ptr<CommandPool> _persistentCommandPool;

        const Device& _device;
    };
} // namespace m1
