#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace m1 {
    class Device;

    class CommandPool {
    public:
        CommandPool(const Device& device, uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags = 0);
        ~CommandPool();

        // Non-copyable, non-movable
        CommandPool(const CommandPool&) = delete;
        CommandPool& operator=(const CommandPool&) = delete;
        CommandPool(CommandPool&&) = delete;
        CommandPool& operator=(CommandPool&&) = delete;

        VkCommandPool getVkCommandPool() const { return _commandPool; }
        std::vector<VkCommandBuffer> allocateCommandBuffers(int count) const;

    private:
        void createCommandPool();

        VkCommandPool _commandPool = VK_NULL_HANDLE;

        const Device& _device;
        uint32_t _queueFamilyIndex;
        VkCommandPoolCreateFlags _flags;
    };
}
