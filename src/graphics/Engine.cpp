#include "Engine.hpp"
#include "Log.hpp"
#include "Queue.hpp"
#include "SceneObject.hpp"
#include "Utils.hpp"
#include "Mesh.hpp"
#include "Particle.hpp"
#include "Sampler.hpp"
#include "UiModule.hpp"
#include "Renderer.hpp"

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
	void Engine::createCubeMap()
	{
		//auto equirectTexture = Utils::loadEquirectangularHDRMap(*this, "../resources/newport_loft.hdr");
		auto equirectTexture = Utils::loadEquirectangularHDRMap(*this, "../resources/HDR_111_Parking_Lot_2_Ref.hdr");

		auto equirectToCubemapDescriptorSet = _descriptorSetManager->allocateDescriptorSets(DescriptorSetLayoutType::OneSampler, 1)[0];

		VkDescriptorImageInfo equirectImageInfo = equirectTexture->getVkDescriptorImageInfo();

		VkWriteDescriptorSet descriptorWrite = Utils::initVkWriteDescriptorSet(equirectToCubemapDescriptorSet, 0,
			VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &equirectImageInfo);

		vkUpdateDescriptorSets(_device.getVkDevice(), 1, &descriptorWrite, 0, nullptr);



		// camera matrices
		glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
		glm::mat4 captureProjViews[] =
		{
			captureProjection * glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			captureProjection * glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			captureProjection * glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
			captureProjection * glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
			captureProjection * glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
			captureProjection * glm::lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f))
		 };


		// equirect to cubemap render pass
		auto commandBuffer = _device.getGraphicsQueue().getPersistentCommandPool().allocateCommandBuffers(1)[0];

		// reset the command buffer and begin a new recording
		vkResetCommandBuffer(commandBuffer, 0);
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; // Optional
		beginInfo.pInheritanceInfo = nullptr; // Optional
		VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

		auto& envCubemapImage = _environmentCubemap->getImage();
		transitionImageLayout(commandBuffer, envCubemapImage.getVkImage(), envCubemapImage.getMipLevels(), VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, envCubemapImage.getArrayLayers());

		for (unsigned int i = 0; i < 6; ++i)
		{
			VkRenderingAttachmentInfo colorAttachment = Renderer::createColorAttachment(envCubemapImage.getSubresourceVkImageView(i, 0));

			auto targetExtent = envCubemapImage.getExtent();
			Renderer::beginRendering(commandBuffer, {{0, 0}, targetExtent}, 1, &colorAttachment, nullptr);

			Renderer::setDynamicStates(commandBuffer, targetExtent);

			// draw
			auto* pipeline = _graphicsPipelines.at(PipelineType::EquirectToCube).get();
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getVkPipeline());

			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getLayout(), 0, 1,
				&equirectToCubemapDescriptorSet, 0, nullptr);

			IblPushConstantData push
			{
				.projView = captureProjViews[i]
			};
			vkCmdPushConstants(commandBuffer, pipeline->getLayout(), VK_SHADER_STAGE_VERTEX_BIT,
				0, sizeof(IblPushConstantData), &push);

			// draw cube
			vkCmdDraw(commandBuffer, 36, 1, 0, 0);

			Renderer::endRendering(commandBuffer);
		}

		transitionImageLayout(commandBuffer, envCubemapImage.getVkImage(), envCubemapImage.getMipLevels(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, envCubemapImage.getArrayLayers());

		// end command buffer recording
		VK_CHECK(vkEndCommandBuffer(commandBuffer));

		// VkSemaphoreCreateInfo semaphoreInfo{};
		// semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		// VkSemaphore semaphore;
		// VK_CHECK(vkCreateSemaphore(_device.getVkDevice(), &semaphoreInfo, nullptr, &semaphore));

		// submit info
		VkSubmitInfo submitInfo
		{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			//wait semaphores
			.waitSemaphoreCount = 0,
			// command buffers
			.commandBufferCount = 1,
			.pCommandBuffers = &commandBuffer,
			// signal semaphore
			.signalSemaphoreCount = 0,
			//.pSignalSemaphores = &semaphore,
		};

		// submit the command buffer (the fence will be signaled when the command buffer finishes executing)
		VK_CHECK(vkQueueSubmit(_device.getGraphicsQueue().getVkQueue(), 1, &submitInfo, nullptr));

		vkDeviceWaitIdle(_device.getVkDevice());

		//generateMipmaps(envCubemapImage); // TODO


		// cubemap to irradiance render pass

		// reset the command buffer and begin a new recording
		vkResetCommandBuffer(commandBuffer, 0);
		beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; // Optional
		beginInfo.pInheritanceInfo = nullptr; // Optional
		VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

		auto& irradianceMapImage = _irradianceCubemap->getImage();
		transitionImageLayout(commandBuffer, irradianceMapImage.getVkImage(), irradianceMapImage.getMipLevels(), VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, irradianceMapImage.getArrayLayers());

		for (unsigned int i = 0; i < 6; ++i)
		{
			VkRenderingAttachmentInfo colorAttachment = Renderer::createColorAttachment(irradianceMapImage.getSubresourceVkImageView(i, 0));

			auto targetExtent = irradianceMapImage.getExtent();
			Renderer::beginRendering(commandBuffer, {{0, 0}, targetExtent}, 1, &colorAttachment, nullptr);

			Renderer::setDynamicStates(commandBuffer, targetExtent);

			// draw
			auto* pipeline = _graphicsPipelines.at(PipelineType::IrradianceConvolution).get();
			vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getVkPipeline());

			VkDescriptorSet descriptorSet = _framesData[0]->skyBoxDescriptorSet;
			vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getLayout(), 0, 1,
				&descriptorSet, 0, nullptr);

			IblPushConstantData push
			{
				.projView = captureProjViews[i]
			};
			vkCmdPushConstants(commandBuffer, pipeline->getLayout(), VK_SHADER_STAGE_VERTEX_BIT,
				0, sizeof(IblPushConstantData), &push);

			// draw cube
			vkCmdDraw(commandBuffer, 36, 1, 0, 0);

			Renderer::endRendering(commandBuffer);
		}

		transitionImageLayout(commandBuffer, irradianceMapImage.getVkImage(), irradianceMapImage.getMipLevels(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, irradianceMapImage.getArrayLayers());

		// end command buffer recording
		VK_CHECK(vkEndCommandBuffer(commandBuffer));

		// VkSemaphoreCreateInfo semaphoreInfo{};
		// semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		// VkSemaphore semaphore;
		// VK_CHECK(vkCreateSemaphore(_device.getVkDevice(), &semaphoreInfo, nullptr, &semaphore));

		// submit info
		submitInfo =
		{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			//wait semaphores
			.waitSemaphoreCount = 0,
			// command buffers
			.commandBufferCount = 1,
			.pCommandBuffers = &commandBuffer,
			// signal semaphore
			.signalSemaphoreCount = 0,
			//.pSignalSemaphores = &semaphore,
		};

		// submit the command buffer (the fence will be signaled when the command buffer finishes executing)
		VK_CHECK(vkQueueSubmit(_device.getGraphicsQueue().getVkQueue(), 1, &submitInfo, nullptr));

		vkDeviceWaitIdle(_device.getVkDevice()); // TODO use fence and semaphores




		// -------------- PREFILTER ENV ----------------------

		// reset the command buffer and begin a new recording
		vkResetCommandBuffer(commandBuffer, 0);
		beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; // Optional
		beginInfo.pInheritanceInfo = nullptr; // Optional
		VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

		auto& prefEnvImage = _prefilteredEnvCubemap->getImage();
		uint32_t prefEnvImgMipLevels = prefEnvImage.getMipLevels();
		transitionImageLayout(commandBuffer, prefEnvImage.getVkImage(), prefEnvImgMipLevels, VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, prefEnvImage.getArrayLayers());

		for (unsigned int mipLevel = 0; mipLevel < prefEnvImgMipLevels; ++mipLevel)
		{
			for (unsigned int face = 0; face < 6; ++face)
			{
				VkRenderingAttachmentInfo colorAttachment = Renderer::createColorAttachment(prefEnvImage.getSubresourceVkImageView(face, mipLevel));

				// extent according to mip-level.
				auto targetSize = prefEnvImage.getExtent().width >> mipLevel;
				VkExtent2D targetExtent = {targetSize, targetSize };

				Renderer::beginRendering(commandBuffer, {{0, 0}, targetExtent}, 1,
					&colorAttachment, nullptr);

				Renderer::setDynamicStates(commandBuffer, targetExtent);

				// draw
				auto* pipeline = _graphicsPipelines.at(PipelineType::PrefilterEnv).get();
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getVkPipeline());

				VkDescriptorSet descriptorSet = _framesData[0]->skyBoxDescriptorSet;
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getLayout(), 0, 1,
					&descriptorSet, 0, nullptr);

				IblPushConstantData push
				{
					.projView = captureProjViews[face],
					.roughness =  static_cast<float>(mipLevel) / static_cast<float>(prefEnvImgMipLevels - 1)
				};
				vkCmdPushConstants(commandBuffer, pipeline->getLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
					0, sizeof(IblPushConstantData), &push);

				// draw cube
				vkCmdDraw(commandBuffer, 36, 1, 0, 0);

				Renderer::endRendering(commandBuffer);
			}
		}

		transitionImageLayout(commandBuffer, prefEnvImage.getVkImage(), prefEnvImgMipLevels, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, prefEnvImage.getArrayLayers());

		// end command buffer recording
		VK_CHECK(vkEndCommandBuffer(commandBuffer));

		// VkSemaphoreCreateInfo semaphoreInfo{};
		// semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		// VkSemaphore semaphore;
		// VK_CHECK(vkCreateSemaphore(_device.getVkDevice(), &semaphoreInfo, nullptr, &semaphore));

		// submit info
		submitInfo =
		{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			//wait semaphores
			.waitSemaphoreCount = 0,
			// command buffers
			.commandBufferCount = 1,
			.pCommandBuffers = &commandBuffer,
			// signal semaphore
			.signalSemaphoreCount = 0,
			//.pSignalSemaphores = &semaphore,
		};

		// submit the command buffer (the fence will be signaled when the command buffer finishes executing)
		VK_CHECK(vkQueueSubmit(_device.getGraphicsQueue().getVkQueue(), 1, &submitInfo, nullptr));

		vkDeviceWaitIdle(_device.getVkDevice()); // TODO use fence and semaphores


		// -------------- BRDF LUT ----------------------

		// reset the command buffer and begin a new recording
		vkResetCommandBuffer(commandBuffer, 0);
		beginInfo = {};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
		beginInfo.flags = 0; // Optional
		beginInfo.pInheritanceInfo = nullptr; // Optional
		VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

		auto& brdfLutImage = _brdfLUT->getImage();
		transitionImageLayout(commandBuffer, brdfLutImage.getVkImage(), brdfLutImage.getMipLevels(), VK_IMAGE_LAYOUT_UNDEFINED,
			VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, brdfLutImage.getArrayLayers());


		{

			{
				VkRenderingAttachmentInfo colorAttachment = Renderer::createColorAttachment(brdfLutImage.getVkImageView());

				VkExtent2D targetExtent = brdfLutImage.getExtent();

				Renderer::beginRendering(commandBuffer, {{0, 0}, targetExtent}, 1,
					&colorAttachment, nullptr);

				Renderer::setDynamicStates(commandBuffer, targetExtent);

				// draw
				auto* pipeline = _graphicsPipelines.at(PipelineType::BrdfLUT).get();
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getVkPipeline());

				VkDescriptorSet descriptorSet = _framesData[0]->skyBoxDescriptorSet;
				vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getLayout(), 0, 1,
					&descriptorSet, 0, nullptr);

				// draw cube
				vkCmdDraw(commandBuffer, 3, 1, 0, 0);

				Renderer::endRendering(commandBuffer);
			}
		}

		transitionImageLayout(commandBuffer, brdfLutImage.getVkImage(), brdfLutImage.getMipLevels(), VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT, brdfLutImage.getArrayLayers());

		// end command buffer recording
		VK_CHECK(vkEndCommandBuffer(commandBuffer));

		// VkSemaphoreCreateInfo semaphoreInfo{};
		// semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
		// VkSemaphore semaphore;
		// VK_CHECK(vkCreateSemaphore(_device.getVkDevice(), &semaphoreInfo, nullptr, &semaphore));

		// submit info
		submitInfo =
		{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			//wait semaphores
			.waitSemaphoreCount = 0,
			// command buffers
			.commandBufferCount = 1,
			.pCommandBuffers = &commandBuffer,
			// signal semaphore
			.signalSemaphoreCount = 0,
			//.pSignalSemaphores = &semaphore,
		};

		// submit the command buffer (the fence will be signaled when the command buffer finishes executing)
		VK_CHECK(vkQueueSubmit(_device.getGraphicsQueue().getVkQueue(), 1, &submitInfo, nullptr));

		vkDeviceWaitIdle(_device.getVkDevice()); // TODO use fence and semaphores

	}

	Engine::Engine(EngineConfig config) : _config(config)
	{
		Log::Get().Info("Engine constructor");

		recreateSwapChain();
		_descriptorSetManager = std::make_unique<DescriptorSetManager>(_device);
		createShadowMapTexture();
		createEnvironmentTextures();

		createPipelines();

		_materialPhongUboAlignment = _device.getUniformBufferAlignment(sizeof(MaterialUbo));
		_materialPbrUboAlignment = _device.getUniformBufferAlignment(sizeof(MaterialPbrUbo));
		createFramesResources();
		createDefaultTextures();
		initLights();
		initParticles();
		updateDescriptorSets();

		createSyncObjects();

		_gui = std::make_unique<UiModule>(*this, _device, _window, *_swapChain);

		createCubeMap();
	}

	Engine::~Engine()
	{
		// wait for the GPU to finish all operations before destroying the resources
		vkDeviceWaitIdle(_device.getVkDevice());

		_gui.reset(); // destroy first

		// destroy texture, image and samplers
		_materials.clear();

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

			if (_config.uiEnabled)
				_gui->build(); // must be called at each frame

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
		if (_config.particlesEnabled)
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
		std::vector<VkSemaphore> waitSemaphores;
		std::vector<VkPipelineStageFlags> waitStages;
		waitSemaphores.push_back(_imageAvailableSems[swapChainImageIndex]);
		waitStages.push_back(VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT);

		if (_config.particlesEnabled)
		{
			waitSemaphores.push_back(frameData.computeCmdExecutedSem);
			waitStages.push_back(VK_PIPELINE_STAGE_VERTEX_INPUT_BIT);
		}

		// specify which semaphores to signal once the command buffer has finished executing
		VkSemaphore cmdExecutedSignalSemaphores[] = {_drawCmdExecutedSems[swapChainImageIndex]};

		// submit info
		VkSubmitInfo submitInfo
		{
			.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
			//wait semaphores
			.waitSemaphoreCount = static_cast<uint32_t>(waitSemaphores.size()),
			.pWaitSemaphores = waitSemaphores.data(),
			.pWaitDstStageMask = waitStages.data(),
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
		frameUbo.shadowsEnabled = _config.shadowsEnabled ? 1 : 0;

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
		auto defaultPipeline = _config.lightingType == LightingType::BlinnPhong ? PipelineType::PhongLighting : PipelineType::PbrLighting;

		auto currentPipelineType = defaultPipeline;

		// bind default pipeline
		Pipeline* currentPipeline = _graphicsPipelines.at(currentPipelineType).get();
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipeline->getVkPipeline());

		// bind frame descriptor set
		VkDescriptorSet descriptorSet0 = _framesData[_currentFrame]->frameDescriptorSet;
    	vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipeline->getLayout(), 0, 1, &descriptorSet0, 0, nullptr);

		// bind default material descriptor set
		VkDescriptorSet descriptorSetMat = _defaultMaterial->getDescriptorSet(currentPipelineType);
		uint32_t dynOff = 0;
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipeline->getLayout(), 1, 1, &descriptorSetMat, 1, &dynOff);
		_currentMaterialName = DEFAULT_MATERIAL_NAME;

		for (auto &obj: _sceneObjects)
		{
			//updateObjectUbo(*obj); // TODO: how to update the object ubo instead of using push constants?

			auto objPipeLineType = obj->PipelineKey.value_or(defaultPipeline);

			// determine which pipeline to use for this object
			if (objPipeLineType != currentPipelineType)
			{
				currentPipelineType = objPipeLineType;
				_currentMaterialName = "";

				// bind pipeline
				currentPipeline = _graphicsPipelines.at(currentPipelineType).get();
				vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, currentPipeline->getVkPipeline());

				// bind descriptor set
				descriptorSet0 = _framesData[_currentFrame]->frameDescriptorSet;
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
					uint32_t dynamicOffset = material.uboIndex * (currentPipelineType == PipelineType::PbrLighting
						                                              ? _materialPbrUboAlignment
						                                              : _materialPhongUboAlignment);

					VkDescriptorSet descriptorSet = material.getDescriptorSet(currentPipelineType);
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

	void Engine::drawSkyBox(VkCommandBuffer commandBuffer) const
	{
		Pipeline* pipeline = _graphicsPipelines.at(PipelineType::SkyBox).get();
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getVkPipeline());

		VkDescriptorSet descriptorSet = _framesData[_currentFrame]->skyBoxDescriptorSet;
		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getLayout(), 0, 1,
			&descriptorSet, 0, nullptr);

		// push constants
		IblPushConstantData push
		{
			.projView = _camera.getProjectionMatrix() * glm::mat4(glm::mat3(_camera.getViewMatrix())) // remove translation from view matrix
		};
		vkCmdPushConstants(commandBuffer, pipeline->getLayout(), VK_SHADER_STAGE_VERTEX_BIT,
			0, sizeof(IblPushConstantData), &push);

		// draw cube
		vkCmdDraw(commandBuffer, 36, 1, 0, 0);
	}

	void Engine::drawParticles(VkCommandBuffer commandBuffer) const
	{
		Pipeline *particlePipeline = _graphicsPipelines.at(PipelineType::Particles).get();
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, particlePipeline->getVkPipeline());

		VkDescriptorSet descriptorSet = _framesData[_currentFrame]->frameDescriptorSet;
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

		if (_config.shadowsEnabled)
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
		VkImageView swapChainImageView = _swapChain->getSwapChainImageView(swapChainImageIndex);

		// TODO: should I use the real current layout instead of undefined?
		// TODO: should I set the layout at each frame even if is not changing (e.g. depthImage). Transition is not only for changing the layout but also to set the memory barriers

		// transition the msaa and color image to COLOR_ATTACHMENT_OPTIMAL
		transitionImageLayout(commandBuffer, colorImage.getVkImage(), 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
		if (_config.msaaEnabled) transitionImageLayout(commandBuffer, msaaImage.getVkImage(), msaaImage.getMipLevels(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

		// transition the depth image to DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		transitionImageLayout(commandBuffer, depthImage.getVkImage(), depthImage.getMipLevels(), VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

		// choose the render target image
		Image& renderTarget = _config.msaaEnabled ? msaaImage : colorImage;
		auto extent = renderTarget.getExtent();

		// set the color attachment
		VkRenderingAttachmentInfo colorAttachment = Renderer::createColorAttachment(renderTarget.getVkImageView());

		// set resolve image if msaa is enable
		if (_config.msaaEnabled)
		{
			colorAttachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
			colorAttachment.resolveImageView = colorImage.getVkImageView();
			colorAttachment.resolveImageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
			colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; // Optimization: don't save the multi-sample image
		}

		// set depth attachment
		VkRenderingAttachmentInfo depthAttachment = Renderer::createDepthAttachment(depthImage.getVkImageView());

		// begin rendering
		Renderer::beginRendering(commandBuffer, {{0, 0}, extent}, 1, &colorAttachment, &depthAttachment);

		// set dynamic states
		Renderer::setDynamicStates(commandBuffer, extent);

		// draw objects
		drawObjectsLoop(commandBuffer);

		drawSkyBox(commandBuffer); // TODO flag ui

		// draw particles
		if (_config.particlesEnabled)
			drawParticles(commandBuffer);

		// end rendering
		Renderer::endRendering(commandBuffer);

		// transition the color image and the swapchain image into their correct transfer layouts
		transitionImageLayout(commandBuffer, colorImage.getVkImage(), 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);
		transitionImageLayout(commandBuffer, swapChainImage, 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

		// copy the color image into the swapchain image
		copyImageToImage(commandBuffer, colorImage.getVkImage(), swapChainImage, colorImage.getExtent(), _swapChain->getExtent());

		if (_config.uiEnabled)
		{
			// set the spawChain image layout to color attachment optimal to render ui on it
			transitionImageLayout(commandBuffer, swapChainImage, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

			// draw the ui
			_gui->draw(commandBuffer, swapChainImageView, {0, 0, _swapChain->getExtent()});

			// set the swapChain image layout to Present to show it on the screen
			transitionImageLayout(commandBuffer, swapChainImage, 1, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_ASPECT_COLOR_BIT);
		}
		else
		{
			// set the swapchain image layout to Present to show it on the screen
			transitionImageLayout(commandBuffer, swapChainImage, 1, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR, VK_IMAGE_ASPECT_COLOR_BIT);
		}

		// end command buffer recording
		VK_CHECK(vkEndCommandBuffer(commandBuffer));
	}

	void Engine::recordComputeCommands(VkCommandBuffer commandBuffer)
	{
		VkCommandBufferBeginInfo beginInfo{};
		beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

		VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipeline->getVkPipeline());
		VkDescriptorSet descriptorSet = _framesData[_currentFrame]->computeParticleDescriptorSet;
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
			.samples = _config.msaaEnabled ? _device.getMaxMsaaSamples() : VK_SAMPLE_COUNT_1_BIT,
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

	void Engine::createEnvironmentTextures()
	{
		auto samplerCreateInfo = Sampler::getDefaultCreateInfo();
		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
		auto sampler = std::make_shared<Sampler>(_device, &samplerCreateInfo);

		// Environment cubemap
		ImageParams imageParams
		{
			.extent = ENVIRONMENT_CUBEMAP_RESOLUTION,
			.format = ENVIRONMENT_CUBEMAP_FORMAT,
			.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
			.usage = Texture::getImageUsageFlags() | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.mipLevels = 1,// Texture::computeMipLevels(ENVIRONMENT_CUBEMAP_RESOLUTION.width, ENVIRONMENT_CUBEMAP_RESOLUTION.height),
			.arrayLayers = 6,
		};
		auto envCubemapImage = std::make_shared<Image>(_device, imageParams);

		_environmentCubemap = std::make_unique<Texture>(_device, std::move(envCubemapImage), sampler);

		// irradiance cubemap
		imageParams =
		{
			.extent = IRRADIANCE_CUBEMAP_RESOLUTION,
			.format = ENVIRONMENT_CUBEMAP_FORMAT,
			.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
			.usage = Texture::getImageUsageFlags() | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.mipLevels = 1,
			.arrayLayers = 6,
		};
		auto irradianceCubemapImage = std::make_shared<Image>(_device, imageParams);
		_irradianceCubemap = std::make_unique<Texture>(_device, std::move(irradianceCubemapImage), sampler);

		// prefiltered cubemap
		imageParams =
		{
			.extent = PREFILTERED_ENV_CUBEMAP_RESOLUTION,
			.format = ENVIRONMENT_CUBEMAP_FORMAT,
			.flags = VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT,
			.usage = Texture::getImageUsageFlags() | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.mipLevels = PREFILTERED_ENV_CUBEMAP_MIP_LEVELS,
			.arrayLayers = 6,
		};
		auto prefilterEnvCubemapImage = std::make_shared<Image>(_device, imageParams);
		_prefilteredEnvCubemap = std::make_unique<Texture>(_device, std::move(prefilterEnvCubemapImage), sampler);

		// BRDF Look-Up Texture
		imageParams =
		{
			.extent = BRDF_LUT_RESOLUTION,
			.format = BRDF_LUT_FORMAT,
			.usage = Texture::getImageUsageFlags() | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
			.mipLevels = 1
		};
		auto brdfImage = std::make_shared<Image>(_device, imageParams);
		_brdfLUT = std::make_unique<Texture>(_device, std::move(brdfImage), sampler);
	}

	void Engine::createShadowMapTexture()
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
			.extent = SHADOW_MAP_RESOLUTION,
			.format = shadowImageFormat,
			.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
			.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT,
			.memoryProps = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT, // dedicated allocation for special, big resources, like fullscreen images used as attachments
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
		auto shadowSampler = std::make_unique<Sampler>(_device, &samplerInfo);

		// create the shadow map texture
		_shadowMap = std::make_unique<Texture>(_device, std::move(shadowMapImage), std::move(shadowSampler));
	}

	void Engine::recordShadowMappingPass(VkCommandBuffer commandBuffer) const
	{
		Image& shadowMapImage = _shadowMap->getImage();

		// transition the layout to DEPTH_STENCIL_ATTACHMENT_OPTIMAL
		transitionImageLayout(commandBuffer, shadowMapImage.getVkImage(), 1, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);

		auto extent = shadowMapImage.getExtent();

		// set depth attachment
		VkRenderingAttachmentInfo depthAttachment = Renderer::createDepthAttachment(shadowMapImage.getVkImageView());

		// begin rendering
		Renderer::beginRendering(commandBuffer, {{0, 0}, extent}, 0, nullptr, &depthAttachment);

		// set dynamic states
		Renderer::setDynamicStates(commandBuffer, extent);

		// bind shadow mapping pipeline
		Pipeline* pipeline = _graphicsPipelines.at(PipelineType::ShadowMapping).get();
		vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->getVkPipeline());

		// bind frame descriptor set
		VkDescriptorSet descriptorSet = _framesData[_currentFrame]->frameDescriptorSet;
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
			vkCmdPushConstants(commandBuffer, pipeline->getLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantData), &push);

			// draw the mesh
			obj->Mesh->draw(commandBuffer);
		}

		// end rendering
		Renderer::endRendering(commandBuffer);

		// transition layout SHADER_READ_ONLY_OPTIMAL
		transitionImageLayout(commandBuffer, shadowMapImage.getVkImage(), 1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
	}

	void Engine::createPipelines()
	{
		_graphicsPipelines.clear();
		_computePipeline.reset();

		// Shadow mapping
		GraphicsPipelineBuilder builder{};
		builder.addSetLayout(_descriptorSetManager->getDescriptorSetLayout(DescriptorSetLayoutType::Frame))
		       .setDepthAttachmentFormat(_shadowMap->getImage().getFormat())
		       .addShaderStage(R"(..\shaders\compiled\shadow.vert.spv)", VK_SHADER_STAGE_VERTEX_BIT)
		       // front face culling to fix peter panning artifacts, but works only for 3D solid objects, not for planes/surfaces
		       .setCullModeFlags(VK_CULL_MODE_FRONT_BIT);
		_graphicsPipelines.emplace(PipelineType::ShadowMapping, builder.build(_device));

		// No lights
		builder = {};
		builder.addSetLayout(_descriptorSetManager->getDescriptorSetLayout(DescriptorSetLayoutType::Frame))
		       .addColorAttachment(_swapChain->getSwapChainImageFormat())
		       .setDepthAttachmentFormat(_swapChain->getDepthImage().getFormat())
		       .addShaderStage(R"(..\shaders\compiled\noLight.vert.spv)", VK_SHADER_STAGE_VERTEX_BIT)
		       .addShaderStage(R"(..\shaders\compiled\noLight.frag.spv)", VK_SHADER_STAGE_FRAGMENT_BIT)
		       .setSamples(_swapChain->getSamples());
		_graphicsPipelines.emplace(PipelineType::NoLight, builder.build(_device));

		// PhongLighting
		builder = {};
		builder.addSetLayout(_descriptorSetManager->getDescriptorSetLayout(DescriptorSetLayoutType::Frame)) // set 0
		       .addSetLayout(_descriptorSetManager->getDescriptorSetLayout(DescriptorSetLayoutType::MaterialPhong)) // set 1
			   .addColorAttachment(_swapChain->getSwapChainImageFormat())
			   .setDepthAttachmentFormat(_swapChain->getDepthImage().getFormat())
			   .addShaderStage(R"(..\shaders\compiled\phong.vert.spv)", VK_SHADER_STAGE_VERTEX_BIT)
			   .addShaderStage(R"(..\shaders\compiled\phong.frag.spv)", VK_SHADER_STAGE_FRAGMENT_BIT)
			   .setSamples(_swapChain->getSamples());
		_graphicsPipelines.emplace(PipelineType::PhongLighting, builder.build(_device));

		// PbrLighting
		builder = {};
		builder.addSetLayout(_descriptorSetManager->getDescriptorSetLayout(DescriptorSetLayoutType::Frame)) // set 0
			   .addSetLayout(_descriptorSetManager->getDescriptorSetLayout(DescriptorSetLayoutType::MaterialPbr)) // set 1
			   .addColorAttachment(_swapChain->getSwapChainImageFormat())
			   .setDepthAttachmentFormat(_swapChain->getDepthImage().getFormat())
			   .addShaderStage(R"(..\shaders\compiled\pbr.vert.spv)", VK_SHADER_STAGE_VERTEX_BIT)
			   .addShaderStage(R"(..\shaders\compiled\pbr.frag.spv)", VK_SHADER_STAGE_FRAGMENT_BIT)
			   .setSamples(_swapChain->getSamples());
		_graphicsPipelines.emplace(PipelineType::PbrLighting, builder.build(_device));

		// Particles
		builder = {};
		builder.addSetLayout(_descriptorSetManager->getDescriptorSetLayout(DescriptorSetLayoutType::Frame)) // set 0
			   .addColorAttachment(_swapChain->getSwapChainImageFormat())
			   .setDepthAttachmentFormat(_swapChain->getDepthImage().getFormat())
			   .addShaderStage(R"(..\shaders\compiled\particle.vert.spv)", VK_SHADER_STAGE_VERTEX_BIT)
			   .addShaderStage(R"(..\shaders\compiled\particle.frag.spv)", VK_SHADER_STAGE_FRAGMENT_BIT)
			   .setPrimitiveTopology(VK_PRIMITIVE_TOPOLOGY_POINT_LIST)
			   .setSamples(_swapChain->getSamples());
		_graphicsPipelines.emplace(PipelineType::Particles, builder.build(_device));

		// SkyBox
		builder = {};
		builder.addSetLayout(_descriptorSetManager->getDescriptorSetLayout(DescriptorSetLayoutType::OneSampler)) // set 0
			   .addColorAttachment(_swapChain->getSwapChainImageFormat())
			   .setDepthAttachmentFormat(_swapChain->getDepthImage().getFormat())
			   .clearVertexInput()
			   .addShaderStage(R"(..\shaders\compiled\skyBox.vert.spv)", VK_SHADER_STAGE_VERTEX_BIT)
			   .addShaderStage(R"(..\shaders\compiled\skyBox.frag.spv)", VK_SHADER_STAGE_FRAGMENT_BIT)
			   .setDepthCompareOp(VK_COMPARE_OP_LESS_OR_EQUAL)
			   .clearPushConstantRanges().addPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(IblPushConstantData))
			   .setSamples(_swapChain->getSamples());
		_graphicsPipelines.emplace(PipelineType::SkyBox, builder.build(_device));

		// Equirect to cube map
		builder = {};
		builder.addSetLayout(_descriptorSetManager->getDescriptorSetLayout(DescriptorSetLayoutType::OneSampler))
			   .addColorAttachment(ENVIRONMENT_CUBEMAP_FORMAT)
			   .clearVertexInput()
			   .addShaderStage(R"(..\shaders\compiled\cubeNDC.vert.spv)", VK_SHADER_STAGE_VERTEX_BIT)
			   .addShaderStage(R"(..\shaders\compiled\equirectToCube.frag.spv)", VK_SHADER_STAGE_FRAGMENT_BIT)
			   .clearPushConstantRanges().addPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(IblPushConstantData));
		_graphicsPipelines.emplace(PipelineType::EquirectToCube, builder.build(_device));

		// Irradiance convolution
		builder = {};
		builder.addSetLayout(_descriptorSetManager->getDescriptorSetLayout(DescriptorSetLayoutType::OneSampler))
			   .addColorAttachment(ENVIRONMENT_CUBEMAP_FORMAT)
			   .clearVertexInput()
			   .addShaderStage(R"(..\shaders\compiled\cubeNDC.vert.spv)", VK_SHADER_STAGE_VERTEX_BIT)
			   .addShaderStage(R"(..\shaders\compiled\irradianceConvolution.frag.spv)", VK_SHADER_STAGE_FRAGMENT_BIT)
			   .clearPushConstantRanges().addPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(IblPushConstantData));
		_graphicsPipelines.emplace(PipelineType::IrradianceConvolution, builder.build(_device));

		// Prefilter env
		builder = {};
		builder.addSetLayout(_descriptorSetManager->getDescriptorSetLayout(DescriptorSetLayoutType::OneSampler))
			   .addColorAttachment(ENVIRONMENT_CUBEMAP_FORMAT)
			   .clearVertexInput()
			   .addShaderStage(R"(..\shaders\compiled\cubeNDC.vert.spv)", VK_SHADER_STAGE_VERTEX_BIT)
			   .addShaderStage(R"(..\shaders\compiled\prefilterEnv.frag.spv)", VK_SHADER_STAGE_FRAGMENT_BIT)
			   .clearPushConstantRanges().addPushConstantRange(VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(IblPushConstantData));
		_graphicsPipelines.emplace(PipelineType::PrefilterEnv, builder.build(_device));

		// BRDF LUT
		builder = {};
		builder.addSetLayout(_descriptorSetManager->getDescriptorSetLayout(DescriptorSetLayoutType::OneSampler))
			   .addColorAttachment(BRDF_LUT_FORMAT)
			   .clearVertexInput()
		.setCullModeFlags(VK_CULL_MODE_NONE)
			   .addShaderStage(R"(..\shaders\compiled\quadNDC.vert.spv)", VK_SHADER_STAGE_VERTEX_BIT)
			   .addShaderStage(R"(..\shaders\compiled\brdfLUT.frag.spv)", VK_SHADER_STAGE_FRAGMENT_BIT)
			   .clearPushConstantRanges();
		_graphicsPipelines.emplace(PipelineType::BrdfLUT, builder.build(_device));

		// Compute
		ComputePipelineBuilder computeBuilder{};
		computeBuilder.addSetLayout(_descriptorSetManager->getDescriptorSetLayout(DescriptorSetLayoutType::ComputeParticles))
		              .setShader(R"(..\shaders\compiled\particle.comp.spv)");
		_computePipeline = computeBuilder.build(_device);
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
		auto descriptorSets = _descriptorSetManager->allocateDescriptorSets(DescriptorSetLayoutType::Frame, FRAMES_IN_FLIGHT);
		auto skyBoxDescriptorSets = _descriptorSetManager->allocateDescriptorSets(DescriptorSetLayoutType::OneSampler, FRAMES_IN_FLIGHT);
		auto computeParticlesDescSet = _descriptorSetManager->allocateDescriptorSets(DescriptorSetLayoutType::ComputeParticles, FRAMES_IN_FLIGHT);
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

			_framesData[i]->skyBoxDescriptorSet = skyBoxDescriptorSets[i];
			_framesData[i]->computeParticleDescriptorSet = computeParticlesDescSet[i];

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
		_lightsUbo.lights[1].posDir = glm::vec4(-0.5f, -.8f, -1.f, 0.0f); // w=0 => dir light
		_lightsUbo.lights[1].color = glm::vec4(1.0f, 1.0f, 1.0f, 4.f);

		// Point light
		_lightsUbo.lights[0].posDir = glm::vec4(5.2f, 6.2f, -5.2f, 1.0f); // w=1 => point light
		_lightsUbo.lights[0].color = glm::vec4(1.0f, 1.0f, 1.0f, 2.0f);
		_lightsUbo.lights[0].attenuation = glm::vec4(1.0f, 0.09f, 0.032f, 0.0f);

		// Create the lights ubo with device local memory for better performance
		VkDeviceSize lightsUboSize = sizeof(LightsUbo);
        _lightsUboBuffer = std::make_unique<Buffer>(_device, lightsUboSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

		// upload lights data to buffer
		Utils::uploadToDeviceBuffer(_device, *_lightsUboBuffer, lightsUboSize, &_lightsUbo);
	}

	void Engine::updateDescriptorSets() const
	{
		// get buffers and images info
		VkDescriptorBufferInfo lightUboInfo = _lightsUboBuffer->getVkDescriptorBufferInfo();
	    VkDescriptorImageInfo shadowMapImageInfo = _shadowMap->getVkDescriptorImageInfo();
		VkDescriptorImageInfo envImageInfo = _environmentCubemap->getVkDescriptorImageInfo();
		VkDescriptorImageInfo irradianceImageInfo = _irradianceCubemap->getVkDescriptorImageInfo();
		VkDescriptorImageInfo prefilteredImageInfo = _prefilteredEnvCubemap->getVkDescriptorImageInfo();
		VkDescriptorImageInfo brdfLUTImageInfo = _brdfLUT->getVkDescriptorImageInfo();

	    // update each DescriptorSet
	    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
	    {
	    	auto& frameResources = _framesData[i];

			//---------- FRAME DESCRIPTOR SET ---------------//
	    	auto frameDescriptorSet = frameResources->frameDescriptorSet;

		    auto objectUboInfo = frameResources->objectUboBuffer->getVkDescriptorBufferInfo();
			auto objectUboWrite = Utils::initVkWriteDescriptorSet(frameDescriptorSet, 0,  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &objectUboInfo);

	    	auto frameUboInfo = frameResources->frameUboBuffer->getVkDescriptorBufferInfo();
	    	auto frameUboWrite = Utils::initVkWriteDescriptorSet(frameDescriptorSet, 1,  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &frameUboInfo);

	    	auto lightsUboWrite = Utils::initVkWriteDescriptorSet(frameDescriptorSet, 2,  VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, &lightUboInfo);
	    	auto shadowMapWrite = Utils::initVkWriteDescriptorSet(frameDescriptorSet, 3,  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &shadowMapImageInfo);
	    	auto irradianceMapWrite = Utils::initVkWriteDescriptorSet(frameDescriptorSet, 4,  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &irradianceImageInfo);
	    	auto prefilteredMapWrite = Utils::initVkWriteDescriptorSet(frameDescriptorSet, 5,  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &prefilteredImageInfo);
	    	auto brdfLUTMapWrite = Utils::initVkWriteDescriptorSet(frameDescriptorSet, 6,  VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &brdfLUTImageInfo);

		    std::array descriptorWrites =
		    {
			    objectUboWrite, frameUboWrite, lightsUboWrite, shadowMapWrite, irradianceMapWrite, prefilteredMapWrite, brdfLUTMapWrite
		    };

		    vkUpdateDescriptorSets(_device.getVkDevice(), descriptorWrites.size(),
		                           descriptorWrites.data(), 0, nullptr);

	    	//---------- COMPUTE PARTICLE DESCRIPTOR SET ---------------//
	    	auto particleDescriptorSet = frameResources->computeParticleDescriptorSet;
	    	// Particles Ssbo previous frame
	    	VkDescriptorBufferInfo particlesSsboInfoPrevFrame{};
	    	particlesSsboInfoPrevFrame.buffer = _framesData[(i - 1) % FRAMES_IN_FLIGHT]->particleSSboBuffer->getVkBuffer();
	    	particlesSsboInfoPrevFrame.offset = 0;
	    	particlesSsboInfoPrevFrame.range = sizeof(Particle) * PARTICLES_COUNT;

		    VkWriteDescriptorSet particlesDescriptorWritePrevFrame = Utils::initVkWriteDescriptorSet(particleDescriptorSet, 0,
			    VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &particlesSsboInfoPrevFrame);

	    	// Particles Ssbo current frame
	    	VkDescriptorBufferInfo particlesSsboInfoCurrentFrame{};
	    	particlesSsboInfoCurrentFrame.buffer = _framesData[i]->particleSSboBuffer->getVkBuffer();
	    	particlesSsboInfoCurrentFrame.offset = 0;
	    	particlesSsboInfoCurrentFrame.range = sizeof(Particle) * PARTICLES_COUNT;

	    	VkWriteDescriptorSet particlesDescriptorWriteCurrentFrame = Utils::initVkWriteDescriptorSet(particleDescriptorSet, 1,
	    		VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, &particlesSsboInfoCurrentFrame);

	    	std::array dw =
	    	{
	    		particlesDescriptorWritePrevFrame, particlesDescriptorWriteCurrentFrame
			};

	    	vkUpdateDescriptorSets(_device.getVkDevice(), dw.size(),
								   dw.data(), 0, nullptr);

	    	//---------- SKY BOX DESCRIPTOR SET ---------------//
	    	VkWriteDescriptorSet envDescriptorWrite = Utils::initVkWriteDescriptorSet(_framesData[i]->skyBoxDescriptorSet, 0,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &envImageInfo);

	    	std::array dw2 =
	    	{
	    		envDescriptorWrite
			};

	    	vkUpdateDescriptorSets(_device.getVkDevice(), dw2.size(),
								   dw2.data(), 0, nullptr);
	    }
    }

	void Engine::updateMaterialDescriptorSets(const Material& material) const
	{
		VkDescriptorImageInfo baseColorImageInfo = material.baseColorMap->getVkDescriptorImageInfo();
		VkDescriptorImageInfo specularImageInfo = material.specularMap->getVkDescriptorImageInfo();
		VkDescriptorImageInfo normalImageInfo = material.normalMap->getVkDescriptorImageInfo();
		VkDescriptorImageInfo metallicRoughnessImageInfo = material.metallicRoughnessMap->getVkDescriptorImageInfo();
		VkDescriptorImageInfo occlusionImageInfo = material.occlusionMap->getVkDescriptorImageInfo();
		VkDescriptorImageInfo emissiveImageInfo = material.emissiveMap->getVkDescriptorImageInfo();

		for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			auto& frameResources = _framesData[i];

			//---------- PHONG DESCRIPTOR SET ---------------//
			VkDescriptorBufferInfo materialDynUboInfo = frameResources->materialDynUboBuffer->getVkDescriptorBufferInfo();
			materialDynUboInfo.range = _materialPhongUboAlignment;

			auto materialDynUboWrite = Utils::initVkWriteDescriptorSet(material.descriptorSetPhong, 0,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &materialDynUboInfo);

			auto diffuseTextDescriptorWrite = Utils::initVkWriteDescriptorSet(material.descriptorSetPhong, 1,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &baseColorImageInfo);

			auto specularDescriptorWrite = Utils::initVkWriteDescriptorSet(material.descriptorSetPhong, 2,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &specularImageInfo);

			std::array descriptorWrites =
			{
				materialDynUboWrite, diffuseTextDescriptorWrite, specularDescriptorWrite
			};

			vkUpdateDescriptorSets(_device.getVkDevice(), static_cast<uint32_t>(descriptorWrites.size()),
								   descriptorWrites.data(), 0, nullptr);

			//---------- PBR DESCRIPTOR SET ---------------//
			VkDescriptorBufferInfo materialPbrDynUboInfo = frameResources->materialPbrDynUboBuffer->getVkDescriptorBufferInfo();
			materialPbrDynUboInfo.range = _materialPbrUboAlignment;

			auto materialPbrDynUboWrite = Utils::initVkWriteDescriptorSet(material.descriptorSetPbr, 0,
				VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, &materialPbrDynUboInfo);

			auto baseColorDescriptorWrite = Utils::initVkWriteDescriptorSet(material.descriptorSetPbr, 1,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &baseColorImageInfo);

			auto normalDescriptorWrite = Utils::initVkWriteDescriptorSet(material.descriptorSetPbr, 2,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &normalImageInfo);

			auto metallicRoughnessDescriptorWrite = Utils::initVkWriteDescriptorSet(material.descriptorSetPbr, 3,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &metallicRoughnessImageInfo);

			auto aoDescriptorWrite = Utils::initVkWriteDescriptorSet(material.descriptorSetPbr, 4,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &occlusionImageInfo);

			auto emissiveDescriptorWrite = Utils::initVkWriteDescriptorSet(material.descriptorSetPbr, 5,
				VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, nullptr, &emissiveImageInfo);

			std::array descriptorPbrWrites =
			{
				materialPbrDynUboWrite, baseColorDescriptorWrite, normalDescriptorWrite, metallicRoughnessDescriptorWrite,
				aoDescriptorWrite, emissiveDescriptorWrite
			};

			vkUpdateDescriptorSets(_device.getVkDevice(), static_cast<uint32_t>(descriptorPbrWrites.size()),
				descriptorPbrWrites.data(), 0, nullptr);
		}
	}

	void Engine::compileSceneObjects() const
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
		std::vector<MaterialPbrUbo> materialPbrUbos;
		materialUbos.emplace_back(*_defaultMaterial);
		materialPbrUbos.emplace_back(*_defaultMaterial);

		for (const auto& material: _materials | std::views::values)
		{
			materialUbos.emplace_back(*material);
			materialPbrUbos.emplace_back(*material);
		}

		// Create the material dynamic ubo buffers, one for each frame in flight
		size_t materialUboSize = materialCount * _materialPhongUboAlignment;
		size_t materialPbrUboSize = materialCount * _materialPbrUboAlignment;
		for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
		{
			// TODO fare func interna per non duplicare codice
			// === Bling-Phong ===

			// create material dyn buffer
			auto materialDynUboBuffer = std::make_unique<Buffer>(_device, materialUboSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

			// copy material ubos array to the dynamic buffer
			Utils::uploadToDeviceBuffer(_device, *materialDynUboBuffer, materialUboSize, materialUbos.data());

			// assign the buffer to the frame resource
			_framesData[i]->materialDynUboBuffer = std::move(materialDynUboBuffer);

			// === PBR ===

			// create material dyn buffer
			auto materialPbrDynUboBuffer = std::make_unique<Buffer>(_device, materialPbrUboSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

			// copy material ubos array to the dynamic buffer
			Utils::uploadToDeviceBuffer(_device, *materialPbrDynUboBuffer, materialPbrUboSize, materialPbrUbos.data());

			// assign the buffer to the frame resource
			_framesData[i]->materialPbrDynUboBuffer = std::move(materialPbrDynUboBuffer);
		}

		// allocate one descriptor set for each material
		auto phongDescriptorSets = _descriptorSetManager->allocateDescriptorSets(DescriptorSetLayoutType::MaterialPhong,
			materialCount);

		auto pbrDescriptorSets = _descriptorSetManager->allocateDescriptorSets(DescriptorSetLayoutType::MaterialPbr,
			materialCount);

		// set materials properties and update descriptorSet
		_defaultMaterial->uboIndex = 0;
		_defaultMaterial->baseColorMap = _whiteMapSRGB;
		_defaultMaterial->specularMap = _whiteMapUnorm;

		_defaultMaterial->normalMap = _defaultNormalMap;
		_defaultMaterial->metallicRoughnessMap = _defaultMetallicRoughnessMap;
		_defaultMaterial->emissiveMap = _blackMapSRGB;
		_defaultMaterial->occlusionMap = _whiteMapSRGB;

		_defaultMaterial->descriptorSetPhong = phongDescriptorSets[0];
		_defaultMaterial->descriptorSetPbr = pbrDescriptorSets[0];
		updateMaterialDescriptorSets(*_defaultMaterial);

		uint32_t index = 1; // index 0 is for the default material
		for (auto& material: _materials | std::views::values)
		{
			// set ubo index
			material->uboIndex = index;

			// load texture
			if (!material->baseColorMap)
			{
				if (!material->diffuseTexturePath.empty())
					material->baseColorMap = loadTexture(material->diffuseTexturePath, VK_FORMAT_R8G8B8A8_SRGB);
				else
					material->baseColorMap = _whiteMapSRGB;
			}

			if (!material->specularMap)
			{
				if (!material->specularTexturePath.empty())
					// UNORM format because generally specular map is a grayscale mask/intensity, not a color
						material->specularMap = loadTexture(material->specularTexturePath, VK_FORMAT_R8G8B8A8_UNORM);
				else
					material->specularMap = _whiteMapUnorm;
			}

			if (!material->normalMap)
				material->normalMap = _defaultNormalMap;

			if (!material->metallicRoughnessMap)
				material->metallicRoughnessMap = _defaultMetallicRoughnessMap;

			if (!material->occlusionMap)
				material->occlusionMap = _whiteMapSRGB;

			if (!material->emissiveMap)
				material->emissiveMap = _blackMapSRGB;

			// update the material descriptor set
			material->descriptorSetPhong = phongDescriptorSets[index];
			material->descriptorSetPbr = pbrDescriptorSets[index];
			index++;
			updateMaterialDescriptorSets(*material);
		}
	}

	void Engine::copyDataToImage(const void* data, uint32_t width, uint32_t height, VkDeviceSize imageSize, const Image* image) const
	{
		// Create a staging buffer to upload the texture data to GPU
		Buffer stagingBuffer{_device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT};

		// Copy texture data to the staging buffer
		stagingBuffer.copyDataToBuffer(data);

		// Transition image layout to be optimal for receiving data
		transitionImageLayout(*image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_ASPECT_COLOR_BIT);

		// Copy the texture data from the staging buffer to the image
		copyBufferToImage(stagingBuffer, *image, width, height);

		if (image->getMipLevels() == 1)
			// Transition image layout to be optimal for shader access
			transitionImageLayout(*image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_IMAGE_ASPECT_COLOR_BIT);
		else
			// Generate mipmaps (also transitions the image to be optimal for shader access)
			generateMipmaps(*image);
	}

	void Engine::copyBufferToImage(const Buffer &srcBuffer, const Image& image, uint32_t width, uint32_t height) const
	{
		// Begin one-time command
		VkCommandBuffer commandBuffer = _device.getGraphicsQueue().beginOneTimeCommand();

		auto layerCount = image.getArrayLayers();
		auto layersSize = srcBuffer.getSize() / layerCount;
		std::vector<VkBufferImageCopy> bufferImageCopies {layerCount};
		// init regions to copy
		for (uint32_t i = 0; i < layerCount; i++)
		{
			VkBufferImageCopy region{};
			region.bufferOffset = i * layersSize;
			region.bufferRowLength = 0; // 0 means tightly packed, no padding bytes
			region.bufferImageHeight = 0; // 0 means tightly packed, no padding bytes

			region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
			region.imageSubresource.mipLevel = 0;
			region.imageSubresource.baseArrayLayer = i;
			region.imageSubresource.layerCount = 1; // TODO can't I just set the layer count here without the for loop?

			// which part of the image to copy to
			region.imageOffset = {0, 0, 0};
			region.imageExtent = {width, height, 1};

			bufferImageCopies[i] = region;
		}

		vkCmdCopyBufferToImage(commandBuffer, srcBuffer.getVkBuffer(), image.getVkImage(),
								   VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // which layout the image is currently using
								   bufferImageCopies.size(), bufferImageCopies.data());

		// Execute the copy command
		_device.getGraphicsQueue().endOneTimeCommand(commandBuffer);
	}

	void Engine::createDefaultTextures()
	{
		uint8_t whitePixel[4] = { 255, 255, 255, 255 };
		uint8_t blackPixel[4] = { 0, 0, 0, 255 };
		uint8_t flatNormalPixel[4] = { 128, 128, 255, 255 }; // (0.5, 0.5, 1.0, 1.0). Use 0.5 because normal is converted from [0, 1] to [-1, 1] range in the shader
		uint8_t defaultMetallicRoughnessPixel[4] = { 0, 255, 0, 255 }; // (unused, roughness=1.0, metallic=0.0, alpha=1.0)

		TextureParams params
		{
			.extent = {1, 1},
			.format = VK_FORMAT_R8G8B8A8_SRGB
		};
		_whiteMapSRGB = createTexture(params, &whitePixel);
		_blackMapSRGB = createTexture(params, &blackPixel);

		params.format = VK_FORMAT_R8G8B8A8_UNORM;
		_whiteMapUnorm = createTexture(params, &whitePixel);
		_defaultNormalMap = createTexture(params, &flatNormalPixel);
		_defaultMetallicRoughnessMap = createTexture(params, &defaultMetallicRoughnessPixel);
	}

	std::unique_ptr<Texture> Engine::loadTexture(const std::string& filePath, VkFormat format) const
	{
		// load texture data. Return a pointer to the array of RGBA values
		int texWidth, texHeight, texChannels;
		stbi_uc *pixels = stbi_load(filePath.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);

		if (!pixels)
			throw std::runtime_error("failed to load texture image!");

		// create the texture
		TextureParams params
		{
			.extent = {static_cast<uint32_t>(texWidth), static_cast<uint32_t> (texHeight)},
			.format = format
		};
		auto texture = createTexture(params, pixels);

		// Free texture data
		stbi_image_free(pixels);

		return texture;
	}

	std::unique_ptr<Texture> Engine::createTexture(const TextureParams& params, void* data) const
	{
		auto width = params.extent.width;
		auto height = params.extent.height;
		VkDeviceSize imageSize = width * height * Utils::getBytesPerPixel(params.format);

		// create the texture object
		auto texture = std::make_unique<Texture>(_device, params);

		// copy data to the texture's image
		Image& textImage = texture->getImage();
		copyDataToImage(data, width, height, imageSize, &textImage);

		return texture;
	}

	std::shared_ptr<Image> Engine::createImage(const ImageParams& params, void* data) const
	{
		auto width = params.extent.width;
		auto height = params.extent.height;
		VkDeviceSize imageSize = width * height * 4; // 4 bytes per pixel (RGBA)

		// create the image object
		auto image = std::make_unique<Image>(_device, params);

		// copy data to the image
		copyDataToImage(data, width, height, imageSize, image.get());

		return image;
	}

	void Engine::processInput(float delta)
	{
		if (_config.uiEnabled && ImGui::GetCurrentContext() != nullptr && ImGui::GetIO().WantCaptureKeyboard)
			return;

		int key = _window.getPressedKey();

		if (key == GLFW_KEY_U) setUiEnabled(!_config.uiEnabled);

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

	void Engine::transitionImageLayout(const Image& image, VkImageLayout oldLayout, VkImageLayout newLayout,
		VkImageAspectFlags aspectMask) const
	{
		VkCommandBuffer commandBuffer = _device.getGraphicsQueue().beginOneTimeCommand();

		transitionImageLayout(commandBuffer, image.getVkImage(), image.getMipLevels(), oldLayout, newLayout, aspectMask,
			image.getArrayLayers());

		_device.getGraphicsQueue().endOneTimeCommand(commandBuffer);
	}


	void Engine::transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, uint32_t mipLevels, VkImageLayout currentLayout,
		VkImageLayout newLayout, VkImageAspectFlags aspectMask, uint32_t layerCount)
	{

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

		VkAccessFlags srcAccessMask, dstAccessMask;
		VkPipelineStageFlags srcStageMask, dstStageMask;
		getStageAndAccessMaskForLayout(currentLayout, srcStageMask, srcAccessMask);
		getStageAndAccessMaskForLayout(newLayout, dstStageMask, dstAccessMask);

		VkImageMemoryBarrier2 barrier
		{
			.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
			.srcStageMask = srcStageMask,
			.srcAccessMask = srcAccessMask,
			.dstStageMask = dstStageMask,
			.dstAccessMask = dstAccessMask,
			.oldLayout = currentLayout,
			.newLayout = newLayout,
			.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED, // for queue family ownership transfer, not used here
			.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
			.image = image,
			.subresourceRange = {aspectMask, 0, mipLevels, 0, layerCount},
		};

		VkDependencyInfo depInfo
		{
			.sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
			.imageMemoryBarrierCount = 1,
			.pImageMemoryBarriers = &barrier,
		};

		vkCmdPipelineBarrier2(commandBuffer, &depInfo);
	}

	void Engine::getStageAndAccessMaskForLayout(VkImageLayout layout, VkPipelineStageFlags& stageMask, VkAccessFlags& accessMask)
	{
		switch (layout)
		{
			case VK_IMAGE_LAYOUT_UNDEFINED:
				// We don't care about previous data, so we don't wait for anything.
				stageMask = VK_PIPELINE_STAGE_2_TOP_OF_PIPE_BIT; // earliest possible stage
				accessMask = VK_ACCESS_2_NONE;
				break;
			case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
				stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
				accessMask = VK_ACCESS_2_TRANSFER_READ_BIT;
				break;
			case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
				stageMask = VK_PIPELINE_STAGE_2_TRANSFER_BIT;
				accessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT;
				break;
			case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
				stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT;
				accessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT;
				break;
			case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
				stageMask = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT | // where the GPU checks the depth before running the Fragment Shader
						VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT; // where the GPU writes the final depth value after the Fragment Shader
				accessMask = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
				break;
			case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
				stageMask = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT; // fragment shader reads from texture
				accessMask = VK_ACCESS_2_SHADER_READ_BIT;
				break;
			case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
				stageMask = VK_PIPELINE_STAGE_2_BOTTOM_OF_PIPE_BIT;
				accessMask = VK_ACCESS_2_NONE;
				break;
			default:
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
	}

	void Engine::generateMipmaps(const Image &image) const
	{
		// Use vkCmdBlitImage command. This command performs copying, scaling, and filtering operations.
		// We will call this multiple times to blit data to each mip level of the image.
		// Source and destination of the command will be the same image, but different mip levels.

		// Check if the image format supports linear blitting
		if (!_device.isLinearFilteringSupported(image.getFormat(), VK_IMAGE_TILING_OPTIMAL))
		{
			Log::Get().Warning("Failed to create mip levels. Texture image format does not support linear blitting!");

			transitionImageLayout(image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
				VK_IMAGE_ASPECT_COLOR_BIT);

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
