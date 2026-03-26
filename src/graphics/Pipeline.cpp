#include "Pipeline.hpp"
#include "Device.hpp"
#include "Utils.hpp"
#include "Vertex.hpp"
#include "Log.hpp"

namespace m1
{
	VkShaderModule createShaderModule(const Device& device, const std::string& shaderPath)
	{
		const std::vector<char>& code = readFile(shaderPath);

		// ShaderModule info
		VkShaderModuleCreateInfo createInfo{};
		createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		createInfo.codeSize = code.size();
		createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		// create the ShaderModule
		VkShaderModule shaderModule;
		VK_CHECK(vkCreateShaderModule(device.getVkDevice(), &createInfo, nullptr, &shaderModule));

		return shaderModule;
	}

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::addShaderStage(const std::string& shaderPath, VkShaderStageFlagBits stage,
		const char* entryPoint)
	{
		VkPipelineShaderStageCreateInfo shaderStageInfo
		{
			.sType               = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
			.stage               = stage,
			.pName               = entryPoint,
			.pSpecializationInfo = nullptr // Optional, used for specialization constants
		};

		_shaderStages.push_back(shaderStageInfo);
		_shaderPaths.push_back(shaderPath);

		return *this;
	}

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::setViewportState(uint32_t viewportCount, uint32_t scissorCount)
	{
		_viewportState.viewportCount = viewportCount;
		_viewportState.scissorCount = scissorCount;
		return *this;
	}

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::clearVertexInput()
	{
		_vertexInput.vertexBindingDescriptionCount = 0;
		_vertexInput.pVertexBindingDescriptions = nullptr;
		_vertexInput.vertexAttributeDescriptionCount = 0;
		_vertexInput.pVertexAttributeDescriptions = nullptr;
		return *this;
	}

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::setPrimitiveTopology(VkPrimitiveTopology topology)
	{
		_inputAssembly.topology = topology;
		return *this;
	}

	//--------- RASTERIZATION ----------//

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::setRasterizationState(VkPolygonMode polygonMode, VkCullModeFlags cullMode,
		VkFrontFace frontFace)
	{
		_rasterization.polygonMode = polygonMode;
		_rasterization.cullMode = cullMode;
		_rasterization.frontFace = frontFace;
		return *this;
	}

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::setPolygonMode(VkPolygonMode polygonMode)
	{
		_rasterization.polygonMode = polygonMode;
		return *this;
	}

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::setCullModeFlags(VkCullModeFlags cullMode)
	{
		_rasterization.cullMode = cullMode;
		return *this;
	}

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::setFrontFace(VkFrontFace frontFace)
	{
		_rasterization.frontFace = frontFace;
		return *this;
	}

	//--------- MULTISAMPLE ----------//

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::setSamples(VkSampleCountFlagBits rasterizationSamples)
	{
		_multisample.rasterizationSamples = rasterizationSamples;
		return *this;
	}

	//--------- DEPTH - STENCIL ----------//

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::setDepthStencilState(VkBool32 depthTestEnable, VkBool32 depthWriteEnable,
		VkCompareOp depthCompareOp)
	{
		_depthStencil.depthTestEnable = depthTestEnable;
		_depthStencil.depthWriteEnable = depthWriteEnable;
		_depthStencil.depthCompareOp = depthCompareOp;
		return *this;
	}

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::enableDepthTest()
	{
		_depthStencil.depthTestEnable = VK_TRUE;
		return *this;
	}

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::disableDepthTest()
	{
		_depthStencil.depthTestEnable = VK_FALSE;
		return *this;
	}

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::enableDepthWrite()
	{
		_depthStencil.depthWriteEnable = VK_TRUE;
		return *this;
	}

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::disableDepthWrite()
	{
		_depthStencil.depthWriteEnable = VK_FALSE;
		return *this;
	}

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::setDepthCompareOp(VkCompareOp compareOp)
	{
		_depthStencil.depthCompareOp = compareOp;
		return *this;
	}

