#pragma once

#include "Window.hpp"
#include "Instance.hpp"
#include "Device.hpp"
#include "SwapChain.hpp"
#include "Pipeline.hpp"
#include "Command.hpp"

#include <memory>
#include <vector>

namespace va
{
    const int FRAMES_IN_FLIGHT = 2;

    class App
    {
    public:
        App();
        ~App();

        void run();

    private:
        void mainLoop();
        void drawFrame();
        void createSyncObjects();
        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
		void recreateSwapChain();

        const uint32_t  WIDTH = 800;
        const uint32_t  HEIGHT = 600;
        Window _window{ WIDTH, HEIGHT, "Vulkan App" };
        Device _device{_window};
        std::unique_ptr<SwapChain> _swapChain;
        std::unique_ptr<Pipeline> _pipeline;
        std::unique_ptr<Command> _command;

        uint32_t _currentFrame = 0;

        // Synchronization objects
        std::vector<VkSemaphore> _imageAvailableSemaphores;
        std::vector<VkSemaphore> _renderFinishedSemaphores;
        std::vector<VkFence> _inFlightFences;
    };
}
