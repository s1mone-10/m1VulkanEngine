#include "CommandPool.hpp"
#include "Device.hpp"
#include "Utils.hpp"
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

        VK_CHECK(vkCreateCommandPool(_device.getVkDevice(), &poolInfo, nullptr, &_commandPool));
    }

    std::vector<VkCommandBuffer> CommandPool::allocateCommandBuffers(int count) const
    {
        std::vector<VkCommandBuffer> commandBuffers(count);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = _commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = count;

        VK_CHECK(vkAllocateCommandBuffers(_device.getVkDevice(), &allocInfo, commandBuffers.data()));

        return commandBuffers;
    }

} // namespace m1
