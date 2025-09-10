#include "Engine.hpp"
#include "log/Log.hpp"
#include <stdexcept>
#include <iostream>
#include <vector>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include <chrono>

namespace m1
{

    Engine::Engine()
    {
        Log::Get().Info("App constructor");
        recreateSwapChain();
		_descriptor = std::make_unique<Descritor>(_device);
        _pipeline = std::make_unique<Pipeline>(_device, *_swapChain, _descriptor->getDescriptorSetLayout());
        _command = std::make_unique<Command>(_device, FRAMES_IN_FLIGHT);
		createTextureImage();
        createVertexBuffer(mesh.Vertices);
        createIndexBuffer(mesh.Indices);
        createUniformBuffers();
		_descriptor->updateDescriotorSets(_uniformBuffers, *_texture);
        createSyncObjects();
    }

    Engine::~Engine()
    {
        // wait for the GPU to finish all operations before destroying the resources
        vkDeviceWaitIdle(_device.getVkDevice());

        for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(_device.getVkDevice(), _renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(_device.getVkDevice(), _imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(_device.getVkDevice(), _inFlightFences[i], nullptr);
        }
        Log::Get().Info("Engine destroyed");
    }

    void Engine::run()
    {
        mainLoop();
    }

    void Engine::mainLoop()
    {
        while (!_window.shouldClose())
        {
            glfwPollEvents();
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
            - Submit the recorded command buffer
            - Present the swap chain image
        */

		// Update the uniform buffer
        updateUniformBuffer(_currentFrame);

        // wait for the previous frame to finish
        vkWaitForFences(_device.getVkDevice(), 1, &_inFlightFences[_currentFrame], VK_TRUE, UINT64_MAX);

        // acquire an image from the swap chain
        uint32_t imageIndex;
        auto result = vkAcquireNextImageKHR(_device.getVkDevice(), _swapChain->getVkSwapChain(), UINT64_MAX, _imageAvailableSemaphores[_currentFrame], VK_NULL_HANDLE, &imageIndex);

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
        vkResetFences(_device.getVkDevice(), 1, &_inFlightFences[_currentFrame]);

        // record the command buffer
        vkResetCommandBuffer(_command->getCommandBuffers()[_currentFrame], 0);
        recordCommandBuffer(_command->getCommandBuffers()[_currentFrame], imageIndex);

        // submit the command buffer
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        // Each entry in the waitStages array corresponds to the semaphore with the same index in pWaitSemaphores
        VkSemaphore waitSemaphores[] = { _imageAvailableSemaphores[_currentFrame] };
        VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT }; // in which stage(s) of the pipeline to wait
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &_command->getCommandBuffers()[_currentFrame];

        VkSemaphore signalSemaphores[] = { _renderFinishedSemaphores[_currentFrame] };
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        if (vkQueueSubmit(_device.getGraphicsQueue(), 1, &submitInfo, _inFlightFences[_currentFrame]) != VK_SUCCESS)
        {
            Log::Get().Error("failed to submit draw command buffer!");
            throw std::runtime_error("failed to submit draw command buffer!");
        }

        // present the swap chain image
        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;

        VkSwapchainKHR swapChains[] = { _swapChain->getVkSwapChain() };
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapChains;
        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(_device.getPresentQueue(), &presentInfo);

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

        _currentFrame = (_currentFrame + 1) % FRAMES_IN_FLIGHT;
    }

    void Engine::updateUniformBuffer(uint32_t currentImage)
    {
        static auto startTime = std::chrono::high_resolution_clock::now();

        auto currentTime = std::chrono::high_resolution_clock::now();
        float time = std::chrono::duration<float, std::chrono::seconds::period>(currentTime - startTime).count();

        UniformBufferObject ubo{};
        ubo.model = glm::rotate(glm::mat4(1.0f), time * glm::radians(90.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.view = glm::lookAt(glm::vec3(2.0f, 2.0f, 2.0f), glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));
        ubo.proj = glm::perspective(glm::radians(45.0f), _swapChain->getExtent().width / (float)_swapChain->getExtent().height, 0.1f, 10.0f);

        // flip the T because GLM was originally designed for OpenGL, where the Y coordinate of the clip coordinates is inverted
        ubo.proj[1][1] *= -1;

        _uniformBuffers[currentImage]->copyDataToBuffer(&ubo);
    }

    void Engine::createSyncObjects()
    {
        _imageAvailableSemaphores.resize(FRAMES_IN_FLIGHT);
        _renderFinishedSemaphores.resize(FRAMES_IN_FLIGHT);
        _inFlightFences.resize(FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // start in signaled state, to don't block the first frame

        for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
        {
            if (vkCreateSemaphore(_device.getVkDevice(), &semaphoreInfo, nullptr, &_imageAvailableSemaphores[i]) != VK_SUCCESS ||
                vkCreateSemaphore(_device.getVkDevice(), &semaphoreInfo, nullptr, &_renderFinishedSemaphores[i]) != VK_SUCCESS ||
                vkCreateFence(_device.getVkDevice(), &fenceInfo, nullptr, &_inFlightFences[i]) != VK_SUCCESS)
            {
                Log::Get().Error("failed to create synchronization objects for a frame!");
                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
        Log::Get().Info("Created synchronization objects");
    }

    void Engine::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
    {
        // it can be execute on separate thread

        // begin command buffer recording
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = nullptr; // Optional

        if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
        {
            Log::Get().Error("failed to begin recording command buffer!");
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

        // bind the vertex buffer
        VkBuffer vertexBuffers[] = { _vertexBuffer->getVkBuffer() };
        VkDeviceSize offsets[] = { 0 };
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

        // bind the index buffer
        vkCmdBindIndexBuffer(commandBuffer, _indexBuffer->getVkBuffer(), 0, VK_INDEX_TYPE_UINT16);

        // bind the descriptor set
        VkDescriptorSet descriptorSet = _descriptor->getDescriptorSet(_currentFrame);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline->getLayout(), 0, 1, &descriptorSet, 0, nullptr);
        // draw command
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(mesh.Indices.size()), 1, 0, 0, 0);

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
        Log::Get().Info("Recreating swap chain");
        while (_window.IsMinimized)
            glfwWaitEvents();

		vkDeviceWaitIdle(_device.getVkDevice());

        if (_swapChain == nullptr)
        {
            _swapChain = std::make_unique<SwapChain>(_device, _window);
        }
        else
        {
            std::unique_ptr<SwapChain> oldSwapChain = std::move(_swapChain);
            _swapChain = std::make_unique<SwapChain>(_device, _window, oldSwapChain->getVkSwapChain());

            /*if (!oldSwapChain->compareSwapFormats(*lveSwapChain.getVkSwapChain()))
            {
                throw std::runtime_error("Swap chain image(or depth) format has changed!");
            }*/

			_window.FramebufferResized = false;
        }
    }

    void Engine::createVertexBuffer(const std::vector<Vertex>& vertices)
    {
        Log::Get().Info("Creating vertex buffer");
        VkDeviceSize size = sizeof(vertices[0]) * vertices.size();

        // Create a staging buffer accessible to CPU to upload the vertex data
        Buffer stagingBuffer{ _device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
        
        // Copy vertex data to the staging buffer
        stagingBuffer.copyDataToBuffer((void*)vertices.data());

        // Create the actual vertex buffer with device local memory for better performance
        _vertexBuffer = std::make_unique<Buffer>(_device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        copyBuffer(stagingBuffer, *_vertexBuffer, size);
    }

    void Engine::createIndexBuffer(const std::vector<uint16_t>& indices)
    {
        Log::Get().Info("Creating index buffer");
        VkDeviceSize size = sizeof(indices[0]) * indices.size();

        // Create a staging buffer accessible to CPU to upload the vertex data
        Buffer stagingBuffer{ _device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };

        // Copy indices data to the staging buffer
        stagingBuffer.copyDataToBuffer((void*)indices.data());

        // Create the actual index buffer with device local memory for better performance
        _indexBuffer = std::make_unique<Buffer>(_device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        copyBuffer(stagingBuffer, *_indexBuffer, size);
    }

    void Engine::createUniformBuffers()
    {
        Log::Get().Info("Creating uniform buffers");
        VkDeviceSize bufferSize = sizeof(UniformBufferObject);

        _uniformBuffers.resize(FRAMES_IN_FLIGHT);       

        for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
        {
            // Use std::unique_ptr to avoid move/copy assignment of Buffer
            _uniformBuffers[i] = std::make_unique<Buffer>(_device, bufferSize, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

            // persistent mapping because we need to update it every frame
            _uniformBuffers[i]->mapMemory();
        }
    }

    void Engine::copyBuffer(const Buffer& srcBuffer, const Buffer& dstBuffer, VkDeviceSize size)
    {
        // Memory transfer operations are executed using command buffers.
        // TODO: create a separate command pool with VK_COMMAND_POOL_CREATE_TRANSIENT_BIT flag to memory allocation optimizations

		// Begin one-time command
        VkCommandBuffer commandBuffer = _command->beginOneTimeCommand();

        // Copy buffer
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0; // Optional
        copyRegion.dstOffset = 0; // Optional
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer.getVkBuffer(), dstBuffer.getVkBuffer(), 1, &copyRegion);

		// Execute the command
		_command->endOneTimeCommand(commandBuffer);
    }

    void Engine::copyBufferToImage(const Buffer& srcBuffer, VkImage image, uint32_t width, uint32_t height)
    {
        // Begin one-time command
        VkCommandBuffer commandBuffer = _command->beginOneTimeCommand();

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
        _command->endOneTimeCommand(commandBuffer);
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
        stbi_uc* pixels = stbi_load("..\\..\\..\\Textures\\statue.jpg", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
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

		// Transition image layout to be optimal for receiving data
        transitionImageLayout(_texture->getImage(), VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

		// Copy the texture data from the staging buffer to the image
        copyBufferToImage(stagingBuffer, _texture->getImage(), static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));

		// Transition image layout to be optimal for shader access
        transitionImageLayout(_texture->getImage(), VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }

    void Engine::transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
    {
        /*
        In Vulkan, an image layout describes how the GPU should treat the memory of an image(texture, framebuffer, etc.).
        A layout transition is changing an image from one layout to another, so the GPU knows how to access it correctly.
        This is done with a pipeline barrier(vkCmdPipelineBarrier), which synchronizes memory access and updates the image layout.
        */

        VkCommandBuffer commandBuffer = _command->beginOneTimeCommand();

        // 
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
		barrier.oldLayout = oldLayout; // it's ok to use VK_IMAGE_LAYOUT_UNDEFINED if we don't care about the existing image data
        barrier.newLayout = newLayout;
		
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED; // for queue family ownership transfer, not used here
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        
        barrier.image = image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
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

        _command->endOneTimeCommand(commandBuffer);
    }

} // namespace m1
