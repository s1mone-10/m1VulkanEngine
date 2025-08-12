#include "Application.hpp"
#include <stdexcept>
#include <iostream>
#include <vector>

namespace va {

Application::Application() {
    _instance = std::make_unique<Instance>(_window);
    _device = std::make_unique<Device>(*_instance, _window);
    createRenderPass();
    _swapChain = std::make_unique<SwapChain>(*_device, _window);
    _pipeline = std::make_unique<Pipeline>(*_device, _renderPass, *_swapChain);
    _command = std::make_unique<Command>(*_device, FRAMES_IN_FLIGHT);
    createFramebuffers();
    createSyncObjects();
}

Application::~Application() {
    // wait for the GPU to finish all operations before destroying the resources
    vkDeviceWaitIdle(_device->get());

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(_device->get(), _renderFinishedSemaphores[i], nullptr);
        vkDestroySemaphore(_device->get(), _imageAvailableSemaphores[i], nullptr);
        vkDestroyFence(_device->get(), _inFlightFences[i], nullptr);
    }

    for (auto framebuffer : _swapChainFramebuffers) {
        vkDestroyFramebuffer(_device->get(), framebuffer, nullptr);
    }

    vkDestroyRenderPass(_device->get(), _renderPass, nullptr);
    std::cout << "Application destroyed" << std::endl;
}

void Application::run() {
    mainLoop();
}

void Application::mainLoop() {
    while (!_window.shouldClose()) {
        glfwPollEvents();
        drawFrame();
    }
}

void Application::drawFrame() {
    /*
        At a high level, rendering a frame in Vulkan consists of a common set of steps:

        - Wait for the previous frame to finish
        - Acquire an image from the swap chain
        - Record a command buffer which draws the scene onto that image
        - Submit the recorded command buffer
        - Present the swap chain image
    */

    // wait for the previous frame to finish
    vkWaitForFences(_device->get(), 1, &_inFlightFences[_currentFrame], VK_TRUE, UINT64_MAX);
    vkResetFences(_device->get(), 1, &_inFlightFences[_currentFrame]);

    // acquire an image from the swap chain
    uint32_t imageIndex;
    vkAcquireNextImageKHR(_device->get(), _swapChain->get(), UINT64_MAX, _imageAvailableSemaphores[_currentFrame], VK_NULL_HANDLE, &imageIndex);

    // record the command buffer
    vkResetCommandBuffer(_command->getCommandBuffers()[_currentFrame], 0);
    recordCommandBuffer(_command->getCommandBuffers()[_currentFrame], imageIndex);

    // submit the command buffer
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    // Each entry in the waitStages array corresponds to the semaphore with the same index in pWaitSemaphores
    VkSemaphore waitSemaphores[] = {_imageAvailableSemaphores[_currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT}; // in which stage(s) of the pipeline to wait
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &_command->getCommandBuffers()[_currentFrame];

    VkSemaphore signalSemaphores[] = {_renderFinishedSemaphores[_currentFrame]};
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = signalSemaphores;

    if (vkQueueSubmit(_device->getGraphicsQueue(), 1, &submitInfo, _inFlightFences[_currentFrame]) != VK_SUCCESS) {
        throw std::runtime_error("failed to submit draw command buffer!");
    }

    // present the swap chain image
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = signalSemaphores;

    VkSwapchainKHR swapChains[] = {_swapChain->get()};
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;

    vkQueuePresentKHR(_device->getPresentQueue(), &presentInfo);

    _currentFrame = (_currentFrame + 1) % FRAMES_IN_FLIGHT;
}

/// <summary>
/// Set information about the framebuffer attachments (images linked to a framebuffer where rendering outputs go)
/// </summary>
void Application::createRenderPass() {
    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = _swapChain->getImageFormat();
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT; // no multisampling
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR; // operation to perform on the attachment before rendering
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; // operation to perform on the attachment after rendering
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // image to be presented in the swap chain

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    // A single render pass can consist of multiple subpasses. For example a sequence of post-processing effects
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    //The index of the attachment in this array is directly referenced from the fragment shader with the layout(location = 0) out vec4 outColor directive!
    subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    // render pass info
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;
    renderPassInfo.pAttachments = &colorAttachment;
    renderPassInfo.subpassCount = 1;
    renderPassInfo.pSubpasses = &subpass;
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    // create the render pass
    if (vkCreateRenderPass(_device->get(), &renderPassInfo, nullptr, &_renderPass) != VK_SUCCESS) {
        throw std::runtime_error("failed to create render pass!");
    }
}

void Application::createFramebuffers() {
    // The attachments specified during render pass creation are bound by wrapping them into a VkFramebuffer object.
    // A framebuffer object references all of the VkImageView objects that represent the attachments
    _swapChainFramebuffers.resize(_swapChain->getImageCount());
    for (size_t i = 0; i < _swapChain->getImageCount(); i++) {
        VkImageView attachments[] = {_swapChain->getImageViews()[i]};

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = _renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments; // objects that should be bound to the respective renderPass pAttachment array
        framebufferInfo.width = _swapChain->getExtent().width;
        framebufferInfo.height = _swapChain->getExtent().height;
        framebufferInfo.layers = 1; // as in the swap chain

        if (vkCreateFramebuffer(_device->get(), &framebufferInfo, nullptr, &_swapChainFramebuffers[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create framebuffer!");
        }
    }
}

void Application::createSyncObjects() {
    _imageAvailableSemaphores.resize(FRAMES_IN_FLIGHT);
    _renderFinishedSemaphores.resize(FRAMES_IN_FLIGHT);
    _inFlightFences.resize(FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT; // start in signaled state, to don't block the first frame

    for (size_t i = 0; i < FRAMES_IN_FLIGHT; i++) {
        if (vkCreateSemaphore(_device->get(), &semaphoreInfo, nullptr, &_imageAvailableSemaphores[i]) != VK_SUCCESS ||
            vkCreateSemaphore(_device->get(), &semaphoreInfo, nullptr, &_renderFinishedSemaphores[i]) != VK_SUCCESS ||
            vkCreateFence(_device->get(), &fenceInfo, nullptr, &_inFlightFences[i]) != VK_SUCCESS) {
            throw std::runtime_error("failed to create synchronization objects for a frame!");
        }
    }
}

void Application::recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
    // it can be execute on separate thread

    // begin command buffer recording
    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = 0; // Optional
    beginInfo.pInheritanceInfo = nullptr; // Optional

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS) {
        throw std::runtime_error("failed to begin recording command buffer!");
    }

    // begin render pass
    VkRenderPassBeginInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassInfo.renderPass = _renderPass;
    renderPassInfo.framebuffer = _swapChainFramebuffers[imageIndex];
    renderPassInfo.renderArea.offset = {0, 0};
    renderPassInfo.renderArea.extent = _swapChain->getExtent();
    VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 1.0f}}};
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearColor;
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    // bind the graphics pipeline
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, _pipeline->get());

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

    // draw command
    /*
        vertexCount: Even though we don't have a vertex buffer, we technically still have 3 vertices to draw.
        instanceCount: Used for instanced rendering, use 1 if you're not doing that.
        firstVertex: Used as an offset into the vertex buffer, defines the lowest value of gl_VertexIndex.
        firstInstance: Used as an offset for instanced rendering, defines the lowest value of gl_InstanceIndex.
    */
    vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    // end render pass
    vkCmdEndRenderPass(commandBuffer);

    // end command buffer recording
    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to record command buffer!");
    }
}

} // namespace va
