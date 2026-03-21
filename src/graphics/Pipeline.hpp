#pragma once

//libs
#include <vulkan/vulkan.h>
#include "glm_config.hpp"

// std
#include <memory>
#include <vector>
#include <string>

#include "Vertex.hpp"

namespace m1
{
	class Device; // Forward declaration
	class SwapChain; // Forward declaration

	enum class PipelineType
	{
		NoLight,
		PhongLighting,
		ShadowMapping,
		PbrLighting,
		Particles,
		EquirectToCubemap,
		SkyBox,
	};

	struct PushConstantData
	{
		glm::mat4 model;
		alignas(16) glm::mat3 normalMatrix; // https://vulkan-tutorial.com/Uniform_buffers/Descriptor_pool_and_sets#page_Alignment-requirements
	};

	struct SkyBoxPushConstantData
	{
		glm::mat4 projection;
		glm::mat4 view;
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

	class GraphicsPipelineBuilder
	{
	private:
		std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;
		std::vector<std::string> _shaderPaths;

		VkPipelineViewportStateCreateInfo _viewportState
		{
			.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
			.viewportCount = 1, // specifies only the count since is dynamic state
			.scissorCount  = 1  // specifies only the count since is dynamic state
		};

		// assembly info: primitive topology
		VkPipelineInputAssemblyStateCreateInfo _inputAssembly
		{
			.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
			.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.primitiveRestartEnable = VK_FALSE
		};

		// rasterization info: how to convert the vertex data into fragments
		VkPipelineRasterizationStateCreateInfo _rasterization
		{
			.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
			.depthClampEnable        = VK_FALSE,
			.rasterizerDiscardEnable = VK_FALSE, // if enabled, discard all the fragments, useful for debugging
			.polygonMode             = VK_POLYGON_MODE_FILL,
			.cullMode                = VK_CULL_MODE_BACK_BIT,
			.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE, // If the projection matrix includes a Y-flip, the order of the vertices is inverted
			.depthBiasEnable         = VK_FALSE,
			.depthBiasConstantFactor = 0.0f, // Optional
			.depthBiasClamp          = 0.0f, // Optional
			.depthBiasSlopeFactor    = 0.0f, // Optional
			.lineWidth               = 1.0f,
		};

		// multisampling info: how to handle multisampling (anti-aliasing)
		VkPipelineMultisampleStateCreateInfo _multisample
		{
			.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
			.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT,
			.sampleShadingEnable   = VK_FALSE, // if enabled, better quality but an additional performance cost
			.minSampleShading      = 0.2f,     // min fraction for sample shading; closer to one is smoother
			.pSampleMask           = nullptr,  // Optional
			.alphaToCoverageEnable = VK_FALSE, // Optional
			.alphaToOneEnable      = VK_FALSE, // Optional
		};

		VkPipelineDepthStencilStateCreateInfo _depthStencil
		{
			.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
			.depthTestEnable       = VK_TRUE,
			.depthWriteEnable      = VK_TRUE,
			.depthCompareOp        = VK_COMPARE_OP_LESS,
			.depthBoundsTestEnable = VK_FALSE, // if true, keep fragments that fall within the specified depth range
			.stencilTestEnable     = VK_FALSE,
			.front                 = {}, // Optional
			.back                  = {}, // Optional
			.minDepthBounds        = 0.0f, // Optional
			.maxDepthBounds        = 1.0f, // Optional
		};

		// color blending info: per attached framebuffer
		std::vector<VkPipelineColorBlendAttachmentState> _colorBlendAttachments
		{
			/* The color blending operation is defined as follows pseudo code:

			if (blendEnable) {
				finalColor.rgb = (srcColorBlendFactor * newColor.rgb) <colorBlendOp> (dstColorBlendFactor * oldColor.rgb);
				finalColor.a = (srcAlphaBlendFactor * newColor.a) <alphaBlendOp> (dstAlphaBlendFactor * oldColor.a);
			} else {
				finalColor = newColor;
			}

			finalColor = finalColor & colorWriteMask;
			*/
			VkPipelineColorBlendAttachmentState
			{
				.blendEnable = true,
				.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA,
				.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
				.colorBlendOp = VK_BLEND_OP_ADD,
				.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
				.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
				.alphaBlendOp = VK_BLEND_OP_ADD,
				.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT,
			}
		};

