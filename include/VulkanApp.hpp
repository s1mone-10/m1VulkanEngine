#pragma once

#include <vulkan/vulkan.h>
#include <optional>
#include <vector>

#include "Window.hpp"

namespace va
{
    const int FRAMES_IN_FLIGHT = 2;

    struct QueueFamilyIndices {
        std::optional<uint32_t> graphicsFamily;
        std::optional<uint32_t> presentFamily;

        bool isComplete() { return graphicsFamily.has_value() && presentFamily.has_value(); }
    };

    class VulkanApp
    {
        struct SwapChainSupportDetails {
            VkSurfaceCapabilitiesKHR capabilities;
            std::vector<VkSurfaceFormatKHR> formats;
            std::vector<VkPresentModeKHR> presentModes;
        };

        const uint32_t  WIDTH = 800;
        const uint32_t  HEIGHT = 600;
        Window window{ WIDTH, HEIGHT, "Vulkan App" };

    public:
        void run();
        ~VulkanApp();

    private:
        VkInstance instance = VK_NULL_HANDLE;
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkDebugUtilsMessengerEXT debugMessenger;
        VkPhysicalDevice physicalDevice = VK_NULL_HANDLE; // implicitly destroyed when the VkInstance is destroyed
        QueueFamilyIndices qfIndices;
        VkDevice device = VK_NULL_HANDLE; // logical device
        VkQueue graphicsQueue; //Device queues are implicitly cleaned up when the device is destroyed
        VkQueue presentQueue;
		VkSwapchainKHR swapChain = VK_NULL_HANDLE;
        std::vector<VkImage> swapChainImages; // automatically cleaned up once the swap chain has been destroyed
        std::vector<VkImageView> swapChainImageViews;
        std::vector<VkFramebuffer> swapChainFramebuffers;
        VkFormat swapChainImageFormat;
        VkExtent2D swapChainExtent;
        VkRenderPass renderPass;
		VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
        VkPipeline graphicsPipeline;
        VkCommandPool commandPool;
        std::vector<VkCommandBuffer> commandBuffers;
        uint32_t currentFrame = 0;

		// Synchronization objects
		std::vector<VkSemaphore> imageAvailableSemaphores; // GPU-GPU synchronization
		std::vector<VkSemaphore> renderFinishedSemaphores; // GPU-GPU synchronization
		std::vector<VkFence> inFlightFences; // CPU-GPU synchronization
		

        void initVulkan();
        void mainLoop();
        void createInstance();
        void populateDebugInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo);
		void createDebugMessenger();
        bool checkValidationLayerSupport();
        std::vector<const char*> getRequiredExtensions();
        void pickPhysicalDevice();
        bool isDeviceSuitable(VkPhysicalDevice device);
        bool checkDeviceExtensionSupport(VkPhysicalDevice device);
        void initQueueFamilies(VkPhysicalDevice device);
        void createLogicalDevice();
        void createSwapChain();
        SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);
        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats);
        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes);
        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);
        void createImageViews();
        void createRenderPass();
        void createGraphicsPipeline();
        VkShaderModule createShaderModule(const std::vector<char>& code);
		void createFramebuffers();
        void createCommandPool();
		void createCommandBuffer();
        void recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
        void createSyncObjects();
		void drawFrame();
    };
}