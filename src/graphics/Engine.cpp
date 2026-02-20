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
#include <limits>

namespace m1
{
	Engine::Engine(EngineConfig config) : _config(config)
	{
		Log::Get().Info("Engine constructor");

		recreateSwapChain();
		_descriptorSetManager = std::make_unique<DescriptorSetManager>(_device);
		createShadowResources();
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
		_bbox = computeSceneBBox();
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
				double avgFrameMs = (_framesTime / _frameCount) * 1000.0;
				_window.setTitle(std::format("Vulkan App | FPS: {:.1f} | Frame: {:.2f} ms", fps, avgFrameMs).c_str());

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
		    - Record a command buffer which draws the scene onto a color image and copy it to the swap chain image
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
		frameUbo.view = _camera.getViewMatrix();
		frameUbo.proj = _camera.getProjectionMatrix();
		frameUbo.lightViewProjMatrix = computeLightViewProjMatrix();
		frameUbo.camPos = glm::vec4(_camera.getPosition(), 1.0f);
		frameUbo.shadowsEnabled = _config.shadows ? 1 : 0;

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

		/*
			Rendering is done on a color image, then copied to the swap chain image.
			If multi-sample antialiasing is enabled, rendering is done on msaa image, then resolved to the color image.

			At a high level:
			- transition the attachments images to right layouts
			- begin rendering
			- draw objects command
			- end rendering
			- copy color image to swap chain image
			- transition swap chain image to present layout
		*/

		// reset the command buffer and begin a new recording
		vkResetCommandBuffer(commandBuffer, 0);
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; // Optional
		beginInfo.pInheritanceInfo = nullptr; // Optional
		VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

		if (_config.shadows)
			// create the shadow map
			recordShadowMappingPass(commandBuffer);
		else
			// transition layout SHADER_READ_ONLY_OPTIMAL - still attached to the descriptor even if not used in the shader when shadows are disabled
			transitionImageLayout(commandBuffer, _shadowMap->getImage().getVkImage(), 1,
				VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

		// gets the images attachments
		Image& colorImage = _swapChain->getColorImage();
		Image& msaaImage = _swapChain->getMsaaColorImage();
		Image& depthImage = _swapChain->getDepthImage();
		VkImage swapChainImage = _swapChain->getSwapChainImage(swapChainImageIndex);

		// TODO: should I use the real current layout instead of undefined?
		// TODO: should I set the layout at each frame even if is not changing (e.g. depthImage). Transition is not only for changing the layout but also to set the memory barriers

		// transition the msaa and color image to COLOR_ATTACHMENT_OPTIMAL
		transitionImageLayout(commandBuffer, colorImage.getVkImage(), 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
		if (_config.msaa) transitionImageLayout(commandBuffer, msaaImage.getVkImage(), msaaImage.getMipLevels(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

		// transition the depth image to DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		transitionImageLayout(commandBuffer, depthImage.getVkImage(), depthImage.getMipLevels(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

		// choose the render target image
		Image& renderTarget = _config.msaa ? msaaImage : colorImage;

		// set the color attachment
		VkRenderingAttachmentInfo colorAttachment
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = renderTarget.getVkImageView(),
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = {{0.0f, 0.0f, 0.0f, 1.0f}},
		};

		// set resolve image if msaa is enable
		if (_config.msaa)
		{
			colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
			colorAttachment.resolveImageView = colorImage.getVkImageView();
			colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Optimization: don't save the multi-sample image
		}

		// set depth attachment
		VkRenderingAttachmentInfo depthAttachment
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = depthImage.getVkImageView(),
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = { .depthStencil { 1.0f, 0 } } // depth range [0.0f, 1.0f] with 1.0f being furthest - init depth with furthest value
		};

		// begin rendering
		VkRenderingInfo renderingInfo
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea = {{0, 0}, renderTarget.getExtent()},
			.layerCount = 1,
			.colorAttachmentCount = 1,
			.pColorAttachments = &colorAttachment,
			.pDepthAttachment = &depthAttachment,
		};
		vkCmdBeginRendering(commandBuffer, &renderingInfo);

		// set viewport
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(renderTarget.getExtent().width);
		viewport.height = static_cast<float>(renderTarget.getExtent().height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		// set scissor
		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = renderTarget.getExtent();
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		// draw objects
		drawObjectsLoop(commandBuffer);

		// draw particles
		drawParticles(commandBuffer);

		// end rendering
		vkCmdEndRendering(commandBuffer);

		// transition the color image and the swapchain image into their correct transfer layouts
		transitionImageLayout(commandBuffer, colorImage.getVkImage(), 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
		transitionImageLayout(commandBuffer, swapChainImage, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

		// copy the color image into the swapchain image
		copyImageToImage(commandBuffer, colorImage.getVkImage(), swapChainImage, colorImage.getExtent(), _swapChain->getExtent());

		// set the swapchain image layout to Present to show it on the screen
		transitionImageLayout(commandBuffer, swapChainImage, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_ASPECT_COLOR_BIT);

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
			.samples = _config.msaa ? _device.getMaxMsaaSamples() : VK_SAMPLE_COUNT_1_BIT,
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
		_camera.setAspectRatio(_swapChain->getAspectRatio());
	}

	BBox Engine::computeSceneBBox() const
	{
		BBox bbox;
		for (const auto& obj : _sceneObjects)
		{
			// skip auxiliary objects
			if (obj->IsAuxiliary)
				continue;

			for (const auto& vertex : obj->Mesh->Vertices)
			{
				glm::vec3 worldPos = glm::vec3(obj->Transform * glm::vec4(vertex.pos, 1.0f));
				bbox.merge(worldPos);
			}
		}
		return bbox;
	}

	glm::mat4 Engine::computeLightViewProjMatrix() const
	{
		const Light& directionalLight = _lightsUbo.lights[1];
		glm::vec3 lightDir = glm::normalize(glm::vec3(directionalLight.posDir));

		// fit on scene bounding box
		auto center = _bbox.getCenter();

		// Create the light view matrix
		glm::vec3 lightPos = center - lightDir;
		glm::vec3 up = glm::abs(glm::dot(lightDir, glm::vec3(0.0f, 0.0f, 1.0f))) > 0.99f
			? glm::vec3(1.0f, 0.0f, 0.0f) // use x-axis if the light direction is parallel to z-axis
			: glm::vec3(0.0f, 0.0f, 1.0f);
		const auto lightView = glm::lookAt(lightPos, center, up);

		float left = -1, right = 1, bottom = -1, top = 1, near = 0, far = 1;

		if (_bbox.min.x <= _bbox.max.x) // Check if the bbox is valid
		{
			// transform bbox corners in light view space and find max and min coordinates (AABB)
			auto corners = _bbox.getCorners();

			float minX = std::numeric_limits<float>::max();
			float maxX = std::numeric_limits<float>::lowest();
			float minY = std::numeric_limits<float>::max();
			float maxY = std::numeric_limits<float>::lowest();
			float minZ = std::numeric_limits<float>::max();
			float maxZ = std::numeric_limits<float>::lowest();

			for (const auto& corner : corners)
			{
				const auto trf = lightView * glm::vec4(corner, 1.0f);
				minX = std::min(minX, trf.x);
				maxX = std::max(maxX, trf.x);
				minY = std::min(minY, trf.y);
				maxY = std::max(maxY, trf.y);
				minZ = std::min(minZ, trf.z);
				maxZ = std::max(maxZ, trf.z);
			}

			// make the AABB square around the center
			float width  = maxX - minX;
			float height = maxY - minY;

			float extend = std::max(width, height);
			glm::vec3 centerLightSpace = lightView * glm::vec4(center, 1.0f);;

			left = centerLightSpace.x - extend * 0.5f;
			right = centerLightSpace.x + extend * 0.5f;

			bottom = centerLightSpace.y - extend * 0.5f;
			top = centerLightSpace.y + extend * 0.5f;

			near = minZ;
			far = maxZ;
		}

		// Build ortho projection that encloses the frustum in light space
		glm::mat4 lightProj = Utils::orthoProjection(left, right, bottom, top, near, far);

		return lightProj * lightView;

		/*
			// Compute light view-projection matrix that tightly fits the camera view frustum (https://learnopengl.com/Guest-Articles/2021/CSM)
			// Steps:
			// 1. Extract camera frustum corners and center in world space
			// 2. Transform those corners into light space
			// 3. Compute an orthographic projection that encloses those points


		// Camera matrices
		auto camView = _camera.getViewMatrix();
		auto camProj = _camera.getProjectionMatrix();

		// We need the inverse of view*proj to transform NDC cube corners in world space
		glm::mat4 invViewProj = glm::inverse(camProj * camView);

		// compute frustum corners in world space
		std::vector<glm::vec4> frustumCornersWorld;
		for (unsigned int x = 0; x < 2; ++x)
		{
			for (unsigned int y = 0; y < 2; ++y)
			{
				for (unsigned int z = 0; z < 2; ++z)
				{
					const glm::vec4 pt =
						invViewProj * glm::vec4(
							2.0f * x - 1.0f,
							2.0f * y - 1.0f,
							static_cast<float>(z), // depth [0, 1]
							1.0f);
					frustumCornersWorld.push_back(pt / pt.w);
				}
			}
		}

		// Compute the center of the frustum
		auto frustumCenterWorld = glm::vec3(0.0f);
		for (const auto &c : frustumCornersWorld) frustumCenterWorld += glm::vec3(c);
		frustumCenterWorld /= 8.0f;

		// Create the light view matrix
		glm::vec3 lightPos = frustumCenterWorld - lightDir;
		glm::vec3 up = glm::abs(glm::dot(lightDir, glm::vec3(0.0f, 0.0f, 1.0f))) > 0.99f
			? glm::vec3(1.0f, 0.0f, 0.0f) // use this if the light direction is parallel to z axis
			: glm::vec3(0.0f, 0.0f, 1.0f);
		const auto lightView = glm::lookAt(lightPos, frustumCenterWorld, up);

		// gets min and max in light view space
		float minX = std::numeric_limits<float>::max();
		float maxX = std::numeric_limits<float>::lowest();
		float minY = std::numeric_limits<float>::max();
		float maxY = std::numeric_limits<float>::lowest();
		float minZ = std::numeric_limits<float>::max();
		float maxZ = std::numeric_limits<float>::lowest();

		// 1. Consider camera frustum corners
		for (const auto& v : frustumCornersWorld)
		{
			const auto trf = lightView * v;
			minX = std::min(minX, trf.x);
			maxX = std::max(maxX, trf.x);
			minY = std::min(minY, trf.y);
			maxY = std::max(maxY, trf.y);
			minZ = std::min(minZ, trf.z);
			maxZ = std::max(maxZ, trf.z);
		}

		// extend near/far (not only geometry which is in the frustum can cast shadows on a surface in the frustum!)
		constexpr float zMult = 10.0f; // Tune this parameter according to the scene
		if (minZ < 0) minZ *= zMult; else minZ /= zMult;
		if (maxZ < 0) maxZ /= zMult; else maxZ *= zMult;

		// Build ortho projection that encloses the frustum in light space
		glm::mat4 lightProj = Utils::orthoProjection(minX, maxX, minY, maxY, minZ, maxZ);

		return lightProj * lightView;
		*/
	}

	void Engine::createShadowResources()
	{
		// find image format
		auto shadowImageFormat = _device.findSupportedFormat(
{ VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT },
			VK_IMAGE_TILING_OPTIMAL,
			VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT
		);

		// set image parameters
		ImageParams params
		{
			.extent = {2048, 2048},
			.format = shadowImageFormat,
			.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			.memoryProps = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, // dedicated allocation for special, big resources, like fullscreen images used as attachments
			.aspectFlags = VK_IMAGE_ASPECT_DEPTH_BIT,
		};

		// create the shadow map image
		auto shadowMapImage = std::make_unique<Image>(_device, params);

		// set sampler info
		VkSamplerCreateInfo samplerInfo{};
		samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
		samplerInfo.magFilter = VK_FILTER_LINEAR;
		samplerInfo.minFilter = VK_FILTER_LINEAR;
		// clamp to a white border => projected fragment coordinates outside the light frustum are not in shadow
		samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_BORDER;
		samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
		samplerInfo.compareEnable = VK_FALSE;
		samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;

		// create the sampler
		VkSampler shadowSampler{};
		VK_CHECK(vkCreateSampler(_device.getVkDevice(), &samplerInfo, nullptr, &shadowSampler));

		// create the shadow map texture
		_shadowMap = std::make_unique<Texture>(_device, std::move(shadowMapImage), shadowSampler);
	}

	void Engine::recordShadowMappingPass(VkCommandBuffer commandBuffer) const
	{
		Image& shadowMapImage = _shadowMap->getImage();

		// transition the layout to DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		transitionImageLayout(commandBuffer, shadowMapImage.getVkImage(), 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

		// set depth attachment
		VkRenderingAttachmentInfo depthAttachment
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView = shadowMapImage.getVkImageView(),
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue = { .depthStencil { 1.0f, 0 } } // depth range [0.0f, 1.0f] with 1.0f being furthest - init depth with furthest value
		};

		// begin rendering
		VkRenderingInfo renderingInfo
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea = {{0, 0}, shadowMapImage.getExtent()},
			.layerCount = 1,
			.colorAttachmentCount = 0,
			.pColorAttachments = nullptr,
			.pDepthAttachment = &depthAttachment,
		};
		vkCmdBeginRendering(commandBuffer, &renderingInfo);

		// set viewport
		VkViewport viewport{};
		viewport.x = 0.0f;
		viewport.y = 0.0f;
		viewport.width = static_cast<float>(shadowMapImage.getExtent().width);
		viewport.height = static_cast<float>(shadowMapImage.getExtent().height);
		viewport.minDepth = 0.0f;
		viewport.maxDepth = 1.0f;
		vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

		// set scissor
		VkRect2D scissor{};
		scissor.offset = {0, 0};
		scissor.extent = shadowMapImage.getExtent();
		vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

		// bind shadow mapping pipeline
		Pipeline* pipeline = _graphicsPipelines.at(PipelineType::ShadowMapping).get();
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getVkPipeline());

		// bind frame descriptor set
		VkDescriptorSet descriptorSet = _framesData[_currentFrame]->descriptorSet;
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getLayout(), 0, 1, &descriptorSet, 0, nullptr);

		// draw objects loop
		for (auto &obj: _sceneObjects)
		{
			// push constants
			PushConstantData push
			{
				.model = obj->Transform,
				.normalMatrix = glm::transpose(glm::inverse(obj->Transform))
			};
			vkCmdPushConstants(commandBuffer, pipeline->getLayout(), VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstantData), &push);

			// draw the mesh
			obj->Mesh->draw(commandBuffer);
		}

