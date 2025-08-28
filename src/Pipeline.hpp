#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

namespace m1 {
    class Device; // Forward declaration
    class SwapChain; // Forward declaration

    class Pipeline {
    public:
        Pipeline(const Device& device, const SwapChain& swapChain);
        ~Pipeline();

        // Non-copyable, non-movable
        Pipeline(const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;
        Pipeline(Pipeline&&) = delete;
        Pipeline& operator=(Pipeline&&) = delete;

        VkPipeline getVkPipeline() const { return _graphicsPipeline; }
        VkPipelineLayout getLayout() const { return _pipelineLayout; }
		VkDescriptorSetLayout getDescriptorSetLayout() const { return _descriptorSetLayout; }

    private:
        static std::vector<char> readFile(const std::string& filename);
        VkShaderModule createShaderModule(const Device& device, const std::vector<char>& code);
        void createGraphicsPipeline(const Device& device, const SwapChain& swapChain);
        void createDescriptorSetLayout();

        VkPipeline _graphicsPipeline = VK_NULL_HANDLE;
        VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;
        VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;

        const Device& _device;
    };
}
