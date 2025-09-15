#pragma once

#include "Window.hpp"
#include "Instance.hpp"
#include "Device.hpp"
#include "SwapChain.hpp"
#include "Descriptor.hpp"
#include "Pipeline.hpp"
#include "Buffer.hpp"
#include "Texture.hpp"
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
        void createIndexBuffer(const std::vector<uint32_t>& indices);
		void createUniformBuffers();
        void copyBuffer(const Buffer& srcBuffer, const Buffer& dstBuffer, VkDeviceSize size);
        void copyBufferToImage(const Buffer& srcBuffer, VkImage image, uint32_t width, uint32_t height);
        void createTextureImage();

		// TODO: move these methods to Image class?
        void transitionImageLayout(const Image& image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
        void generateMipmaps(const Image& image);

        void loadModel();

        const uint32_t  WIDTH = 800;
        const uint32_t  HEIGHT = 600;

        const std::string MODEL_PATH = "../../../resources/viking_room.obj";
        const std::string TEXTURE_PATH = "../../../resources/viking_room.png";

        Window _window{ WIDTH, HEIGHT, "Vulkan App" };
        Device _device{_window};
        std::unique_ptr<SwapChain> _swapChain;
        std::unique_ptr<Pipeline> _pipeline;
        std::vector<VkCommandBuffer> _commandBuffers;

		std::unique_ptr<Buffer> _vertexBuffer;
        std::unique_ptr<Buffer> _indexBuffer;
		
        std::unique_ptr<Texture> _texture;

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
