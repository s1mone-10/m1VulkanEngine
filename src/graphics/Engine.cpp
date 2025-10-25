#include "Engine.hpp"
#include "log/Log.hpp"
#include "Queue.hpp"
#include "SceneObject.hpp"
#include "Utils.hpp"
#include "geometry/Mesh.hpp"

//libs
#include "glm_config.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// std
#include <stdexcept>
#include <vector>
#include <chrono>

namespace m1
{
    Engine::Engine()
    {
        Log::Get().Info("Engine constructor");

        recreateSwapChain();
		_descriptor = std::make_unique<Descriptor>(_device);
        _pipeline = std::make_unique<Pipeline>(_device, *_swapChain, *_descriptor);
        _drawSceneCmdBuffers = _device.getGraphicsQueue().getPersistentCommandPool().createCommandBuffers(FRAMES_IN_FLIGHT);
		createTextureImage();
        createUniformBuffers();
        initLights();
		_descriptor->updateDescriptorSets(_objectUboBuffers, _frameUboBuffers, *_lightsUboBuffer);

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
            vkDestroyFence(_device.getVkDevice(), _frameFences[i], nullptr);

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
	    _materials.try_emplace(material->getName(), std::move(material));
    }

    void Engine::compile()
    {
	    compileMaterials();
    	compileSceneObjects();
    }