		// dynamic state: will be specified at drawing time
		std::vector<VkDynamicState> _dynamicStates{VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};

		std::vector<VkDescriptorSetLayout> _setLayouts{};

		std::vector<VkPushConstantRange> _pushConstantRanges
		{
			VkPushConstantRange
			{
				.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, // shaders that can access the push constants
				.offset     = 0, // mainly for if using separate ranges for vertex and fragments shaders
				.size       = sizeof(PushConstantData),
			}
		};

		// rendering info: color and depth attachments
		std::vector<VkFormat> _colorAttachmentFormats{};
		VkPipelineRenderingCreateInfo _rendering
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
			.colorAttachmentCount = 0,
			.pColorAttachmentFormats = nullptr,
			.depthAttachmentFormat = VK_FORMAT_UNDEFINED,
		};

	public:
		GraphicsPipelineBuilder& addShaderStage(const std::string& shaderPath, VkShaderStageFlagBits stage, const char* entryPoint = "main");

		GraphicsPipelineBuilder& setViewportState(uint32_t viewportCount, uint32_t scissorCount);

		GraphicsPipelineBuilder& setPrimitiveTopology(VkPrimitiveTopology topology);

		GraphicsPipelineBuilder& setRasterizationState(VkPolygonMode polygonMode = VK_POLYGON_MODE_FILL,
			VkCullModeFlags cullMode = VK_CULL_MODE_BACK_BIT, VkFrontFace frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE);


		GraphicsPipelineBuilder& setPolygonMode(VkPolygonMode polygonMode);

		GraphicsPipelineBuilder& setCullModeFlags(VkCullModeFlags cullMode);

		GraphicsPipelineBuilder& setFrontFace(VkFrontFace frontFace);

		GraphicsPipelineBuilder& setSamples(VkSampleCountFlagBits rasterizationSamples);
		GraphicsPipelineBuilder& setDepthStencilState(VkBool32 depthTestEnable = VK_TRUE, VkBool32 depthWriteEnable = VK_TRUE,
			VkCompareOp depthCompareOp = VK_COMPARE_OP_LESS);

		GraphicsPipelineBuilder& enableDepthTest();
		GraphicsPipelineBuilder& disableDepthTest();
		GraphicsPipelineBuilder& enableDepthWrite();

		GraphicsPipelineBuilder& disableDepthWrite();

		GraphicsPipelineBuilder& setDepthCompareOp(VkCompareOp compareOp);

		GraphicsPipelineBuilder& enableBlend();

		GraphicsPipelineBuilder& disableBlend();
		GraphicsPipelineBuilder& clearDynamicStates();

		GraphicsPipelineBuilder& addDynamicState(VkDynamicState dynamicState);

		GraphicsPipelineBuilder& addSetLayout(VkDescriptorSetLayout descriptorSetLayout);

		GraphicsPipelineBuilder& clearPushConstantRanges();

		GraphicsPipelineBuilder& addPushConstantRange(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size);

		GraphicsPipelineBuilder& addColorAttachment(VkFormat format);

		GraphicsPipelineBuilder& setDepthAttachmentFormat(VkFormat format);

		GraphicsPipelineBuilder& setStencilAttachmentFormat(VkFormat format);

		/**
		 * Create the graphics pipeline.
		 */
		[[nodiscard]] std::unique_ptr<Pipeline> build(const Device& device);

		static VkShaderModule createShaderModule(const Device& device, const std::string& shaderPath);
	};

	class ComputePipelineBuilder
	{
		std::string _shaderPath;
		VkPipelineShaderStageCreateInfo _shaderStage
		{
			.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage = VK_SHADER_STAGE_COMPUTE_BIT,
			.pName = "main",
		};

		std::vector<VkDescriptorSetLayout> _setLayouts{};
		std::vector<VkPushConstantRange> _pushConstantRanges{};

	public:
		ComputePipelineBuilder(){}

		ComputePipelineBuilder& setShader(const std::string& shaderPath);
		ComputePipelineBuilder& addSetLayout(VkDescriptorSetLayout descriptorSetLayout);
		ComputePipelineBuilder& addPushConstantRange(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size);

		std::unique_ptr<Pipeline> build(const Device& device);
	};
}