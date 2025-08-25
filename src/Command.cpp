#include "Command.hpp"
#include "Device.hpp"
#include "log/Log.hpp"
#include <stdexcept>
#include <iostream>

namespace va {

Command::Command(const Device& device, int framesInFlight) : _device(device), _framesInFlight(framesInFlight) {
    Log::Get().Info("Creating command pool and buffers");
    createCommandPool();
    createCommandBuffers();
}

Command::~Command() {
    vkDestroyCommandPool(_device.getVkDevice(), _commandPool, nullptr);
    Log::Get().Info("Command pool destroyed");
}

void Command::createCommandPool() {
    QueueFamilyIndices queueFamilyIndices = _device.getQueueFamilyIndices();

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicsFamily.value();
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

    if (vkCreateCommandPool(_device.getVkDevice(), &poolInfo, nullptr, &_commandPool) != VK_SUCCESS) {
        Log::Get().Error("failed to create command pool!");
        throw std::runtime_error("failed to create command pool!");
    }
}

void Command::createCommandBuffers() {
    _commandBuffers.resize(_framesInFlight);

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = _commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = (uint32_t)_commandBuffers.size();

    if (vkAllocateCommandBuffers(_device.getVkDevice(), &allocInfo, _commandBuffers.data()) != VK_SUCCESS) {
        Log::Get().Error("failed to allocate command buffers!");
        throw std::runtime_error("failed to allocate command buffers!");
    }
}

} // namespace va
