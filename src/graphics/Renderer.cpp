#include "Renderer.hpp"

namespace m1
{
	void beginRendering(VkCommandBuffer cmdBuffer, VkRect2D renderArea, uint32_t colorAttachmentCount, VkRenderingAttachmentInfo* pColorAttachments,
		VkRenderingAttachmentInfo* pDepthAttachment)
	{
		// begin rendering
		VkRenderingInfo renderingInfo
		{
			.sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
			.renderArea = renderArea,
			.layerCount = 1,
			.colorAttachmentCount = colorAttachmentCount,
			.pColorAttachments = pColorAttachments,
			.pDepthAttachment = pDepthAttachment,
		};
		vkCmdBeginRendering(cmdBuffer, &renderingInfo);
	}

	void endRendering(VkCommandBuffer cmdBuffer)
	{
		vkCmdEndRendering(cmdBuffer);
	}

	void setDynamicStates(VkCommandBuffer cmdBuffer, VkExtent2D extent)
	{
		// set viewport
		VkViewport viewport
		{
			.x        = 0.0f,
			.y        = 0.0f,
			.width    = static_cast<float>(extent.width),
			.height   = static_cast<float>(extent.height),
			.minDepth = 0.0f,
			.maxDepth = 1.0f,
		};
		vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);

		// set scissor
		VkRect2D scissor
		{
			.offset = {0, 0},
			.extent = extent,
		};
		vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
	}

	VkRenderingAttachmentInfo createColorAttachment(VkImageView imageView)
	{
		return
		{
			.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView   = imageView,
			.imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
			.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue  = {{0.0f, 0.0f, 0.0f, 1.0f}},
		};
	}

	VkRenderingAttachmentInfo createDepthAttachment(VkImageView imageView)
	{
		return
		{
			.sType       = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
			.imageView   = imageView,
			.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
			.loadOp      = VK_ATTACHMENT_LOAD_OP_CLEAR,
			.storeOp     = VK_ATTACHMENT_STORE_OP_STORE,
			.clearValue  = {.depthStencil{1.0f, 0}} // depth range [0.0f, 1.0f] with 1.0f being furthest - init depth with furthest value
		};
	}
}