	//--------- COLOR BLENDING ----------//

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::enableBlend()
	{
		for (auto& colorBlendAttachment: _colorBlendAttachments)
			colorBlendAttachment.blendEnable = VK_TRUE;

		return *this;
	}

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::disableBlend()
	{
		for (auto& colorBlendAttachment: _colorBlendAttachments)
			colorBlendAttachment.blendEnable = VK_FALSE;

		return *this;
	}

	//--------- DYNAMIC STATES ----------//

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::clearDynamicStates()
	{
		_dynamicStates.clear();
		return *this;
	}

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::addDynamicState(VkDynamicState dynamicState)
	{
		_dynamicStates.push_back(dynamicState);
		return *this;
	}

	//--------- PUSH CONSTANT ----------//

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::addSetLayout(VkDescriptorSetLayout descriptorSetLayout)
	{
		_setLayouts.push_back(descriptorSetLayout);
		return *this;
	}

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::clearPushConstantRanges()
	{
		_pushConstantRanges.clear();
		return *this;
	}

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::addPushConstantRange(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size)
	{
		VkPushConstantRange pushConstantRange
		{
			.stageFlags = stageFlags,
			.offset     = offset,
			.size       = size
		};

		_pushConstantRanges.push_back(pushConstantRange);
		return *this;
	}

	//--------- RENDERING ----------//

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::addColorAttachment(VkFormat format)
	{
		_colorAttachmentFormats.push_back(format);
		return *this;
	}

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::setDepthAttachmentFormat(VkFormat format)
	{
		_rendering.depthAttachmentFormat = format;
		return *this;
	}

	GraphicsPipelineBuilder& GraphicsPipelineBuilder::setStencilAttachmentFormat(VkFormat format)
	{
		_rendering.stencilAttachmentFormat = format;
		return *this;
	}

	/**
	 * Create the graphics pipeline.
	 */
	std::unique_ptr<Pipeline> GraphicsPipelineBuilder::build(const Device& device)
	{
		for (size_t i = 0; i < _shaderPaths.size(); i++)
		{
			VkShaderModule shaderModule = createShaderModule(device, _shaderPaths[i]);

			_shaderStages[i].module = shaderModule;
		}

		VkPipelineDynamicStateCreateInfo dynamicState
		{
			.sType             = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
			.dynamicStateCount = static_cast<uint32_t>(_dynamicStates.size()),
			.pDynamicStates    = _dynamicStates.data()
		};

		// layout info: specify layout of dynamic values (descriptors and push constant) for shaders
		VkPipelineLayoutCreateInfo pipelineLayoutInfo
		{
			.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount         = static_cast<uint32_t>(_setLayouts.size()),
			.pSetLayouts            = _setLayouts.data(), // specify layout of descriptors for shaders
			.pushConstantRangeCount = static_cast<uint32_t>(_pushConstantRanges.size()),
			.pPushConstantRanges    = _pushConstantRanges.data(),
		};

		// crete pipeline layout
		VkPipelineLayout pipelineLayout;
		VK_CHECK(vkCreatePipelineLayout(device.getVkDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout));

		// color blending info: global settings
		VkPipelineColorBlendStateCreateInfo colorBlending
		{
			.sType           = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
			.logicOpEnable   = VK_FALSE,
			.logicOp         = VK_LOGIC_OP_COPY, // Optional,
			.attachmentCount = static_cast<uint32_t>(_colorBlendAttachments.size()),
			.pAttachments    = _colorBlendAttachments.data(),
			.blendConstants  = {0.0f, 0.0f, 0.0f, 0.0f} // Optional,
		};

		if (_colorAttachmentFormats.size() > 0)
		{
			_rendering.colorAttachmentCount = static_cast<uint32_t>(_colorAttachmentFormats.size());
			_rendering.pColorAttachmentFormats = _colorAttachmentFormats.data();
		}

		// pipeline info: all data configured above
		VkGraphicsPipelineCreateInfo pipelineInfo
		{
			.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,

			// append rendering info
			.pNext = &_rendering,

			// set shader, programmable stage,
			.stageCount = static_cast<uint32_t>(_shaderStages.size()),
			.pStages    = _shaderStages.data(),

			// set structures describing the fixed stage,
			.pVertexInputState   = &_vertexInput,
			.pInputAssemblyState = &_inputAssembly,
			.pViewportState      = &_viewportState,
			.pRasterizationState = &_rasterization,
			.pMultisampleState   = &_multisample,
			.pDepthStencilState  = &_depthStencil,
			.pColorBlendState    = &colorBlending,
			.pDynamicState       = &dynamicState,

			// set layout and render pas,
			.layout  = pipelineLayout,
			.subpass = 0,

			// optional. Vulkan allows you to create a new graphics pipeline by deriving from an existing pipeline
			.basePipelineHandle = VK_NULL_HANDLE, // Optional
			.basePipelineIndex  = -1              // Optional
		};

		// create the graphics pipeline
		VkPipeline graphicsPipeline;
		VK_CHECK(vkCreateGraphicsPipelines(device.getVkDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline));

		// destroy shader modules
		for (auto& _shaderStage: _shaderStages)
			vkDestroyShaderModule(device.getVkDevice(), _shaderStage.module, nullptr);

		return std::make_unique<Pipeline>(device, graphicsPipeline, pipelineLayout);
	}



