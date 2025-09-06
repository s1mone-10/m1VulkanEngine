#pragma once

#include "Window.hpp"
#include "Instance.hpp"
#include "Device.hpp"
#include "SwapChain.hpp"
#include "Descriptor.hpp"
#include "Pipeline.hpp"
#include "Command.hpp"
#include "Buffer.hpp"
#include "geometry/Mesh.hpp"

#include <memory>
#include <vector>

namespace m1
{
    class Engine
    {
    public:
        static const int FRAMES_IN_FLIGHT = 2;

        Engine();
        ~Engine();

        void run();

    private:
        void mainLoop();
        void drawFrame();
        void updateUniformBuffer(uint32_t currentImage);
        void createSyncObjects();
        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
		void recreateSwapChain();
        void createVertexBuffer(const std::vector<Vertex>& vertices);
        void createIndexBuffer(const std::vector<uint16_t>& indices);
		void createUniformBuffers();
        void copyBuffer(const Buffer& srcBuffer, const Buffer& dstBuffer, VkDeviceSize size);
        void copyBufferToImage(const Buffer& srcBuffer, VkImage image, uint32_t width, uint32_t height);
        void createTextureImage();
        void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);

        const uint32_t  WIDTH = 800;
        const uint32_t  HEIGHT = 600;
        Window _window{ WIDTH, HEIGHT, "Vulkan App" };
        Device _device{_window};
        std::unique_ptr<SwapChain> _swapChain;
        std::unique_ptr<Pipeline> _pipeline;
        std::unique_ptr<Command> _command;

		std::unique_ptr<Buffer> _vertexBuffer;
        std::unique_ptr<Buffer> _indexBuffer;
		
        VkImage _textureImage; // TODO create a Texture/Image class. See also images in the SwapChain
        VkDeviceMemory _textureImageMemory;
        VkImageView _textureImageView;
        VkSampler _textureSampler;

		std::vector<std::unique_ptr<Buffer>> _uniformBuffers;
		std::unique_ptr<Descritor> _descriptor;
		
        Mesh mesh{};
        uint32_t _currentFrame = 0;

        // Synchronization objects
        std::vector<VkSemaphore> _imageAvailableSemaphores;
        std::vector<VkSemaphore> _renderFinishedSemaphores;
        std::vector<VkFence> _inFlightFences;
    };
}
