#pragma once

#include "Descriptor.hpp"

//libs
#include <vulkan/vulkan.h>
#include "glm_config.hpp"

// std
#include <vector>
#include <string>

namespace m1
{
    class Device; // Forward declaration
    class SwapChain; // Forward declaration

    struct PushConstantData
    {
        glm::mat4 model;
        alignas(16) glm::mat3 normalMatrix; // https://vulkan-tutorial.com/Uniform_buffers/Descriptor_pool_and_sets#page_Alignment-requirements
    };

    class Pipeline {
    public:
        Pipeline(const Device& device, const SwapChain& swapChain, const Descriptor& descriptor);
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
        void createGraphicsPipeline(const Device& device, const SwapChain& swapChain, const Descriptor& descriptor);
        void createDescriptorSetLayout();

        VkPipeline _graphicsPipeline = VK_NULL_HANDLE;
        VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;

        const Device& _device;
    };
}
