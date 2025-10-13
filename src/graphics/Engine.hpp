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
#include "Camera.hpp"

#include <memory>
#include <vector>

namespace m1
{
    struct SceneObject;

    class Engine
    {
    public:
        static const int FRAMES_IN_FLIGHT = 2;

        Engine();
        ~Engine();

        void run();
        void addSceneObject(std::unique_ptr<SceneObject> obj);

    private:
        void mainLoop();
        void drawFrame();
        void updateUniformBuffer(uint32_t currentImage);
        void createSyncObjects();
        void recordDrawCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex);
		void recreateSwapChain();
        
		void createUniformBuffers();
        void initLights();
        
        void copyBufferToImage(const Buffer& srcBuffer, VkImage image, uint32_t width, uint32_t height);
        void createTextureImage();

        void processInput(float delta);

		// TODO: move these methods to Image class?
        void transitionImageLayout(const Image& image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
        void generateMipmaps(const Image& image);

        const uint32_t  WIDTH = 800;
        const uint32_t  HEIGHT = 600;

        const std::string TEXTURE_PATH = "../../../resources/viking_room.png";

        Camera camera{};

        Window _window{ WIDTH, HEIGHT, "Vulkan App" };
        Device _device{ _window };
        std::unique_ptr<SwapChain> _swapChain;
        std::unique_ptr<Pipeline> _pipeline;
        std::vector<VkCommandBuffer> _drawSceneCmdBuffers;

        std::unique_ptr<Texture> _texture;

		std::vector<std::unique_ptr<Buffer>> _uniformBuffers;
        std::unique_ptr<Buffer> _lightsUbo;
		std::unique_ptr<Descritor> _descriptor;
		
        std::vector<std::unique_ptr<SceneObject>> _sceneObjects{};
        uint32_t _currentFrame = 0;

		// Synchronization objects (semaphores for GPU-GPU sync, fences for CPU-GPU sync)
        std::vector<VkSemaphore> _imageAvailableSems;
        std::vector<VkSemaphore> _drawCmdExecutedSems;
        std::vector<VkFence> _frameFences;
        VkSemaphore _acquireSemaphore; // only used during acquire of an image, then swapped into _imageAvailableSems
    };
}