    void Engine::mainLoop()
    {
        auto currentTime = std::chrono::high_resolution_clock::now();

        while (!_window.shouldClose())
        {
            glfwPollEvents();

            // update frame time
            auto newTime = std::chrono::high_resolution_clock::now();
            float frameTime = std::chrono::duration<float, std::chrono::seconds::period>(newTime - currentTime).count();
            currentTime = newTime;

            // process input
            processInput(frameTime);

            drawFrame();
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

		// Update the frame uniform buffer
        updateFrameUbo();

        // wait for the previous frame to finish (with Fence wait on the CPU)
        vkWaitForFences(_device.getVkDevice(), 1, &_frameFences[_currentFrame], VK_TRUE, UINT64_MAX);

		// acquire an image from the swap chain (signal the semaphore when the image is ready)
        uint32_t imageIndex;
        auto result = vkAcquireNextImageKHR(_device.getVkDevice(), _swapChain->getVkSwapChain(), UINT64_MAX, _acquireSemaphore, VK_NULL_HANDLE, &imageIndex);

        // Since I don't know the image index in advance, I use a staging semaphore that is then swapped with the one in the array.
        VkSemaphore temp = _acquireSemaphore;
        _acquireSemaphore = _imageAvailableSems[imageIndex];
        _imageAvailableSems[imageIndex] = temp;

        // recreate the swap chain if needed
		if (result == VK_ERROR_OUT_OF_DATE_KHR) // swap chain is no longer compatible with the surface (e.g. window resized)
        {
            Log::Get().Warning("Swap chain out of date, recreating");
            recreateSwapChain();
            return;
        }
        else if (result != VK_SUCCESS && 
			result != VK_SUBOPTIMAL_KHR) // swap chain no longer matches the surface properties exactly, but can still be used to present to the surface successfully
        {
            Log::Get().Error("failed to acquire swap chain image!");
            throw std::runtime_error("failed to acquire swap chain image!");
        }

		// reset the fence to unsignaled state
        vkResetFences(_device.getVkDevice(), 1, &_frameFences[_currentFrame]);

        // record the command buffer
        vkResetCommandBuffer(_drawSceneCmdBuffers[_currentFrame], 0);
        recordDrawCommands(_drawSceneCmdBuffers[_currentFrame], imageIndex);

        // specify the semaphores and stages to wait on
        // Each entry in the waitStages array corresponds to the semaphore with the same index in waitSemaphores
        VkSemaphore waitSemaphores[] = { _imageAvailableSems[imageIndex] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }; // in which stage(s) of the pipeline to wait

        // specify which semaphores to signal once the command buffer has finished executing
        VkSemaphore submitSignalSemaphores[] = { _drawCmdExecutedSems[imageIndex] };
        
        // submit info
        VkSubmitInfo submitInfo
        {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
            //wait semaphores
			.waitSemaphoreCount = 1,
            .pWaitSemaphores = waitSemaphores,
            .pWaitDstStageMask = waitStages,
			// command buffers
            .commandBufferCount = 1,
            .pCommandBuffers = &_drawSceneCmdBuffers[_currentFrame],
			// signal semaphore
            .signalSemaphoreCount = 1,
            .pSignalSemaphores = submitSignalSemaphores,
        };

		// submit the command buffer (fence will be signaled when the command buffer finishes executing)
        if (vkQueueSubmit(_device.getGraphicsQueue().getVkQueue(), 1, &submitInfo, _frameFences[_currentFrame]) != VK_SUCCESS)
        {
            Log::Get().Error("failed to submit draw command buffer!");
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        // present info
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = submitSignalSemaphores; // wait for the command buffer to finish

        VkSwapchainKHR swapChains[] = { _swapChain->getVkSwapChain() };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        // present the swap chain image
        result = vkQueuePresentKHR(_device.getPresentQueue().getVkQueue(), &presentInfo);

        // recreate the swap chain if needed
        if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || _window.FramebufferResized)
        {
            Log::Get().Warning("Swap chain suboptimal, out of date, or window resized. Recreating.");
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
    	_frameUbos[_currentFrame].view = camera.getViewMatrix();
    	_frameUbos[_currentFrame].proj = camera.getProjectionMatrix();

        _frameUboBuffers[_currentFrame]->copyDataToBuffer(&_frameUbos[_currentFrame]);
    }

	void Engine::updateObjectUbo(const SceneObject& sceneObject)
    {
    	_objectUbos[_currentFrame].model = sceneObject.Transform;
    	_objectUbos[_currentFrame].normalMatrix = glm::transpose(glm::inverse(sceneObject.Transform));

    	_objectUboBuffers[_currentFrame]->copyDataToBuffer(&_objectUbos[_currentFrame]);
    }

    void Engine::createSyncObjects()
    {
        // use separate semaphore per swap chain image (even if the frame count is different)
        // to synchronize between acquiring and presenting images
        size_t imageCount = _swapChain->getImageCount();
        _imageAvailableSems.resize(imageCount);
        _drawCmdExecutedSems.resize(imageCount);

        _frameFences.resize(FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // start in signaled state, to don't block the first frame

        for (size_t i = 0; i < imageCount; i++)
        {
            if (vkCreateSemaphore(_device.getVkDevice(), &semaphoreInfo, nullptr, &_imageAvailableSems[i]) != VK_SUCCESS ||
                vkCreateSemaphore(_device.getVkDevice(), &semaphoreInfo, nullptr, &_drawCmdExecutedSems[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create synchronization semaphores");
            }
        }

        if (vkCreateSemaphore(_device.getVkDevice(), &semaphoreInfo, nullptr, &_acquireSemaphore) != VK_SUCCESS)
            throw std::runtime_error("failed to create synchronization semaphores");

        for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
        {
            if (vkCreateFence(_device.getVkDevice(), &fenceInfo, nullptr, &_frameFences[i]) != VK_SUCCESS)
            {
                throw std::runtime_error("failed to create synchronization fence");
            }
        }
    }

    void Engine::drawObjectsLoop(VkCommandBuffer commandBuffer)
    {
	    for (auto& obj : _sceneObjects)
	    {
	    	//updateObjectUbo(*obj); // TODO: how to update the object ubo instead of using push constants?

	    	// push constants
	    	PushConstantData push
			{
				.model = obj->Transform,
				.normalMatrix = glm::transpose(glm::inverse(obj->Transform))
			};
	    	vkCmdPushConstants(commandBuffer, _pipeline->getLayout(), VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(PushConstantData), &push);

	    	if (obj->MaterialUboIndex >=0 && obj->MaterialUboIndex != _currentMaterialUboIndex)
	    	{
	    		_currentMaterialUboIndex = obj->MaterialUboIndex;
	    		uint32_t dynamicOffset = _currentMaterialUboIndex * _materialUboAlignment;
	    		VkDescriptorSet descriptorSet = _descriptor->getMaterialDescriptorSet(_currentFrame);
	    		vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline->getLayout(), 1, 1, &descriptorSet, 1, &dynamicOffset);

	    	}

		    obj->Mesh->draw(commandBuffer);
	    }
    }

    void Engine::recordDrawCommands(VkCommandBuffer commandBuffer, uint32_t imageIndex)
    {
        // it can be executed on a separate thread

        // begin command buffer recording
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to begin recording command buffer!");
        }

        // begin render pass
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = _swapChain->getRenderPass();
        renderPassInfo.framebuffer = _swapChain->getFrameBuffer(imageIndex);
        renderPassInfo.renderArea.offset = { 0, 0 };
        renderPassInfo.renderArea.extent = _swapChain->getExtent();
        
		std::array<VkClearValue, 2> clearValues{}; // the order of clear values must match the order of attachments in the render pass
        clearValues[0].color = { {0.0f, 0.0f, 0.0f, 1.0f} };
		clearValues[1].depthStencil = { 1.0f, 0 }; // depth range [0.0f, 1.0f] with 1.0f being furthest - init depth with furthest value
        renderPassInfo.clearValueCount = static_cast<uint32_t>(clearValues.size());
        renderPassInfo.pClearValues = clearValues.data();

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        // bind the graphics pipeline
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline->getVkPipeline());

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
        scissor.offset = { 0, 0 };
        scissor.extent = _swapChain->getExtent();
        vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        // bind the descriptor set
        VkDescriptorSet descriptorSet = _descriptor->getDescriptorSet(_currentFrame);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline->getLayout(), 0, 1, &descriptorSet, 0, nullptr);

        drawObjectsLoop(commandBuffer);

        // end render pass
        vkCmdEndRenderPass(commandBuffer);

        // end command buffer recording
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        {
            Log::Get().Error("failed to record command buffer!");
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void Engine::recreateSwapChain()
    {
        // TODO: I should recreate the pipeline if image format or render pass (including subpass layout, attachments, sample count, etc.) changed

        Log::Get().Info("Recreating swap chain");
        while (_window.IsMinimized)
            glfwWaitEvents();

		vkDeviceWaitIdle(_device.getVkDevice());

        bool msaa = true;
		VkSampleCountFlagBits samples;
		samples = msaa ? _device.getMaxMsaaSamples() : VK_SAMPLE_COUNT_1_BIT;

        if (_swapChain == nullptr)
        {
            _swapChain = std::make_unique<SwapChain>(_device, _window, samples);
        }
        else
        {
            std::unique_ptr<SwapChain> oldSwapChain = std::move(_swapChain);
            _swapChain = std::make_unique<SwapChain>(_device, _window, samples, oldSwapChain->getVkSwapChain());

            /*if (!oldSwapChain->compareSwapFormats(*lveSwapChain.getVkSwapChain()))
            {
                throw std::runtime_error("Swap chain image(or depth) format has changed!");
            }*/

			_window.FramebufferResized = false;
        }

        // update camera aspect ratio
		camera.setAspectRatio(_swapChain->getAspectRatio());
    }

    void Engine::createUniformBuffers()
    {
        Log::Get().Info("Creating uniform buffers");

    	// init arrays
    	VkDeviceSize frameUboSize = sizeof(FrameUbo);
        _frameUboBuffers.resize(FRAMES_IN_FLIGHT);
    	_frameUbos.resize(FRAMES_IN_FLIGHT);

    	VkDeviceSize objectUboSize = sizeof(ObjectUbo);
    	_objectUboBuffers.resize(FRAMES_IN_FLIGHT);
    	_objectUbos.resize(FRAMES_IN_FLIGHT);

        for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
        {
            // VK_MEMORY_PROPERTY_HOST_COHERENT_BIT: ensures that writes to the mapped memory by the host are automatically visible to the driver (no need for an explicit flush)
        	// persistent mapping because we need to update it every frame

        	// create frame ubo
            _frameUboBuffers[i] = std::make_unique<Buffer>(_device, frameUboSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
            _frameUboBuffers[i]->mapMemory(); // persistent mapping
        	_frameUbos[i] = {};

        	// create object ubo
        	_objectUboBuffers[i] = std::make_unique<Buffer>(_device, objectUboSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        	_objectUboBuffers[i]->mapMemory(); // persistent mapping
        	_objectUbos[i] = {};
        }
    }

    void Engine::initLights()
    {
        // define lights
        LightsUbo lightsUbo{};

        // Ambient light
        lightsUbo.ambient = glm::vec4(0.2f, 0.2f, 0.2f, 1.0f); // soft gray ambient

        // Directional light (like sunlight)
        lightsUbo.lights[0].position = glm::vec4(-0.5f, -1.0f, -0.3f, 0.0f); // w=0 => dir light
        lightsUbo.lights[0].color = glm::vec4(1.0f, 1.0f, 0.9f, 3.0f);     // warm white, intensity=3.0
        lightsUbo.numLights = 1;

        // Create the lights ubo with device local memory for better performance
        VkDeviceSize lightsUboSize = sizeof(LightsUbo);
        _lightsUboBuffer = std::make_unique<Buffer>(_device, lightsUboSize, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        // upload lights data to buffer
        Utils::uploadToDeviceBuffer(_device, *_lightsUboBuffer, lightsUboSize, &lightsUbo);
    }

    void Engine::compileSceneObjects()
    {
	    for (auto& obj : _sceneObjects)
	    {
	    	auto matName = obj->Mesh->getMaterialName();
	    	if (!matName.empty())
	    	{
	    		auto materialIt = _materials.find(matName);
	    		if (materialIt == _materials.end())
	    			throw std::runtime_error("Material '" + matName + "' not found");

	    		obj->MaterialUboIndex = materialIt->second->getUboIndex();
	    	}

	    	obj->Mesh->compile(_device);
	    }
    }

    void Engine::compileMaterials()
    {
    	if (_materials.empty())
    		return;

	    _materialUboAlignment = _device.getUniformBufferAlignment(sizeof(MaterialUbo));
	    size_t bufferSize = _materials.size() * _materialUboAlignment;

    	for (const auto & kv : _materials)
    	{
    		_materialUbos.emplace_back(*kv.second);
    		kv.second->setUboIndex(_materialUbos.size() - 1);
    	}

	    _materialDynUboBuffers.resize(FRAMES_IN_FLIGHT);
	    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
	    {
		    // create buffer
		    _materialDynUboBuffers[i] = std::make_unique<Buffer>(_device, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

	    	// copy materialUbos data to buffer
	    	_materialDynUboBuffers[i]->mapMemory();
	    	_materialDynUboBuffers[i]->copyDataToBuffer(_materialUbos.data());
	    	_materialDynUboBuffers[i]->unmapMemory();
	    }

    	_descriptor->updateMaterialDescriptorSets(_materialDynUboBuffers, *_texture);
    }

    void Engine::copyBufferToImage(const Buffer& srcBuffer, VkImage image, uint32_t width, uint32_t height)
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
        region.imageOffset = { 0, 0, 0 };
        region.imageExtent = { width, height, 1 };

        vkCmdCopyBufferToImage(commandBuffer, srcBuffer.getVkBuffer(), image,
            VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, // which layout the image is currently using
            1, &region
        );

        // Execute the copy command
        _device.getGraphicsQueue().endOneTimeCommand(commandBuffer);
    }

    void Engine::createTextureImage()
    {
        /*
        - Load image from file
		- Create a staging buffer and copy the image data to it
		- Create the VkImage object
		- Copy data from the staging buffer to the VkImage
        */

        // load texture data. Return a pointer to the array of RGBA values
        int texWidth, texHeight, texChannels;
        stbi_uc* pixels = stbi_load(TEXTURE_PATH.c_str(), &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
		VkDeviceSize imageSize = texWidth * texHeight * 4; // 4 bytes per pixel (RGBA)

        if (!pixels)
        {
            throw std::runtime_error("failed to load texture image!");
        }

		// Create a staging buffer to upload the texture data to GPU
        Buffer stagingBuffer{ _device, imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };

		// Copy texture data to the staging buffer
        stagingBuffer.copyDataToBuffer(pixels);

        // Free texture data
        stbi_image_free(pixels);

        _texture = std::make_unique<Texture>(_device, texWidth, texHeight);

        Image& textImage = _texture->getImage();

		// Transition image layout to be optimal for receiving data
        transitionImageLayout(textImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		// Copy the texture data from the staging buffer to the image
        copyBufferToImage(stagingBuffer, textImage.getVkImage(), static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

        //transitioned to VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL while generating mipmaps
		// Transition image layout to be optimal for shader access
        //transitionImageLayout(textImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

		// Generate mipmaps (also transitions the image to be optimal for shader access)
		generateMipmaps(textImage);
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
            if(camera.getProjectionType() == Camera::ProjectionType::Perspective)
                camera.setProjectionType(Camera::ProjectionType::Orthographic);
            else
				camera.setProjectionType(Camera::ProjectionType::Perspective);
        }
    }

    void Engine::transitionImageLayout(const Image& image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        /*
        In Vulkan, an image layout describes how the GPU should treat the memory of an image(texture, framebuffer, etc.).
        A layout transition is changing an image from one layout to another, so the GPU knows how to access it correctly.
        This is done with a pipeline barrier(vkCmdPipelineBarrier), which synchronizes memory access and updates the image layout.
        */

        VkCommandBuffer commandBuffer = _device.getGraphicsQueue().beginOneTimeCommand();

        // 
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout; // it's ok to use VK_IMAGE_LAYOUT_UNDEFINED if we don't care about the existing image data
        barrier.newLayout = newLayout;
		
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // for queue family ownership transfer, not used here
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        
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
			0,          // dependency flags
			0, nullptr, // memory barriers
            0, nullptr, // buffer memory barries
			1, &barrier // image memory barriers
        );

        _device.getGraphicsQueue().endOneTimeCommand(commandBuffer);
    }

    void Engine::generateMipmaps(const Image& image)
    {
        // Use vkCmdBlitImage command. This command performs copying, scaling, and filtering operations.
        // We will call this multiple times to blit data to each mip level of the image.
		// Source and destination of the command will be the same image, but different mip levels.

        // Check if image format supports linear blitting
        if(!_device.isLinearFilteringSupported(image.getFormat(), VK_IMAGE_TILING_OPTIMAL))
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
            blit.srcOffsets[0] = { 0, 0, 0 };
            blit.srcOffsets[1] = { mipWidth, mipHeight, 1};
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            blit.srcSubresource.mipLevel = i - 1;
            blit.srcSubresource.baseArrayLayer = 0;
            blit.srcSubresource.layerCount = 1;
            blit.dstOffsets[0] = { 0, 0, 0 };
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
