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

	enum class PipelineType
	{
		NoLight,
		PhongLighting,
		Particles
	};

    struct PushConstantData
    {
        glm::mat4 model;
        alignas(16) glm::mat3 normalMatrix; // https://vulkan-tutorial.com/Uniform_buffers/Descriptor_pool_and_sets#page_Alignment-requirements
    };

    struct GraphicsPipelineConfig
    {
    	// swap chain
    	const SwapChain& swapChain;

    	// shaders
        std::string vertShaderPath;
        std::string fragShaderPath;

    	// vertex binding and attributes
        VkVertexInputBindingDescription vertexBindingDescription;
        std::vector<VkVertexInputAttributeDescription> vertexAttributeDescriptions;

    	// fixed function
    	VkPrimitiveTopology topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    	VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL;
    	VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT;
    	VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    	// depth
    	bool depthTestEnable = true;
    	bool depthWriteEnable = true;

    	// blending
    	bool blendEnable = false;

    	// layouts
    	uint32_t setLayoutCount;
    	VkDescriptorSetLayout* pSetLayouts;
    };

    class Pipeline
	{
    public:
        Pipeline(const Device& device, VkPipeline pipeline, VkPipelineLayout pipelineLayout);
        ~Pipeline();

        //Non-copyable, non-movable
        Pipeline(const Pipeline&) = delete;
        Pipeline& operator=(const Pipeline&) = delete;
        Pipeline(Pipeline&&) = delete;
        Pipeline& operator=(Pipeline&&) = delete;

        VkPipeline getVkPipeline() const { return _pipeline; }
        VkPipelineLayout getLayout() const { return _pipelineLayout; }

    private:
        VkPipeline _pipeline = VK_NULL_HANDLE;
        VkPipelineLayout _pipelineLayout = VK_NULL_HANDLE;

    	const Device& _device;
    };

	class PipelineFactory
	{
	public:
		static std::unique_ptr<Pipeline> createGraphicsPipeline(const Device& device, const GraphicsPipelineConfig& config);
		static std::unique_ptr<Pipeline> createComputePipeline(const Device& device, const Descriptor& descriptor);

	private:
		static std::vector<char> readFile(const std::string& filename);
		static VkShaderModule createShaderModule(const Device& device, const std::vector<char>& code);
	};
}