	ComputePipelineBuilder& ComputePipelineBuilder::setShader(const std::string& shaderPath)
	{
		_shaderPath = shaderPath;
		return *this;
	}
	ComputePipelineBuilder& ComputePipelineBuilder::addSetLayout(VkDescriptorSetLayout descriptorSetLayout)
	{
		_setLayouts.push_back(descriptorSetLayout);
		return *this;
	}

	ComputePipelineBuilder& ComputePipelineBuilder::addPushConstantRange(VkShaderStageFlags stageFlags, uint32_t offset, uint32_t size)
	{
		VkPushConstantRange pushConstantRange
				{
					.stageFlags = stageFlags,
					.offset     = offset,
					.size       = size
				};

		_pushConstantRanges.push_back(pushConstantRange);
		return *this;
	}

	std::unique_ptr<Pipeline> ComputePipelineBuilder::build(const Device& device)
	{
		VkShaderModule shaderModule = createShaderModule(device, _shaderPath);
		_shaderStage.module = shaderModule;

		// layout info: specify layout of dynamic values (descriptors and push constant) for shaders
		VkPipelineLayoutCreateInfo pipelineLayoutInfo
		{
			.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			.setLayoutCount         = static_cast<uint32_t>(_setLayouts.size()),
			.pSetLayouts            = _setLayouts.data(), // specify layout of descriptors for shaders
			.pushConstantRangeCount = static_cast<uint32_t>(_pushConstantRanges.size()),
			.pPushConstantRanges    = _pushConstantRanges.data(),
		};

		VkPipelineLayout pipelineLayout;
		VK_CHECK(vkCreatePipelineLayout(device.getVkDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout));

		VkComputePipelineCreateInfo computePipelineInfo
		{
			.sType  = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
			.stage  = _shaderStage,
			.layout = pipelineLayout,
		};

		VkPipeline computePipeline;
		VK_CHECK(vkCreateComputePipelines(device.getVkDevice(), VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &computePipeline));

		vkDestroyShaderModule(device.getVkDevice(), _shaderStage.module, nullptr);

		return std::make_unique<Pipeline>(device, computePipeline, pipelineLayout);

	}

	Pipeline::Pipeline(const Device& device, VkPipeline pipeline, VkPipelineLayout pipelineLayout) : _pipeline(pipeline), _pipelineLayout(pipelineLayout),
		_device(device) {}

	Pipeline::~Pipeline()
	{
		vkDestroyPipeline(_device.getVkDevice(), _pipeline, nullptr);
		vkDestroyPipelineLayout(_device.getVkDevice(), _pipelineLayout, nullptr);
		Log::Get().Trace("Pipeline destroyed");
	}
} // namespace m1
