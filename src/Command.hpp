#pragma once

#include <vulkan/vulkan.h>
#include <vector>

namespace m1 {
    class Device;

    class Command {
    public:
        Command(const Device& device, int framesInFlight);
        ~Command();

        // Non-copyable, non-movable
        Command(const Command&) = delete;
        Command& operator=(const Command&) = delete;
        Command(Command&&) = delete;
        Command& operator=(Command&&) = delete;

        VkCommandPool getVkCommandPool() const { return _commandPool; }
        const std::vector<VkCommandBuffer>& getCommandBuffers() const { return _commandBuffers; }
        VkCommandBuffer beginOneTimeCommand() const;
        void endOneTimeCommand(VkCommandBuffer commandBuffer) const;

    private:
        void createCommandPool();
        void createCommandBuffers();

        VkCommandPool _commandPool = VK_NULL_HANDLE;
        std::vector<VkCommandBuffer> _commandBuffers;

        const Device& _device;
        int _framesInFlight;
    };
}
