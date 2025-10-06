#pragma once

#include <vulkan/vulkan.h>
#include <vector>
#include <string>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/vec4.hpp>
#include <glm/mat4x4.hpp>

namespace m1 {
    class Device; // Forward declaration
    class SwapChain; // Forward declaration

    struct PushConstantData
    {
        glm::vec2 offset;
        alignas(16) glm::vec3 color; // https://vulkan-tutorial.com/Uniform_buffers/Descriptor_pool_and_sets#page_Alignment-requirements
    };

    class Pipeline {
    public:
        Pipeline(const Device& device, const SwapChain& swapChain, const VkDescriptorSetLayout descriptorSetLayout);
        ~Pipeline();

        // Non-copyable, non-movable
        Pipeline(const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;
        Pipeline(Pipeline&&) = delete;
        Pipeline& operator=(Pipeline&&) = delete;

        VkPipeline getVkPipeline() const { return _graphicsPipeline; }
        VkPipelineLayout getLayout() const { return _pipelineLayout; }

    private:
        static std::vector<char> readFile(const std::string& filename);
        VkShaderModule createShaderModule(const Device& device, const std::vector<char>& code);
        void createGraphicsPipeline(const Device& device, const SwapChain& swapChain, const VkDescriptorSetLayout descriptorSetLayout);
        void createDescriptorSetLayout();

        VkPipeline _graphicsPipeline = VK_NULL_HANDLE;
        VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;

        const Device& _device;
    };
}
