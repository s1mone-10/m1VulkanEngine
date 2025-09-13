#include "CommandPool.hpp"
#include "Device.hpp"
#include "log/Log.hpp"
#include <stdexcept>
#include <iostream>

namespace m1
{

    CommandPool::CommandPool(const Device& device, uint32_t queueFamilyIndex, VkCommandPoolCreateFlags flags)
        : _device(device)
        , _queueFamilyIndex(queueFamilyIndex)
        , _flags(flags)
    {
        Log::Get().Info("Creating command pool");
        createCommandPool();
    }

    CommandPool::~CommandPool()
    {
        // Command buffers will be automatically freed when their command pool is destroyed
        vkDestroyCommandPool(_device.getVkDevice(), _commandPool, nullptr);
        Log::Get().Info("Command pool destroyed");
    }

    void CommandPool::createCommandPool()
    {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = _queueFamilyIndex;
        poolInfo.flags = _flags;

        if (vkCreateCommandPool(_device.getVkDevice(), &poolInfo, nullptr, &_commandPool) != VK_SUCCESS)
        {
            Log::Get().Error("failed to create command pool!");
            throw std::runtime_error("failed to create command pool!");
        }
    }

    std::vector<VkCommandBuffer> CommandPool::createCommandBuffers(int count) const
    {
        std::vector<VkCommandBuffer> commandBuffers(count);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = _commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = count;

        if (vkAllocateCommandBuffers(_device.getVkDevice(), &allocInfo, commandBuffers.data()) != VK_SUCCESS)
        {
            Log::Get().Error("failed to allocate command buffers!");
            throw std::runtime_error("failed to allocate command buffers!");
        }

        return commandBuffers;
    }

} // namespace m1
