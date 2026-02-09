#include "Engine.hpp"
#include "log/Log.hpp"
#include "Queue.hpp"
#include "SceneObject.hpp"
#include "Utils.hpp"
#include "geometry/Mesh.hpp"
#include "geometry/Particle.hpp"

//libs
#include "glm_config.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// std
#include <array>
#include <stdexcept>
#include <vector>
#include <chrono>
#include <random>
#include <ranges>

namespace m1
{
	Engine::Engine(EngineConfig config) : _engineConfig(config)
	{
		Log::Get().Info("Engine constructor");

		recreateSwapChain();
		_descriptorSetManager = std::make_unique<DescriptorSetManager>(_device);
		createPipelines();

		_materialUboAlignment = _device.getUniformBufferAlignment(sizeof(MaterialUbo));
		createFramesResources();
		createDefaultTexture();
		initLights();
		initParticles();
		updateFrameDescriptorSet();

		createSyncObjects();
	}

	Engine::~Engine()
	{
		// wait for the GPU to finish all operations before destroying the resources
		vkDeviceWaitIdle(_device.getVkDevice());

		// Command buffers are implicitly destroyed when the command pool is destroyed

		for (size_t i = 0; i < _imageAvailableSems.size(); i++)
		{
			vkDestroySemaphore(_device.getVkDevice(), _drawCmdExecutedSems[i], nullptr);
			vkDestroySemaphore(_device.getVkDevice(), _imageAvailableSems[i], nullptr);
		}
		vkDestroySemaphore(_device.getVkDevice(), _acquireSemaphore, nullptr);

		for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			vkDestroyFence(_device.getVkDevice(), _framesData[i]->drawCmdExecutedFence, nullptr);
			vkDestroyFence(_device.getVkDevice(), _framesData[i]->computeCmdExecutedFence, nullptr);
			vkDestroySemaphore(_device.getVkDevice(), _framesData[i]->computeCmdExecutedSem, nullptr);
		}

