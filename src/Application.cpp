#include "Application.hpp"
#include <stdexcept>
#include <iostream>
#include <vector>

namespace va
{

    App::App()
    {
        recreateSwapChain();
        _pipeline = std::make_unique<Pipeline>(_device, *_swapChain);
        _command = std::make_unique<Command>(_device, FRAMES_IN_FLIGHT);
        createVertexBuffer(mesh.Vertices);
        createIndexxBuffer(mesh.Indices);
        createSyncObjects();
    }

    App::~App()
    {
        // wait for the GPU to finish all operations before destroying the resources
        vkDeviceWaitIdle(_device.getVkDevice());

        for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++)
        {
            vkDestroySemaphore(_device.getVkDevice(), _renderFinishedSemaphores[i], nullptr);
            vkDestroySemaphore(_device.getVkDevice(), _imageAvailableSemaphores[i], nullptr);
            vkDestroyFence(_device.getVkDevice(), _inFlightFences[i], nullptr);
        }

        std::cout << "App destroyed" << std::endl;
    }

    void App::run()
    {
        mainLoop();
    }

    void App::mainLoop()
    {
        while (!_window.shouldClose())
        {
            glfwPollEvents();
            drawFrame();
        }
    }

    void App::drawFrame()
    {
        /*
            At a high level, rendering a frame in Vulkan consists of a common set of steps:

            - Wait for the previous frame to finish
            - Acquire an image from the swap chain
            - Record a command buffer which draws the scene onto that image
            - Submit the recorded command buffer
            - Present the swap chain image
        */

        // wait for the previous frame to finish
        vkWaitForFences(_device.getVkDevice(), 1, &_inFlightFences[_currentFrame], VK_TRUE, UINT64_MAX);

        // acquire an image from the swap chain
        uint32_t imageIndex;
        auto result = vkAcquireNextImageKHR(_device.getVkDevice(), _swapChain->getVkSwapChain(), UINT64_MAX, _imageAvailableSemaphores[_currentFrame], VK_NULL_HANDLE, &imageIndex);

		if (result == VK_ERROR_OUT_OF_DATE_KHR) // swap chain is no longer compatible with the surface (e.g. window resized)
        {
            recreateSwapChain();
            return;
        }
        else if (result != VK_SUCCESS && 
			result != VK_SUBOPTIMAL_KHR) // swap chain no longer matches the surface properties exactly, but can still be used to present to the surface successfully
        {
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
            recreateSwapChain();
        }
        else if (result != VK_SUCCESS)
        {
            throw std::runtime_error("failed to present swap chain image!");
        }

        _currentFrame = (_currentFrame + 1) % FRAMES_IN_FLIGHT;
    }

    void App::createSyncObjects()
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
                throw std::runtime_error("failed to create synchronization objects for a frame!");
            }
        }
    }

    void App::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex)
    {
        // it can be execute on separate thread

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
        VkClearValue clearColor = { {{0.0f, 0.0f, 0.0f, 1.0f}} };
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;
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

        // draw command
        vkCmdDrawIndexed(commandBuffer, static_cast<uint32_t>(mesh.Indices.size()), 1, 0, 0, 0);

        // end render pass
        vkCmdEndRenderPass(commandBuffer);

        // end command buffer recording
        if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
        {
            throw std::runtime_error("failed to record command buffer!");
        }
    }

    void App::recreateSwapChain()
    {
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

    void App::createVertexBuffer(const std::vector<Vertex>& vertices)
    {
        VkDeviceSize size = sizeof(vertices[0]) * vertices.size();

        // Create a staging buffer accessible to CPU to upload the vertex data
        Buffer stagingBuffer{ _device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };
        
        // Copy vertex data to the staging buffer
        stagingBuffer.copyDataToBuffer((void*)vertices.data(), size);

        // Create the actual vertex buffer with device local memory for better performance
        _vertexBuffer = std::make_unique<Buffer>(_device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        copyBuffer(stagingBuffer, *_vertexBuffer, size);
    }

    void App::createIndexxBuffer(const std::vector<uint16_t>& indices)
    {
        VkDeviceSize size = sizeof(indices[0]) * indices.size();

        // Create a staging buffer accessible to CPU to upload the vertex data
        Buffer stagingBuffer{ _device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT };

        // Copy indices data to the staging buffer
        stagingBuffer.copyDataToBuffer((void*)indices.data(), size);

        // Create the actual index buffer with device local memory for better performance
        _indexBuffer = std::make_unique<Buffer>(_device, size, VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        copyBuffer(stagingBuffer, *_indexBuffer, size);
    }

    void App::copyBuffer(const Buffer& srcBuffer, const Buffer& dstBuffer, VkDeviceSize size)
    {
        // Memory transfer operations are executed using command buffers.
        // TODO: create a separate command pool with VK_COMMAND_POOL_CREATE_TRANSIENT_BIT flag to memory allocation optimizations

        // Create the command buffer
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = _command->getVkCommandPool();
        allocInfo.commandBufferCount = 1;
        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(_device.getVkDevice(), &allocInfo, &commandBuffer);

        // Begin recording the command buffer
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        // Copy buffer
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0; // Optional
        copyRegion.dstOffset = 0; // Optional
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer.getVkBuffer(), dstBuffer.getVkBuffer(), 1, &copyRegion);

        // End recording the command buffer
        vkEndCommandBuffer(commandBuffer);

        // Submit the command buffer to the graphics queue
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        vkQueueSubmit(_device.getGraphicsQueue(), 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(_device.getGraphicsQueue()); // TODO: use a fence to schedule multiple transfers simultaneously

        // Free the command buffer
        vkFreeCommandBuffers(_device.getVkDevice(), _command->getVkCommandPool(), 1, &commandBuffer);
    }

} // namespace va