		// end rendering
		vkCmdEndRendering(commandBuffer);

		// transition layout SHADER_READ_ONLY_OPTIMAL
		transitionImageLayout(commandBuffer, shadowMapImage.getVkImage(), 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
	}

	void Engine::createPipelines()
	{
		// Shadow mapping
		std::array shadowSetLayouts
		{
			_descriptorSetManager->getFrameDescriptorSetLayout(),
		};
		GraphicsPipelineConfig shadowPipelineInfo
		{
			.swapChain = *_swapChain,
			.shadowMapFormat = _shadowMap->getImage().getFormat(),
			.vertShaderPath = R"(..\shaders\compiled\shadow.vert.spv)",
			.vertexBindingDescription = Vertex::getBindingDescription(),
			.vertexAttributeDescriptions = Vertex::getAttributeDescriptions(),
			.cullMode = VK_CULL_MODE_FRONT_BIT, // front face culling to fix peter panning artifacts, but works only for 3D solid objects, not for planes/surfaces
			.depthTestEnable = true,
			.depthWriteEnable = true,
			.setLayoutCount = shadowSetLayouts.size(),
			.pSetLayouts = shadowSetLayouts.data(),
		};
		_graphicsPipelines.emplace(PipelineType::ShadowMapping, PipelineFactory::createShadowMapPipeline(_device, shadowPipelineInfo));

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
		// Ambient light
		_lightsUbo.ambient = glm::vec4(1.0f, 1.0f, 1.0f, 0.08f); // soft gray ambient

		_lightsUbo.numLights = 2;

		// Directional light (like sunlight)
		_lightsUbo.lights[1].posDir = glm::vec4(-0.5f, 1.0f, -0.8f, 0.0f); // w=0 => dir light
		_lightsUbo.lights[1].color = glm::vec4(1.0f, 1.0f, 1.0f, 0.2f);

		// Point light
		_lightsUbo.lights[0].posDir = glm::vec4(5.2f, 5.2f, 6.2f, 1.0f); // w=1 => point light
		_lightsUbo.lights[0].color = glm::vec4(1.0f, 1.0f, 1.0f, 0.4f);
		_lightsUbo.lights[0].attenuation = glm::vec4(1.0f, 0.09f, 0.032f, 0.0f);

		// Create the lights ubo with device local memory for better performance
		VkDeviceSize lightsUboSize = sizeof(LightsUbo);
        _lightsUboBuffer = std::make_unique<Buffer>(_device, lightsUboSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

		// upload lights data to buffer
		Utils::uploadToDeviceBuffer(_device, *_lightsUboBuffer, lightsUboSize, &_lightsUbo);
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

		// ShadowMap info (used to define the sampler)
	    VkDescriptorImageInfo shadowMapImageInfo
		{
			.sampler = _shadowMap->getSampler(),
			.imageView = _shadowMap->getImage().getVkImageView(),
			.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
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

	    	// Shadow map sampler write
	    	VkWriteDescriptorSet shadowMapDescriptorWrite
			{
				.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
				.dstSet = frameDescriptorSet,
				.dstBinding = 5,
				.dstArrayElement = 0,
				.descriptorCount = 1,
				.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
				.pImageInfo = &shadowMapImageInfo
			};

		    std::array descriptorWrites =
		    {
			    objectUboWrite, frameUboWrite, lightsDescriptorWrite, particlesDescriptorWritePrevFrame, particlesDescriptorWriteCurrentFrame, shadowMapDescriptorWrite
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
        transitionImageLayout(textImage, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

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

		if (key == GLFW_KEY_W) _camera.moveUp(delta);
		if (key == GLFW_KEY_S) _camera.moveUp(-delta);
		if (key == GLFW_KEY_D) _camera.moveRight(delta);
		if (key == GLFW_KEY_A) _camera.moveRight(-delta);

		if (key == GLFW_KEY_UP) _camera.orbitVertical(delta);
		if (key == GLFW_KEY_DOWN) _camera.orbitVertical(-delta);
		if (key == GLFW_KEY_RIGHT) _camera.orbitHorizontal(delta);
		if (key == GLFW_KEY_LEFT) _camera.orbitHorizontal(-delta);

		if (key == GLFW_KEY_PAGE_DOWN || key == GLFW_KEY_E) _camera.zoom(delta);
		if (key == GLFW_KEY_PAGE_UP || key == GLFW_KEY_Q) _camera.zoom(-delta);

		if (key == GLFW_KEY_P)
		{
			if (_camera.getProjectionType() == Camera::ProjectionType::Perspective)
				_camera.setProjectionType(Camera::ProjectionType::Orthographic);
			else
				_camera.setProjectionType(Camera::ProjectionType::Perspective);
		}
	}

	void Engine::transitionImageLayout(const Image &image, VkImageLayout oldLayout, VkImageLayout newLayout, VkImageAspectFlags aspectMask) const
	{
		VkCommandBuffer commandBuffer = _device.getGraphicsQueue().beginOneTimeCommand();

		transitionImageLayout(commandBuffer, image.getVkImage(), image.getMipLevels(), oldLayout, newLayout, aspectMask);

		_device.getGraphicsQueue().endOneTimeCommand(commandBuffer);
	}


	void Engine::transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, uint32_t mipLevels, VkImageLayout currentLayout, VkImageLayout newLayout, VkImageAspectFlags aspectMask)
	{
		// TODO: refactor this method without duplicated code?

		/*
		In Vulkan, an image layout describes how the GPU should treat the memory of an image (texture, framebuffer, etc.).
		A layout transition is changing an image from one layout to another, so the GPU knows how to access it correctly.
		This is done with a pipeline barrier(vkCmdPipelineBarrier2), which synchronizes memory access and updates the image layout.

		SYNCHRONIZATION PARAMETERS (https://docs.vulkan.org/spec/latest/chapters/synchronization.html)

		srcStageMask: pipeline stage to wait to be finished before starting the transition
		srcAccessMask: memory cache to flush before starting the transition. E.g.: if the GPU just wrote to the image, the data might still
						be in a fast L1/L2 cache and not in the main VRAM yet. This flag tells the driver which caches to flush.

		destStageMask: pipeline stage to block until the transition is done
		dstAccessMask: which memory caches need to be invalidated. E.g.: If you are going to read the texture,
						the GPU needs to ensure the L1/L2 read caches are fresh.
		*/

		VkImageMemoryBarrier2 barrier
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.oldLayout = currentLayout,
			.newLayout = newLayout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, // for queue family ownership transfer, not used here
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = image,
			.subresourceRange = {aspectMask, 0, mipLevels, 0, 1}
		};

		// UNDEFINED -> COLOR_ATTACHMENT
		// We don't care about previous data, so we don't wait for anything.
		if (currentLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
		{
			barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT; // earliest possible stage
			barrier.srcAccessMask = VK_ACCESS_2_NONE;

			barrier.dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
			barrier.dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
		}
		// COLOR_ATTACHMENT -> PRESENT
		else if (currentLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
		{
			barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
			barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

			barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT; // Nothing typically runs after this on the GPU
			barrier.dstAccessMask = VK_ACCESS_2_NONE;
		}
		// TRANSFER_DST -> PRESENT
		else if (currentLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
		{
			barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;

			barrier.dstStageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
			barrier.dstAccessMask = VK_ACCESS_2_NONE;
		}
		// COLOR_ATTACHMENT -> TRANSFER_SRC
		else if (currentLayout == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
		{
			barrier.srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
			barrier.srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;

			barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
			barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
		}
		// UNDEFINED -> TRANSFER_DST (upload data to texture)
		else if (currentLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
		{
			barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
			barrier.srcAccessMask = VK_ACCESS_2_NONE;

			barrier.dstStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT; // transfer stage (it's a pseudo-stage)
			barrier.dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
		}
		// TRANSFER_DST -> SHADER_READ (Texture upload finished, ready for shader)
		else if (currentLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			barrier.srcStageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
			barrier.srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;

			barrier.dstStageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT; // fragment shader reads from the texture
			barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
		}
		// UNDEFINED -> DEPTH_STENCIL_ATTACHMENT
		else if (currentLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
		{
			barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
			barrier.srcAccessMask = VK_ACCESS_2_NONE;

			// Block the Depth Test units
			barrier.dstStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | // where the GPU checks the depth before running the Fragment Shader
									VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT; // where the GPU writes the final depth value after the Fragment Shader
			barrier.dstAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
		}
		// DEPTH_STENCIL_ATTACHMENT -> VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL (shadowMap)
		else if (currentLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			barrier.srcStageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT;
			barrier.srcAccessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

			barrier.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
		}
		// UNDEFINED -> VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL (shadowMap)
		else if (currentLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
		{
			barrier.srcStageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT;
			barrier.srcAccessMask = VK_ACCESS_2_NONE;

			barrier.dstStageMask = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
			barrier.dstAccessMask = VK_ACCESS_2_SHADER_READ_BIT;
		}
		else
		{
			throw std::invalid_argument("not implemented image layout transition!");

			/*
			// Fallback for unknown transitions (Safe but slow)
			// It basically waits for EVERYTHING to finish before doing the transition.
			barrier.srcStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			barrier.srcAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT;
			barrier.dstStageMask = VK_PIPELINE_STAGE_2_ALL_COMMANDS_BIT;
			barrier.dstAccessMask = VK_ACCESS_2_MEMORY_WRITE_BIT | VK_ACCESS_2_MEMORY_READ_BIT;
			*/
		}

		VkDependencyInfo depInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &barrier,
		};

		vkCmdPipelineBarrier2(commandBuffer, &depInfo);
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

            transitionImageLayout(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
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

	void Engine::copyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize)
	{
		VkImageBlit2 blitRegion{ .sType = VK_STRUCTURE_TYPE_IMAGE_BLIT_2, .pNext = nullptr };

		blitRegion.srcOffsets[1].x = srcSize.width;
		blitRegion.srcOffsets[1].y = srcSize.height;
		blitRegion.srcOffsets[1].z = 1;

		blitRegion.dstOffsets[1].x = dstSize.width;
		blitRegion.dstOffsets[1].y = dstSize.height;
		blitRegion.dstOffsets[1].z = 1;

		blitRegion.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blitRegion.srcSubresource.baseArrayLayer = 0;
		blitRegion.srcSubresource.layerCount = 1;
		blitRegion.srcSubresource.mipLevel = 0;

		blitRegion.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
		blitRegion.dstSubresource.baseArrayLayer = 0;
		blitRegion.dstSubresource.layerCount = 1;
		blitRegion.dstSubresource.mipLevel = 0;

		VkBlitImageInfo2 blitInfo{ .sType = VK_STRUCTURE_TYPE_BLIT_IMAGE_INFO_2, .pNext = nullptr };
		blitInfo.dstImage = destination;
		blitInfo.dstImageLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
		blitInfo.srcImage = source;
		blitInfo.srcImageLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
		blitInfo.filter = VK_FILTER_LINEAR;
		blitInfo.regionCount = 1;
		blitInfo.pRegions = &blitRegion;

		vkCmdBlitImage2(cmd, &blitInfo);
	}
} // namespace m1