		Log::Get().Info("Engine destroyed");
	}

	void Engine::run()
	{
		mainLoop();
	}

	void Engine::addSceneObject(std::unique_ptr<SceneObject> obj)
	{
		_sceneObjects.push_back(std::move(obj));
	}

	void Engine::addMaterial(std::unique_ptr<Material> material)
	{
		_materials.try_emplace(material->name, std::move(material));
	}

	void Engine::compile()
	{
		compileMaterials();
		compileSceneObjects();
	}

	int _frameCount = 0;
	float _framesTime = 0.0f;

	void Engine::mainLoop()
	{
		auto prevTime = std::chrono::high_resolution_clock::now();

		while (!_window.shouldClose())
		{
			glfwPollEvents();

			drawFrame();

			// update frame time
			_frameCount++;
			auto currentTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - prevTime).count();
			prevTime = currentTime;

			// process input
			processInput(frameTime);

			// update fps
			// NOTE: VK_PRESENT_MODE_FIFO_KHR enables vertical sync and caps FPS to the monitor refresh rate.
			_framesTime += frameTime;
			if (_framesTime >= 1.0f)
			{
				double fps = 1.0f / (_framesTime / _frameCount);
				_window.setTitle(std::format("Vulkan App | FPS: {:.1f}", fps).c_str());

				_framesTime = 0.0f;
				_frameCount = 0;
			}
		}
	}

	void Engine::drawFrame()
	{
		/*
		    At a high level, rendering a frame in Vulkan consists of a common set of steps:

		    - Wait for the previous frame to finish
		    - Acquire an image from the swap chain
		    - Record a command buffer which draws the scene onto that image
			- Submit the recorded command buffer (waiting on the image to be available - signal when the command buffer finishes)
			- Present the swap chain image (waiting on the command buffer to finish)
		*/

		FrameData& frameData = *_framesData[_currentFrame];

		// record and submit compute commands
		{
			// wait for the previous computation to finish
			vkWaitForFences(_device.getVkDevice(), 1, &frameData.computeCmdExecutedFence, VK_TRUE, UINT64_MAX);
			vkResetFences(_device.getVkDevice(), 1, &frameData.computeCmdExecutedFence);

			// record compute commands
			vkResetCommandBuffer(frameData.computeCmdBuffer, 0);
			recordComputeCommands(frameData.computeCmdBuffer);

			VkSubmitInfo computeSubmitInfo
			{
				.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
				// command buffers
				.commandBufferCount = 1,
				.pCommandBuffers = &frameData.computeCmdBuffer,
				// signal semaphore
				.signalSemaphoreCount = 1,
				.pSignalSemaphores = &frameData.computeCmdExecutedSem,
			};

	    	VK_CHECK(vkQueueSubmit(_device.getComputeQueue().getVkQueue(), 1, &computeSubmitInfo, frameData.computeCmdExecutedFence));
		}

		// Update the frame uniform buffer
		updateFrameUbo();

		// wait for the previous frame to finish (with Fence wait on the CPU)
		vkWaitForFences(_device.getVkDevice(), 1, &frameData.drawCmdExecutedFence, VK_TRUE, UINT64_MAX);
		// reset the fence to unsignaled state
		vkResetFences(_device.getVkDevice(), 1, &frameData.drawCmdExecutedFence);

		// acquire an image from the swap chain (signal the semaphore when the image is ready)
		uint32_t swapChainImageIndex;
        auto result = vkAcquireNextImageKHR(_device.getVkDevice(), _swapChain->getVkSwapChain(), UINT64_MAX, _acquireSemaphore, VK_NULL_HANDLE, &swapChainImageIndex);

		// Since I don't know the image index in advance, I use a staging semaphore then swapped with the one in the array.
		VkSemaphore temp = _acquireSemaphore;
		_acquireSemaphore = _imageAvailableSems[swapChainImageIndex];
		_imageAvailableSems[swapChainImageIndex] = temp;

		// recreate the swap chain if needed
		if (result == VK_ERROR_OUT_OF_DATE_KHR) // swap chain is no longer compatible with the surface (e.g. window resized)
		{
			Log::Get().Warning("Swap chain out of date, recreating");
			recreateSwapChain();
			return;
		}
		if (result != VK_SUCCESS &&
            result != VK_SUBOPTIMAL_KHR) // swap chain no longer matches the surface properties exactly but can still be used to present to the surface successfully
		{
			Log::Get().Error("failed to acquire swap chain image!");
			throw std::runtime_error("failed to acquire swap chain image!");
		}

		// record the drawing commands
		vkResetCommandBuffer(frameData.drawSceneCmdBuffer, 0);
		recordDrawSceneCommands(frameData.drawSceneCmdBuffer, swapChainImageIndex);

		// specify the semaphores and stages to wait on
		// Each entry in the waitStages array corresponds to the semaphore with the same index in waitSemaphores
		VkSemaphore waitSemaphores[] = {frameData.computeCmdExecutedSem, _imageAvailableSems[swapChainImageIndex]};
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }; // in which stage(s) of the pipeline to wait

		// specify which semaphores to signal once the command buffer has finished executing
		VkSemaphore cmdExecutedSignalSemaphores[] = {_drawCmdExecutedSems[swapChainImageIndex]};

		// submit info
		VkSubmitInfo submitInfo
		{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			//wait semaphores
			.waitSemaphoreCount = 2,
			.pWaitSemaphores = waitSemaphores,
			.pWaitDstStageMask = waitStages,
			// command buffers
			.commandBufferCount = 1,
			.pCommandBuffers = &frameData.drawSceneCmdBuffer,
			// signal semaphore
			.signalSemaphoreCount = 1,
			.pSignalSemaphores = cmdExecutedSignalSemaphores,
		};

		// submit the command buffer (the fence will be signaled when the command buffer finishes executing)
        VK_CHECK(vkQueueSubmit(_device.getGraphicsQueue().getVkQueue(), 1, &submitInfo, frameData.drawCmdExecutedFence));

		// present info
		VkPresentInfoKHR presentInfo{};
		presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
		presentInfo.waitSemaphoreCount = 1;
		presentInfo.pWaitSemaphores = cmdExecutedSignalSemaphores; // wait for the command buffer to finish

		VkSwapchainKHR swapChains[] = {_swapChain->getVkSwapChain()};
		presentInfo.swapchainCount = 1;
		presentInfo.pSwapchains = swapChains;
		presentInfo.pImageIndices = &swapChainImageIndex;

		// present the swap chain image
		result = vkQueuePresentKHR(_device.getPresentQueue().getVkQueue(), &presentInfo);

		// recreate the swap chain if needed
		if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || _window.FramebufferResized)
		{
			Log::Get().Trace("Swap chain suboptimal, out of date, or window resized. Recreating.");
			recreateSwapChain();
        }
        else if (result != VK_SUCCESS)
		{
			Log::Get().Error("failed to present swap chain image!");
			throw std::runtime_error("failed to present swap chain image!");
		}

		// advance to the next frame
		_currentFrame = (_currentFrame + 1) % FRAMES_IN_FLIGHT;
	}

	void Engine::updateFrameUbo()
	{
		auto frameUbo = _framesData[_currentFrame]->frameUbo;
		frameUbo.view = camera.getViewMatrix();
		frameUbo.proj = camera.getProjectionMatrix();
		frameUbo.camPos = camera.getPosition();

		_framesData[_currentFrame]->frameUboBuffer->copyDataToBuffer(&frameUbo);
	}

	void Engine::updateObjectUbo(const SceneObject &sceneObject)
	{
		auto objectUbo = _framesData[_currentFrame]->objectUbo;
		objectUbo.model = sceneObject.Transform;
		objectUbo.normalMatrix = glm::transpose(glm::inverse(sceneObject.Transform));

		_framesData[_currentFrame]->objectUboBuffer->copyDataToBuffer(&objectUbo);
	}

	void Engine::createSyncObjects()
	{
		// use separate semaphore per swap chain image (even if the frame count is different)
		// to synchronize between acquiring and presenting images
		size_t imageCount = _swapChain->getImageCount();
		_imageAvailableSems.assign(imageCount, VK_NULL_HANDLE);
		_drawCmdExecutedSems.assign(imageCount, VK_NULL_HANDLE);

		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		for (size_t i = 0; i < imageCount; i++)
		{
            VK_CHECK(vkCreateSemaphore(_device.getVkDevice(), &semaphoreInfo, nullptr, &_imageAvailableSems[i]));
            VK_CHECK(vkCreateSemaphore(_device.getVkDevice(), &semaphoreInfo, nullptr, &_drawCmdExecutedSems[i]));
		}

		VK_CHECK(vkCreateSemaphore(_device.getVkDevice(), &semaphoreInfo, nullptr, &_acquireSemaphore));
	}

	void Engine::drawObjectsLoop(VkCommandBuffer commandBuffer)
	{
		auto currentPipelineType = DEFAULT_PIPELINE;

		// bind default pipeline
		Pipeline* currentPipeline = _graphicsPipelines.at(currentPipelineType).get();
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipeline->getVkPipeline());

		// bind frame descriptor set
		VkDescriptorSet descriptorSet0 = _framesData[_currentFrame]->descriptorSet;
    	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipeline->getLayout(), 0, 1, &descriptorSet0, 0, nullptr);

		// bind default material descriptor set
		VkDescriptorSet descriptorSetMat = _defaultMaterial->descriptorSet;
		uint32_t dynOff = 0;
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipeline->getLayout(), 1, 1, &descriptorSetMat, 1, &dynOff);
		_currentMaterialName = DEFAULT_MATERIAL_NAME;

		for (auto &obj: _sceneObjects)
		{
			//updateObjectUbo(*obj); // TODO: how to update the object ubo instead of using push constants?

			auto objPipeLineType = obj->PipelineKey.value_or(DEFAULT_PIPELINE);

			// determine which pipeline to use for this object
			if (objPipeLineType != currentPipelineType)
			{
				currentPipelineType = objPipeLineType;
				_currentMaterialName = "";

				// bind pipeline
				currentPipeline = _graphicsPipelines.at(currentPipelineType).get();
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipeline->getVkPipeline());

				// bind descriptor set
				descriptorSet0 = _framesData[_currentFrame]->descriptorSet;
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipeline->getLayout(),
				                        0, 1, &descriptorSet0, 0, nullptr);

				_currentMaterialName = "";
			}

			if (currentPipelineType != PipelineType::NoLight)
			{
				// gets the object material and bind the descriptor set if different from the current material
				auto matName = obj->Mesh->getMaterialName();
				const Material& material = matName.empty() ? *_defaultMaterial : *_materials.at(matName);

				if (material.name != _currentMaterialName)
				{
					_currentMaterialName = matName;
					uint32_t dynamicOffset = material.uboIndex * _materialUboAlignment;
					VkDescriptorSet descriptorSet = material.descriptorSet;
					vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipeline->getLayout(), 1, 1, &descriptorSet, 1, &dynamicOffset);
				}
			}

			// push constants
			PushConstantData push
			{
				.model = obj->Transform,
				.normalMatrix = glm::transpose(glm::inverse(obj->Transform))
			};
			vkCmdPushConstants(commandBuffer, currentPipeline->getLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantData), &push);

			obj->Mesh->draw(commandBuffer);
		}
	}

	void Engine::drawParticles(VkCommandBuffer commandBuffer)
	{
		Pipeline *particlePipeline = _graphicsPipelines.at(PipelineType::Particles).get();
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, particlePipeline->getVkPipeline());

		VkDescriptorSet descriptorSet = _framesData[_currentFrame]->descriptorSet;
	    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, particlePipeline->getLayout(), 0, 1, &descriptorSet, 0, nullptr);

		VkBuffer vertexBuffers[] = {_framesData[_currentFrame]->particleSSboBuffer->getVkBuffer()};
		VkDeviceSize offsets[] = {0};
		vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
		vkCmdDraw(commandBuffer, PARTICLES_COUNT, 1, 0, 0);
	}

	void Engine::recordDrawSceneCommands(VkCommandBuffer commandBuffer, uint32_t swapChainImageIndex)
	{
		// it can be executed on a separate thread

		// begin command buffer recording
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; // Optional
		beginInfo.pInheritanceInfo = nullptr; // Optional

		VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

		// begin render pass
		VkRenderPassBeginInfo renderPassInfo{};
		renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		renderPassInfo.renderPass = _swapChain->getRenderPass();
		renderPassInfo.framebuffer = _swapChain->getFrameBuffer(swapChainImageIndex);
		renderPassInfo.renderArea.offset = {0, 0};
		renderPassInfo.renderArea.extent = _swapChain->getExtent();

		std::array<VkClearValue, 2> clearValues{}; // the order of clear values must match the order of attachments in the render pass
		clearValues[0].color = {{0.0f, 0.0f, 0.0f, 1.0f}};
		clearValues[1].depthStencil = { 1.0f, 0 }; // depth range [0.0f, 1.0f] with 1.0f being furthest - init depth with furthest value
		renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
		renderPassInfo.pClearValues = clearValues.data();

		vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

		// set viewport
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(_swapChain->getExtent().width);
		viewport.height = static_cast<float>(_swapChain->getExtent().height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		// set scissor
		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = _swapChain->getExtent();
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		// draw objects
		drawObjectsLoop(commandBuffer);

		// draw particles
		drawParticles(commandBuffer);

		// end render pass
		vkCmdEndRenderPass(commandBuffer);

		// end command buffer recording
		VK_CHECK(vkEndCommandBuffer(commandBuffer));
	}

	void Engine::recordComputeCommands(VkCommandBuffer commandBuffer)
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipeline->getVkPipeline());
		VkDescriptorSet descriptorSet = _framesData[_currentFrame]->descriptorSet;
    	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipeline->getLayout(), 0, 1, &descriptorSet, 0, 0);

		// groupsCount = PARTICLE_COUNT / 256 because we defined in the particle shader 256 invocations for each group
		vkCmdDispatch(commandBuffer, PARTICLES_COUNT / 256, 1, 1);

		VK_CHECK(vkEndCommandBuffer(commandBuffer));
	}

	void Engine::recreateSwapChain()
	{
		// TODO: I should recreate the pipeline if image format or render pass (including subpass layout, attachments, sample count, etc.) changed

		Log::Get().Info("Recreating swap chain");
		while (_window.IsMinimized)
			glfwWaitEvents();

		vkDeviceWaitIdle(_device.getVkDevice());

		SwapChainConfig config
		{
			.samples = _engineConfig.msaa ? _device.getMaxMsaaSamples() : VK_SAMPLE_COUNT_1_BIT,
		};

		if (_swapChain != nullptr)
		{
			config.oldSwapChain = _swapChain->getVkSwapChain();

			/*if (!oldSwapChain->compareSwapFormats(*lveSwapChain.getVkSwapChain()))
			{
			    throw std::runtime_error("Swap chain image(or depth) format has changed!");
			}*/

			_window.FramebufferResized = false;
		}

		_swapChain = std::make_unique<SwapChain>(_device, _window, config);

		// update camera aspect ratio
		camera.setAspectRatio(_swapChain->getAspectRatio());
	}

	void Engine::createPipelines()
	{
		// NoLight
		std::array noLightSetLayouts =
		{
			_descriptorSetManager->getFrameDescriptorSetLayout(), // set 0
		};

		GraphicsPipelineConfig noLightPipelineInfo
		{
			.swapChain = *_swapChain,
			.vertShaderPath = R"(..\shaders\compiled\noLight.vert.spv)",
			.fragShaderPath = R"(..\shaders\compiled\noLight.frag.spv)",
			.vertexBindingDescription = Vertex::getBindingDescription(),
			.vertexAttributeDescriptions = Vertex::getAttributeDescriptions(),
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.setLayoutCount = noLightSetLayouts.size(),
			.pSetLayouts = noLightSetLayouts.data(),
		};
		_graphicsPipelines.emplace(PipelineType::NoLight, PipelineFactory::createGraphicsPipeline(_device, noLightPipelineInfo));

		// PhongLighting
		std::array phongSetLayouts =
		{
			_descriptorSetManager->getFrameDescriptorSetLayout(), // set 0
			_descriptorSetManager->getMaterialDescriptorSetLayout(), // set 1
		};

		GraphicsPipelineConfig phongPipelineInfo
		{
			.swapChain = *_swapChain,
			.vertShaderPath = R"(..\shaders\compiled\phong.vert.spv)",
			.fragShaderPath = R"(..\shaders\compiled\phong.frag.spv)",
			.vertexBindingDescription = Vertex::getBindingDescription(),
			.vertexAttributeDescriptions = Vertex::getAttributeDescriptions(),
			.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
			.setLayoutCount = phongSetLayouts.size(),
			.pSetLayouts = phongSetLayouts.data(),
		};
    	_graphicsPipelines.emplace(PipelineType::PhongLighting, PipelineFactory::createGraphicsPipeline(_device, phongPipelineInfo));

		// Particles
		std::array particlesSetLayouts = {_descriptorSetManager->getFrameDescriptorSetLayout()};
		GraphicsPipelineConfig particlesPipelineInfo
		{
			.swapChain = *_swapChain,
			.vertShaderPath = R"(..\shaders\compiled\particle.vert.spv)",
			.fragShaderPath = R"(..\shaders\compiled\particle.frag.spv)",
			.vertexBindingDescription = Particle::getVertexBindingDescription(),
			.vertexAttributeDescriptions = Particle::getVertexAttributeDescriptions(),
			.topology = VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
			.setLayoutCount = particlesSetLayouts.size(),
			.pSetLayouts = particlesSetLayouts.data(),
		};
    	_graphicsPipelines.emplace(PipelineType::Particles, PipelineFactory::createGraphicsPipeline(_device, particlesPipelineInfo));

		// Compute
		_computePipeline = PipelineFactory::createComputePipeline(_device, _descriptorSetManager->getFrameDescriptorSetLayout());
	}

	void Engine::createFramesResources()
	{
		Log::Get().Info("Creating frame resources");

		// one FrameData for each FRAMES_IN_FLIGHT to don't share resources between frames
		_framesData.resize(FRAMES_IN_FLIGHT);
		VkDeviceSize frameUboSize = sizeof(FrameUbo);
		VkDeviceSize objectUboSize = sizeof(ObjectUbo);

		// Fence create info
		VkFenceCreateInfo fenceInfo{};
		fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
		fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // start in signaled state, to don't block the first frame

		// Semaphore create info
		VkSemaphoreCreateInfo semaphoreInfo{};
		semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

		// allocate descriptor sets and command buffers
		auto descriptorSets = _descriptorSetManager->allocateFrameDescriptorSets(FRAMES_IN_FLIGHT);
		auto drawSceneCmdBuffers = _device.getGraphicsQueue().getPersistentCommandPool().allocateCommandBuffers(FRAMES_IN_FLIGHT);
		auto computeCmdBuffers = _device.getComputeQueue().getPersistentCommandPool().allocateCommandBuffers(FRAMES_IN_FLIGHT);

		for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			// VK_MEMORY_PROPERTY_HOST_COHERENT_BIT: ensures that writes to the mapped memory by the host are automatically visible to the driver (no need for an explicit flush)
			// persistent mapping because we need to update it every frame

			// create frame ubo
			auto frameUboBuffer = std::make_unique<Buffer>(_device, frameUboSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT); // persistent mapping
			FrameUbo frameUbo = {};

			// create object ubo
			auto objectUboBuffer = std::make_unique<Buffer>(_device, objectUboSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
				VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT | VMA_ALLOCATION_CREATE_MAPPED_BIT); // persistent mapping
			ObjectUbo objectUbo = {};

			// create synchronization objects
			VkFence drawFence, computeFence;
			VkSemaphore computeSem;
            VK_CHECK(vkCreateFence(_device.getVkDevice(), &fenceInfo, nullptr, &drawFence));
            VK_CHECK(vkCreateFence(_device.getVkDevice(), &fenceInfo, nullptr, &computeFence));
            VK_CHECK(vkCreateSemaphore(_device.getVkDevice(), &semaphoreInfo, nullptr, &computeSem));

			// create the frame data
			_framesData[i] = std::make_unique<FrameData> (frameUbo, std::move(frameUboBuffer), objectUbo,
				std::move(objectUboBuffer), descriptorSets[i], drawFence, drawSceneCmdBuffers[i]);

			_framesData[i]->computeCmdExecutedFence = computeFence;
			_framesData[i]->computeCmdExecutedSem = computeSem;
			_framesData[i]->computeCmdBuffer = computeCmdBuffers[i];
		}
	}

	void Engine::initParticles()
	{
		Log::Get().Info("Creating shader storage buffers");

		// Initialize particles
		std::default_random_engine rndEngine((unsigned) time(nullptr));
		std::uniform_real_distribution<float> rndDist(0.0f, 1.0f);

		// Initial particle positions on a circle
		std::vector<Particle> particles(PARTICLES_COUNT);
		for (auto &particle: particles)
		{
			float r = 0.25f * sqrt(rndDist(rndEngine));
			float theta = rndDist(rndEngine) * 2 * 3.14159265358979323846;
			float x = r * cos(theta) * HEIGHT / WIDTH;
			float y = r * sin(theta);
			particle.position = glm::vec2(x, y);
			particle.velocity = glm::normalize(glm::vec2(x, y)) * 0.05f;
			particle.color = glm::vec4(rndDist(rndEngine), rndDist(rndEngine), rndDist(rndEngine), 1.0f);
		}

		VkDeviceSize bufferSize = sizeof(Particle) * PARTICLES_COUNT;

		// Create a staging buffer accessible to CPU to upload the data
		Buffer stagingBuffer{ _device, bufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT };

		// Copy data to the staging buffer
		stagingBuffer.copyDataToBuffer(particles.data());

		for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			// create the SSBO buffer
			// VK_BUFFER_USAGE_STORAGE_BUFFER_BIT: to be read and write in the compute shader
			// VK_BUFFER_USAGE_VERTEX_BUFFER_BIT: to be used in the vertex shader
			_framesData[i]->particleSSboBuffer = std::make_unique<Buffer>(_device, bufferSize, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

			// Copy staging buffer to SSBO buffer
			Utils::copyBuffer(_device, stagingBuffer, *_framesData[i]->particleSSboBuffer, bufferSize);
		}
	}

	void Engine::initLights()
	{
		// define lights
		LightsUbo lightsUbo{};

		// Ambient light
		lightsUbo.ambient = glm::vec4(1.0f, 1.0f, 1.0f, 0.08f); // soft gray ambient

		lightsUbo.numLights = 2;

		// Directional light (like sunlight)
		lightsUbo.lights[1].posDir = glm::vec4(-0.5f, 1.0f, -0.3f, 0.0f); // w=0 => dir light
		lightsUbo.lights[1].color = glm::vec4(1.0f, 1.0f, 1.0f, 0.2f);

		// Point light
		lightsUbo.lights[0].posDir = glm::vec4(5.2f, 5.2f, 6.2f, 1.0f); // w=1 => point light
		lightsUbo.lights[0].color = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
		lightsUbo.lights[0].attenuation = glm::vec4(1.0f, 0.09f, 0.032f, 0.0f);

		// Create the lights ubo with device local memory for better performance
		VkDeviceSize lightsUboSize = sizeof(LightsUbo);
        _lightsUboBuffer = std::make_unique<Buffer>(_device, lightsUboSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

		// upload lights data to buffer
		Utils::uploadToDeviceBuffer(_device, *_lightsUboBuffer, lightsUboSize, &lightsUbo);
	}

	void Engine::updateFrameDescriptorSet()
    {
	    // LightUbo Info
	    VkDescriptorBufferInfo lightUboInfo
		{
		    .buffer = _lightsUboBuffer->getVkBuffer(),
		    .offset = 0,
		    .range = sizeof(LightsUbo)
	    };

	    // populate each DescriptorSet
	    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
	    {
	    	auto& frameResources = _framesData[i];
	    	auto frameDescriptorSet = frameResources->descriptorSet;

		    // ObjectUbo Info
		    VkDescriptorBufferInfo objectUboInfo
	    	{
			    .buffer = frameResources->objectUboBuffer->getVkBuffer(),
			    .offset = 0,
			    .range = sizeof(ObjectUbo)
		    };

		    // FrameUbo Info
		    VkDescriptorBufferInfo frameUboInfo
	    	{
			    .buffer = frameResources->frameUboBuffer->getVkBuffer(),
			    .offset = 0,
			    .range = sizeof(FrameUbo)
		    };

		    // ObjectUbo Descriptor Write
		    VkWriteDescriptorSet objectUboWrite
	    	{
			    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			    .dstSet = frameDescriptorSet,
			    .dstBinding = 0,
			    .dstArrayElement = 0,
	    		.descriptorCount = 1,
			    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			    .pBufferInfo = &objectUboInfo
		    };

		    // FrameUbo Descriptor Write
		    VkWriteDescriptorSet frameUboWrite
	    	{
			    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			    .dstSet = frameDescriptorSet,
			    .dstBinding = 1,
			    .dstArrayElement = 0,
	    		.descriptorCount = 1,
			    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			    .pBufferInfo = &frameUboInfo
		    };

		    // Lights Ubo Descriptor Write
		    VkWriteDescriptorSet lightsDescriptorWrite
	    	{
			    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			    .dstSet = frameDescriptorSet,
			    .dstBinding = 2,
			    .dstArrayElement = 0,
	    		.descriptorCount = 1,
			    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
			    .pBufferInfo = &lightUboInfo
		    };

	    	// Particles Ssbo previous frame
	    	VkDescriptorBufferInfo particlesSsboInfoPrevFrame{};
	    	particlesSsboInfoPrevFrame.buffer = _framesData[(i - 1) % FRAMES_IN_FLIGHT]->particleSSboBuffer->getVkBuffer();
	    	particlesSsboInfoPrevFrame.offset = 0;
	    	particlesSsboInfoPrevFrame.range = sizeof(Particle) * PARTICLES_COUNT;

	    	VkWriteDescriptorSet particlesDescriptorWritePrevFrame
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = frameDescriptorSet,
				.dstBinding = 3,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pBufferInfo = &particlesSsboInfoPrevFrame
			};

	    	// Particles Ssbo current frame
	    	VkDescriptorBufferInfo particlesSsboInfoCurrentFrame{};
	    	particlesSsboInfoCurrentFrame.buffer = _framesData[i]->particleSSboBuffer->getVkBuffer();
	    	particlesSsboInfoCurrentFrame.offset = 0;
	    	particlesSsboInfoCurrentFrame.range = sizeof(Particle) * PARTICLES_COUNT;

	    	VkWriteDescriptorSet particlesDescriptorWriteCurrentFrame
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = frameDescriptorSet,
				.dstBinding = 4,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,
				.pBufferInfo = &particlesSsboInfoCurrentFrame
			};

		    std::array descriptorWrites =
		    {
			    objectUboWrite, frameUboWrite, lightsDescriptorWrite, particlesDescriptorWritePrevFrame, particlesDescriptorWriteCurrentFrame
		    };

		    vkUpdateDescriptorSets(_device.getVkDevice(), descriptorWrites.size(),
		                           descriptorWrites.data(), 0, nullptr);
	    }
    }

	void Engine::updateMaterialDescriptorSets(const Material& material)
	{
		for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			auto& frameResources = _framesData[i];

			// MaterialUbo Info
			VkDescriptorBufferInfo materialDynUboInfo
			{
				.buffer = frameResources->materialDynUboBuffer->getVkBuffer(),
				.offset = 0,
				.range = _materialUboAlignment
			};

			// Material Descriptor Write
			VkWriteDescriptorSet materialDynUboWrite
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = material.descriptorSet,
				.dstBinding = 0,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC,
				.pBufferInfo = &materialDynUboInfo
			};

			// diffuse Texture Image Info
			VkDescriptorImageInfo diffuseImageInfo
			{
				.sampler = material.diffuseMap->getSampler(),
				.imageView = material.diffuseMap->getImage().getVkImageView(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			};

			// diffuse Texture Descriptor Write
			VkWriteDescriptorSet diffuseTextDescriptorWrite
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = material.descriptorSet,
				.dstBinding = 1,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &diffuseImageInfo
			};

			// specular Texture Image Info
			VkDescriptorImageInfo specularImageInfo
			{
				.sampler = material.specularMap->getSampler(),
				.imageView = material.specularMap->getImage().getVkImageView(),
				.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
			};

			// specular Texture Descriptor Write
			VkWriteDescriptorSet specularDescriptorWrite
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = material.descriptorSet,
				.dstBinding = 2,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &specularImageInfo
			};

			std::array descriptorWrites =
			{
				materialDynUboWrite, diffuseTextDescriptorWrite, specularDescriptorWrite
			};

			vkUpdateDescriptorSets(_device.getVkDevice(), static_cast<uint32_t>(descriptorWrites.size()),
								   descriptorWrites.data(), 0, nullptr);
		}
	}

	void Engine::compileSceneObjects()
	{
		for (auto &obj: _sceneObjects)
		{
			obj->Mesh->compile(_device);
		}
	}

	void Engine::compileMaterials()
	{
		/*
			- create a MaterialUbo for each Material, write them to a dynamic ubo buffer and store the UboIndex in the material
				(one dynUboBuffer for each frame in flight)
			- allocate and update a descriptorSet for each Material
				(they use the same dynamic ubo buffer but textures are different for each material)
				(descriptorSet are shared between frame in flight because only read operations)
		*/

		size_t materialCount = _materials.size() + 1; // +1 is default material

		// Init a material ubo array
		std::vector<MaterialUbo> materialUbos;
		materialUbos.emplace_back(*_defaultMaterial);

		for (const auto& material: _materials | std::views::values)
		{
			materialUbos.emplace_back(*material);
		}

		// Create the material dynamic ubo buffers, one for each frame in flight
		size_t materialUboSize = materialCount * _materialUboAlignment;
		for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			// create material dyn buffer
			auto materialDynUboBuffer = std::make_unique<Buffer>(_device, materialUboSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

			// copy material ubos array to the dynamic buffer
			Utils::uploadToDeviceBuffer(_device, *materialDynUboBuffer, materialUboSize, materialUbos.data());

			// assign the buffer to the frame resource
			_framesData[i]->materialDynUboBuffer = std::move(materialDynUboBuffer);
		}

		// allocate one descriptor set for each material
		auto descriptorSets = _descriptorSetManager->allocateMaterialDescriptorSets(materialCount);

		// set materials properties and update descriptorSet
		_defaultMaterial->uboIndex = 0;
		_defaultMaterial->diffuseMap = _whiteTexture;
		_defaultMaterial->specularMap = _whiteTexture;
		_defaultMaterial->descriptorSet = descriptorSets[0];
		updateMaterialDescriptorSets(*_defaultMaterial);

		uint32_t index = 1; // index 0 is for the default material
		for (auto& material: _materials | std::views::values)
		{
			// set ubo index
			material->uboIndex = index;

			// load texture
			if (!material->diffuseTexturePath.empty())
				material->diffuseMap = loadTexture(material->diffuseTexturePath);
			else
				material->diffuseMap = _whiteTexture;

			if (!material->specularTexturePath.empty())
				material->specularMap = loadTexture(material->specularTexturePath);
			else
				material->specularMap = _whiteTexture;

			// update the material descriptor set
			material->descriptorSet = descriptorSets[index++];
			updateMaterialDescriptorSets(*material);
		}
	}

	void Engine::copyBufferToImage(const Buffer &srcBuffer, VkImage image, uint32_t width, uint32_t height)
	{
		// Begin one-time command
		VkCommandBuffer commandBuffer = _device.getGraphicsQueue().beginOneTimeCommand();

		// Copy buffer to image
		VkBufferImageCopy region{};
		region.bufferOffset = 0;
		region.bufferRowLength = 0; // 0 means tightly packed, no padding bytes
		region.bufferImageHeight = 0; // 0 means tightly packed, no padding bytes

		region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		region.imageSubresource.mipLevel = 0;
		region.imageSubresource.baseArrayLayer = 0;
		region.imageSubresource.layerCount = 1;

		// which part of the image to copy to
		region.imageOffset = {0, 0, 0};
		region.imageExtent = {width, height, 1};

		vkCmdCopyBufferToImage(commandBuffer, srcBuffer.getVkBuffer(), image,
		                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // which layout the image is currently using
		                       1, &region
		);

		// Execute the copy command
		_device.getGraphicsQueue().endOneTimeCommand(commandBuffer);
	}

	void Engine::createDefaultTexture()
	{
		uint8_t whitePixel[4] = { 255, 255, 255, 255 };

		_whiteTexture = createTexture(1, 1, &whitePixel);
	}

	std::unique_ptr<Texture> Engine::loadTexture(const std::string& filePath)
	{
		// load texture data. Return a pointer to the array of RGBA values
		int texWidth, texHeight, texChannels;
		stbi_uc *pixels = stbi_load(filePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

		if (!pixels)
		{
			throw std::runtime_error("failed to load texture image!");
		}

		// create the texture
		auto texture = createTexture(static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight), pixels);

		// Free texture data
		stbi_image_free(pixels);

		return texture;
	}

	std::unique_ptr<Texture> Engine::createTexture(uint32_t width, uint32_t height, void* data)
	{
		/*
		- Create a staging buffer and copy the image data to it
		- Create the VkImage object
		- Copy data from the staging buffer to the VkImage
		*/

		VkDeviceSize imageSize = width * height * 4; // 4 bytes per pixel (RGBA)

		// Create a staging buffer to upload the texture data to GPU
        Buffer stagingBuffer{ _device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT };

		// Copy texture data to the staging buffer
		stagingBuffer.copyDataToBuffer(data);

		auto texture = std::make_unique<Texture>(_device, width, height);

		Image& textImage = texture->getImage();

		// Transition image layout to be optimal for receiving data
        transitionImageLayout(textImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		// Copy the texture data from the staging buffer to the image
        copyBufferToImage(stagingBuffer, textImage.getVkImage(), width, height);

		//transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps
		// Transition image layout to be optimal for shader access
		//transitionImageLayout(textImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		// Generate mipmaps (also transitions the image to be optimal for shader access)
		generateMipmaps(textImage);

		return texture;
	}

	void Engine::processInput(float delta)
	{
		int key = _window.getPressedKey();

		if (key == GLFW_KEY_W) camera.moveUp(delta);
		if (key == GLFW_KEY_S) camera.moveUp(-delta);
		if (key == GLFW_KEY_D) camera.moveRight(delta);
		if (key == GLFW_KEY_A) camera.moveRight(-delta);

		if (key == GLFW_KEY_UP) camera.orbitVertical(delta);
		if (key == GLFW_KEY_DOWN) camera.orbitVertical(-delta);
		if (key == GLFW_KEY_RIGHT) camera.orbitHorizontal(delta);
		if (key == GLFW_KEY_LEFT) camera.orbitHorizontal(-delta);

		if (key == GLFW_KEY_PAGE_DOWN || key == GLFW_KEY_E) camera.zoom(delta);
		if (key == GLFW_KEY_PAGE_UP || key == GLFW_KEY_Q) camera.zoom(-delta);

		if (key == GLFW_KEY_P)
		{
			if (camera.getProjectionType() == Camera::ProjectionType::Perspective)
				camera.setProjectionType(Camera::ProjectionType::Orthographic);
			else
				camera.setProjectionType(Camera::ProjectionType::Perspective);
		}
	}

    void Engine::transitionImageLayout(const Image& image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
	{
		/*
		In Vulkan, an image layout describes how the GPU should treat the memory of an image (texture, framebuffer, etc.).
		A layout transition is changing an image from one layout to another, so the GPU knows how to access it correctly.
		This is done with a pipeline barrier(vkCmdPipelineBarrier), which synchronizes memory access and updates the image layout.
		*/

		VkCommandBuffer commandBuffer = _device.getGraphicsQueue().beginOneTimeCommand();

		// Set a memory barrier to synchronize access to the image
		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout; // it's ok to use VK_IMAGE_LAYOUT_UNDEFINED if we don't care about the existing image data
		barrier.newLayout = newLayout;

		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // for queue family ownership transfer, not used here
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

		// set image info
		barrier.image = image.getVkImage();
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseMipLevel = 0;
		barrier.subresourceRange.levelCount = image.getMipLevels();
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;

		VkPipelineStageFlags sourceStage; // stage that should happen before the barrier
		VkPipelineStageFlags destinationStage; // stage that will wait until on the barrier

		if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			barrier.srcAccessMask = 0;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

			sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT; // earliest possible stage
			destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT; // transfer stage (it's a pseudo-stage)
        }
        else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
			destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT; // fragment shader reads from the texture
        }
        else
		{
			throw std::invalid_argument("unsupported layout transition!");
		}

		vkCmdPipelineBarrier(
			commandBuffer,
			sourceStage,
			destinationStage,
			0, // dependency flags
			0, nullptr, // memory barriers
			0, nullptr, // buffer memory barries
			1, &barrier // image memory barriers
		);

		_device.getGraphicsQueue().endOneTimeCommand(commandBuffer);
	}

	void Engine::generateMipmaps(const Image &image)
	{
		// Use vkCmdBlitImage command. This command performs copying, scaling, and filtering operations.
		// We will call this multiple times to blit data to each mip level of the image.
		// Source and destination of the command will be the same image, but different mip levels.

		// Check if the image format supports linear blitting
		if (!_device.isLinearFilteringSupported(image.getFormat(), VK_IMAGE_TILING_OPTIMAL))
		{
			Log::Get().Warning("Failed to create mip levels. Texture image format does not support linear blitting!");

            transitionImageLayout(image, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
			return;
		}

		VkCommandBuffer commandBuffer = _device.getGraphicsQueue().beginOneTimeCommand();

		auto vkImage = image.getVkImage();

		VkImageMemoryBarrier barrier{};
		barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.image = vkImage;
		barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
		barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		barrier.subresourceRange.baseArrayLayer = 0;
		barrier.subresourceRange.layerCount = 1;
		barrier.subresourceRange.levelCount = 1;

		int32_t mipWidth = image.getWidth();
		int32_t mipHeight = image.getHeight();
		auto mipLevels = image.getMipLevels();
		for (uint32_t i = 1; i < mipLevels; i++)
		{
			barrier.subresourceRange.baseMipLevel = i - 1;
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
			barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer,
			                     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
			                     0, nullptr,
			                     0, nullptr,
			                     1, &barrier);

			// blit info
			VkImageBlit blit{};
			blit.srcOffsets[0] = {0, 0, 0};
			blit.srcOffsets[1] = {mipWidth, mipHeight, 1};
			blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.srcSubresource.mipLevel = i - 1;
			blit.srcSubresource.baseArrayLayer = 0;
			blit.srcSubresource.layerCount = 1;
			blit.dstOffsets[0] = {0, 0, 0};
			blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 }; // each mip level is half the size of the previous level
			blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			blit.dstSubresource.mipLevel = i;
			blit.dstSubresource.baseArrayLayer = 0;
			blit.dstSubresource.layerCount = 1;

			// blit command
			vkCmdBlitImage(commandBuffer,
			               vkImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
			               vkImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
			               1, &blit,
			               VK_FILTER_LINEAR);

			// transition mip level i-1 to shader read only optimal
			barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
			barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
			barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
			barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

			vkCmdPipelineBarrier(commandBuffer,
			                     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
			                     0, nullptr,
			                     0, nullptr,
			                     1, &barrier);

			// next mip level is half the size
			if (mipWidth > 1) mipWidth /= 2;
			if (mipHeight > 1) mipHeight /= 2;
		}

		// transition the last mip level to shader read only optimal
		barrier.subresourceRange.baseMipLevel = mipLevels - 1;
		barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
		barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
		barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

		vkCmdPipelineBarrier(commandBuffer,
		                     VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
		                     0, nullptr,
		                     0, nullptr,
		                     1, &barrier);

		_device.getGraphicsQueue().endOneTimeCommand(commandBuffer);
	}
} // namespace m1
