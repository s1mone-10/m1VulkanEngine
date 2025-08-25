#pragma once

#include "Window.hpp"
#include "Instance.hpp"
#include "Device.hpp"
#include "SwapChain.hpp"
#include "Pipeline.hpp"
#include "Command.hpp"
#include "Buffer.hpp"
#include "geometry/Mesh.hpp"

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
        void updateUniformBuffer(uint32_t currentImage);
        void createSyncObjects();
        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
		void recreateSwapChain();
        void createVertexBuffer(const std::vector<Vertex>& vertices);
        void createIndexxBuffer(const std::vector<uint16_t>& indices);
		void createUniformBuffers();
        void copyBuffer(const Buffer& srcBuffer, const Buffer& dstBuffer, VkDeviceSize size);
        void createDescriptorPool();
        void createDescriptorSets();

        const uint32_t  WIDTH = 800;
        const uint32_t  HEIGHT = 600;
        Window _window{ WIDTH, HEIGHT, "Vulkan App" };
        Device _device{_window};
        std::unique_ptr<SwapChain> _swapChain;
        std::unique_ptr<Pipeline> _pipeline;
        std::unique_ptr<Command> _command;
		std::unique_ptr<Buffer> _vertexBuffer;
        std::unique_ptr<Buffer> _indexBuffer;
		std::vector<std::unique_ptr<Buffer>> _uniformBuffers;
		VkDescriptorPool _descriptorPool;
        std::vector<VkDescriptorSet> _descriptorSets;
        

        Mesh mesh{};
        uint32_t _currentFrame = 0;

        // Synchronization objects
        std::vector<VkSemaphore> _imageAvailableSemaphores;
        std::vector<VkSemaphore> _renderFinishedSemaphores;
        std::vector<VkFence> _inFlightFences;
    };
}
