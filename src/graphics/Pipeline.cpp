#include "Pipeline.hpp"
#include "Device.hpp"
#include "SwapChain.hpp"
#include "DescriptorSetManager.hpp"
#include "geometry/Vertex.hpp"
#include "log/Log.hpp"
#include <stdexcept>
#include <fstream>

namespace m1
{
    Pipeline::Pipeline(const Device& device, VkPipeline pipeline, VkPipelineLayout pipelineLayout) : _device(device), _pipeline(pipeline), _pipelineLayout(pipelineLayout)
    {
    }

    Pipeline::~Pipeline()
    {
        vkDestroyPipeline(_device.getVkDevice(), _pipeline, nullptr);
        vkDestroyPipelineLayout(_device.getVkDevice(), _pipelineLayout, nullptr);
        Log::Get().Info("Pipeline destroyed");
    }

    std::vector<char> PipelineFactory::readFile(const std::string& filename)
    {
        // ate: Start reading at the end of the file
        std::ifstream file(filename, std::ios::ate | std::ios::binary);

        if (!file.is_open())
        {
            Log::Get().Error("failed to open file: " + filename);
            throw std::runtime_error("failed to open file: " + filename);
        }

        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);

        file.seekg(0);
        file.read(buffer.data(), fileSize);

        file.close();

