#pragma once

#include "Window.hpp"
#include "Instance.hpp"
#include "Device.hpp"
#include "SwapChain.hpp"
#include "DescriptorSetManager.hpp"
#include "Pipeline.hpp"
#include "Buffer.hpp"
#include "Texture.hpp"
#include "Material.hpp"
#include "Camera.hpp"
#include "FrameData.hpp"
#include "geometry/BBox.hpp"

// std
#include <memory>
#include <vector>
#include <string>
#include <unordered_map>

namespace m1
{
    class SceneObject;

	struct EngineConfig
	{
		bool msaa = true;
		bool shadows = true;
	};

    class Engine
    {
    public:
        static constexpr int FRAMES_IN_FLIGHT = 2;
    	static constexpr auto DEFAULT_PIPELINE = PipelineType::PhongLighting;
        static constexpr int PARTICLES_COUNT = 8192;
        static constexpr auto DEFAULT_MATERIAL_NAME = "Default";

        Engine(EngineConfig config);
        ~Engine();

        void run();
        void addSceneObject(std::unique_ptr<SceneObject> obj);
    	void addMaterial(std::unique_ptr<Material> material);
    	void compile();
    	const EngineConfig& getConfig() const { return _config; }

    private:
        void mainLoop();
        void drawFrame();
        void updateFrameUbo();
        void updateObjectUbo(const SceneObject &sceneObject);
        void createSyncObjects();
        void drawObjectsLoop(VkCommandBuffer commandBuffer);
    	void drawParticles(VkCommandBuffer commandBuffer);
        void recordDrawSceneCommands(VkCommandBuffer commandBuffer, uint32_t swapChainImageIndex);
        void recordComputeCommands(VkCommandBuffer commandBuffer);
        void recreateSwapChain();
    	void createPipelines();
		void createFramesResources();
		void createShadowResources();
		void recordShadowMappingPass(VkCommandBuffer commandBuffer) const;
    	BBox computeSceneBBox() const;
        glm::mat4 computeLightViewProjMatrix() const;
        void initParticles();
        void initLights();
        void updateFrameDescriptorSet();
        void updateMaterialDescriptorSets(const Material &material);
    	void compileSceneObjects();
    	void compileMaterials();
        
        void copyBufferToImage(const Buffer& srcBuffer, VkImage image, uint32_t width, uint32_t height);

    	void createDefaultTexture();
        std::unique_ptr<Texture> loadTexture(const std::string &filePath);
        std::unique_ptr<Texture> createTexture(uint32_t width, uint32_t height, void *data);

        void processInput(float delta);

        void transitionImageLayout(const Image &image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspectMask) const;
        static void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, uint32_t mipLevels, VkImageLayout currentLayout, VkImageLayout newLayout, VkImageAspectFlags aspectMask);
        void generateMipmaps(const Image& image);
        static void copyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);

        const uint32_t  WIDTH = 800;
        const uint32_t  HEIGHT = 600;

    	EngineConfig _config{};

        Camera _camera{};

        Window _window{ WIDTH, HEIGHT, "Vulkan App" };
        Device _device{ _window };
        std::unique_ptr<SwapChain> _swapChain;
    	std::unordered_map<PipelineType, std::unique_ptr<Pipeline>> _graphicsPipelines;
        std::unique_ptr<Pipeline> _computePipeline;

    	std::vector<std::unique_ptr<FrameData>> _framesData;

    	// static lights -> just one buffer. If lights change at each frame, move them in the FrameResources
    	std::unique_ptr<Buffer> _lightsUboBuffer;
    	LightsUbo _lightsUbo{};

		std::unique_ptr<DescriptorSetManager> _descriptorSetManager;
    	VkDeviceSize _materialUboAlignment = -1;

        std::vector<std::unique_ptr<SceneObject>> _sceneObjects{};
    	BBox _bbox;
    	std::unordered_map<std::string, std::unique_ptr<Material>> _materials{};
    	std::unique_ptr<Material> _defaultMaterial = std::make_unique<Material>(DEFAULT_MATERIAL_NAME);
    	std::shared_ptr<Texture> _whiteTexture;
    	std::string _currentMaterialName;
        uint32_t _currentFrame = 0;

    	std::unique_ptr<Texture> _shadowMap;

		// Synchronization objects (semaphores for GPU-GPU sync, fences for CPU-GPU sync)
        std::vector<VkSemaphore> _imageAvailableSems;
        std::vector<VkSemaphore> _drawCmdExecutedSems;
        VkSemaphore _acquireSemaphore; // only used during acquiring of an image, then swapped into _imageAvailableSems
    };
}
