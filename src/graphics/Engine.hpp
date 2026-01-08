#pragma once

#include "Window.hpp"
#include "Instance.hpp"
#include "Device.hpp"
#include "SwapChain.hpp"
#include "Descriptor.hpp"
#include "Pipeline.hpp"
#include "Buffer.hpp"
#include "Texture.hpp"
#include "geometry/Material.hpp"
#include "Camera.hpp"

#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

namespace m1
{
    class SceneObject;

    class Engine
    {
    public:
        static constexpr int FRAMES_IN_FLIGHT = 2;
    	static constexpr auto DEFAULT_PIPELINE = PipelineType::PhongLighting;
        static constexpr int PARTICLES_COUNT = 8192;

        Engine();
        ~Engine();

        void run();
        void addSceneObject(std::unique_ptr<SceneObject> obj);
    	void addMaterial(std::unique_ptr<Material> material);
    	void compile();

    private:
        void mainLoop();
        void drawFrame();
        void updateFrameUbo();
        void updateObjectUbo(const SceneObject &sceneObject);
        void createSyncObjects();
        void drawObjectsLoop(VkCommandBuffer commandBuffer);
    	void drawParticles(VkCommandBuffer commandBuffer);
        void recordDrawSceneCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex);
        void recordComputeCommands(VkCommandBuffer commandBuffer);
        void recreateSwapChain();
    	void createPipelines();
		void createUniformBuffers();
        void initParticles();
        void initLights();
    	void compileSceneObjects();
    	void compileMaterials();
        
        void copyBufferToImage(const Buffer& srcBuffer, VkImage image, uint32_t width, uint32_t height);
        void createTextureImage();

        void processInput(float delta);

		// TODO: move these methods to Image class?
        void transitionImageLayout(const Image& image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout);
        void generateMipmaps(const Image& image);

        const uint32_t  WIDTH = 800;
        const uint32_t  HEIGHT = 600;

        const std::string TEXTURE_PATH = "../resources/viking_room.png";

        Camera camera{};

        Window _window{ WIDTH, HEIGHT, "Vulkan App" };
        Device _device{ _window };
        std::unique_ptr<SwapChain> _swapChain;
    	std::unordered_map<PipelineType, std::unique_ptr<Pipeline>> _graphicsPipelines;
        std::unique_ptr<Pipeline> _computePipeline;
        std::vector<VkCommandBuffer> _drawSceneCmdBuffers;
        std::vector<VkCommandBuffer> _computeCmdBuffers;

        std::unique_ptr<Texture> _texture;

		std::vector<std::unique_ptr<Buffer>> _frameUboBuffers;
    	std::vector<FrameUbo> _frameUbos;
    	std::vector<std::unique_ptr<Buffer>> _objectUboBuffers;
    	std::vector<ObjectUbo> _objectUbos;
    	std::unique_ptr<Buffer> _lightsUboBuffer;
		std::unique_ptr<Descriptor> _descriptor;
    	std::vector<std::unique_ptr<Buffer>> _materialDynUboBuffers;
    	std::vector<MaterialUbo> _materialUbos;
    	VkDeviceSize _materialUboAlignment = -1;

    	std::vector<std::unique_ptr<Buffer>> _particleSSboBuffers;
		
        std::vector<std::unique_ptr<SceneObject>> _sceneObjects{};
        std::unordered_map<std::string, std::unique_ptr<Material>> _materials{};
    	uint64_t _currentMaterialUboIndex = -1;
        uint32_t _currentFrame = 0;

		// Synchronization objects (semaphores for GPU-GPU sync, fences for CPU-GPU sync)
        std::vector<VkSemaphore> _imageAvailableSems;
        std::vector<VkSemaphore> _drawCmdExecutedSems;
        std::vector<VkFence> _frameFences;
        VkSemaphore _acquireSemaphore; // only used during acquiring of an image, then swapped into _imageAvailableSems
    	std::vector<VkSemaphore> _computeCmdExecutedSems;
    	std::vector<VkFence> _computeFences;
    };
}