        return buffer;
    }

    VkShaderModule PipelineFactory::createShaderModule(const Device& device, const std::vector<char>& code)
    {
		// ShaderModule info
        VkShaderModuleCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        createInfo.codeSize = code.size();
        createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

		// create the ShaderModule
        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device.getVkDevice(), &createInfo, nullptr, &shaderModule) != VK_SUCCESS)
        {
            Log::Get().Error("failed to create shader module!");
            throw std::runtime_error("failed to create shader module!");
        }

        return shaderModule;
    }

    std::unique_ptr<Pipeline> PipelineFactory::createGraphicsPipeline(const Device& device, const GraphicsPipelineConfig& config)
    {
        Log::Get().Info("Creating graphics pipeline");
        // read shaders' code
        std::vector<char> vertShaderCode = readFile(config.vertShaderPath);
        std::vector<char> fragShaderCode = readFile(config.fragShaderPath);

        // wrap to shader modules
        VkShaderModule vertShaderModule = createShaderModule(device, vertShaderCode);
        VkShaderModule fragShaderModule = createShaderModule(device, fragShaderCode);

        // set info to assign the shaders to a specific pipeline stage
        VkPipelineShaderStageCreateInfo vertShaderStageInfo{};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT; // set the pipeline stage
        vertShaderStageInfo.module = vertShaderModule;
        vertShaderStageInfo.pName = "main"; // entry point
        vertShaderStageInfo.pSpecializationInfo = nullptr; // Optional, used for specialization constants

        VkPipelineShaderStageCreateInfo fragShaderStageInfo{};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = fragShaderModule;
        fragShaderStageInfo.pName = "main";
        vertShaderStageInfo.pSpecializationInfo = nullptr; // Optional, used for specialization constants

        VkPipelineShaderStageCreateInfo shaderStages[] = { vertShaderStageInfo, fragShaderStageInfo };

        // dynamic state: will be specified at drawing time
        std::vector<VkDynamicState> dynamicStates = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
        VkPipelineDynamicStateCreateInfo dynamicState{};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
        dynamicState.pDynamicStates = dynamicStates.data();

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1; // specifies only the count since is dynamic state
        viewportState.scissorCount = 1; // specifies only the count since is dynamic state

        // assembly info: topology
        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = config.topology;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        // vertex info: describes the format of the vertex data that will be passed to the vertex shader
        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &config.vertexBindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = static_cast<uint32_t>(config.vertexAttributeDescriptions.size());
        vertexInputInfo.pVertexAttributeDescriptions = config.vertexAttributeDescriptions.data();

        // rasterizer info: how to convert the vertex data into fragments
        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE; // if enabled, discard all the fragments, useful for debugging
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
		rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE; // If the projection matrix includes a Y-flip, the order of the vertices is inverted
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

        // multisampling info: how to handle multisampling (anti-aliasing)
        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE; // if enabled, better quality but an additional performance cost
        multisampling.rasterizationSamples = config.swapChain.getSamples();
        multisampling.minSampleShading = 0.2f; // min fraction for sample shading; closer to one is smoother
        multisampling.pSampleMask = nullptr; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional

		// depth and stencil info
        VkPipelineDepthStencilStateCreateInfo depthStencil{};
        depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
        depthStencil.depthTestEnable = config.depthTestEnable;
        depthStencil.depthWriteEnable = config.depthWriteEnable;
		depthStencil.depthCompareOp = VK_COMPARE_OP_LESS; // lower depth = closer
        depthStencil.depthBoundsTestEnable = VK_FALSE; // if true, keep fragments that fall within the specified depth range
        depthStencil.minDepthBounds = 0.0f; // Optional
        depthStencil.maxDepthBounds = 1.0f; // Optional
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {}; // Optional
        depthStencil.back = {}; // Optional

        // color blending info: per attached framebuffer
        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = config.blendEnable;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
        /* The color blending operation is defined as follows pseudo code:

            if (blendEnable) {
                finalColor.rgb = (srcColorBlendFactor * newColor.rgb) <colorBlendOp> (dstColorBlendFactor * oldColor.rgb);
                finalColor.a = (srcAlphaBlendFactor * newColor.a) <alphaBlendOp> (dstAlphaBlendFactor * oldColor.a);
            } else {
                finalColor = newColor;
            }

            finalColor = finalColor & colorWriteMask;
        */

        // color blending info: globlal settings
        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional

        // push constant
        VkPushConstantRange pushConstantRange
        {
            .stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, // shaders that can access the push constants
            .offset = 0, // mainly for if using separate ranges for vertex and fragments shaders
            .size = sizeof(PushConstantData)
        };

    	// layout info: specify layout of dynamic values (descriptors and push constant) for shaders
        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = config.setLayoutCount;
        pipelineLayoutInfo.pSetLayouts = config.pSetLayouts; // specify layout of descriptors for shaders
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushConstantRange;

		// crete pipeline layout
    	VkPipelineLayout pipelineLayout;
        if (vkCreatePipelineLayout(device.getVkDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
        {
            Log::Get().Error("failed to create pipeline layout!");
            throw std::runtime_error("failed to create pipeline layout!");
        }

        // pipeline info: all data configured above
        VkGraphicsPipelineCreateInfo pipelineInfo{
            .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
            
            // set shader, programmable stage,
            .stageCount = 2,
            .pStages = shaderStages,
            
            // set structures describing the fixed stage,
            .pVertexInputState = &vertexInputInfo,
            .pInputAssemblyState = &inputAssembly,
            .pViewportState = &viewportState,
            .pRasterizationState = &rasterizer,
            .pMultisampleState = &multisampling,
            .pDepthStencilState = &depthStencil,
            .pColorBlendState = &colorBlending,
            .pDynamicState = &dynamicState,
            
            // set layout and render pas,
            .layout = pipelineLayout,
            .renderPass = config.swapChain.getRenderPass(),
            .subpass = 0,

            // optional. Vulkan allows you to create a new graphics pipeline by deriving from an existing pipeline
            .basePipelineHandle = VK_NULL_HANDLE, // Optional
            .basePipelineIndex = -1 // Optional
        };

        // create the graphics pipeline
    	VkPipeline graphicsPipeline;
        if (vkCreateGraphicsPipelines(device.getVkDevice(), VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
        {
            Log::Get().Error("failed to create graphics pipeline!");
            throw std::runtime_error("failed to create graphics pipeline!");
        }

        vkDestroyShaderModule(device.getVkDevice(), fragShaderModule, nullptr);
        vkDestroyShaderModule(device.getVkDevice(), vertShaderModule, nullptr);

    	return std::make_unique<Pipeline>(device, graphicsPipeline, pipelineLayout);
    }

	std::unique_ptr<Pipeline> PipelineFactory::createComputePipeline(const Device& device, const VkDescriptorSetLayout& descriptorSetLayout)
	{
    	std::vector<char> compShaderCode = readFile(R"(..\shaders\compiled\particle.comp.spv)");
    	VkShaderModule compShaderModule = createShaderModule(device, compShaderCode);

    	VkPipelineShaderStageCreateInfo shaderStageInfo{};
    	shaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    	shaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    	shaderStageInfo.module = compShaderModule;
    	shaderStageInfo.pName = "main";

    	VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    	pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    	pipelineLayoutInfo.setLayoutCount = 1;
    	pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;

    	VkPipelineLayout pipelineLayout;
    	if (vkCreatePipelineLayout(device.getVkDevice(), &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS) {
    		throw std::runtime_error("failed to create compute pipeline layout!");
    	}

    	VkComputePipelineCreateInfo computePipelineInfo{};
    	computePipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
    	computePipelineInfo.layout = pipelineLayout;
    	computePipelineInfo.stage = shaderStageInfo;

    	VkPipeline computePipeline;
    	if (vkCreateComputePipelines(device.getVkDevice(), VK_NULL_HANDLE, 1, &computePipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
    		throw std::runtime_error("failed to create compute pipeline!");
    	}

    	vkDestroyShaderModule(device.getVkDevice(), compShaderModule, nullptr);

    	return std::make_unique<Pipeline>(device, computePipeline, pipelineLayout);
	}
} // namespace m1
