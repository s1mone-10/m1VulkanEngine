#pragma once
#include <vector>

#include <vulkan/vulkan.h>

namespace m1
{
	void beginRendering(VkCommandBuffer cmdBuffer, VkRect2D renderArea, uint32_t colorAttachmentCount,
		VkRenderingAttachmentInfo* pColorAttachments, VkRenderingAttachmentInfo* pDepthAttachment);
	void endRendering(VkCommandBuffer cmdBuffer);
	void setDynamicStates(VkCommandBuffer cmdBuffer, VkExtent2D extent);
	VkRenderingAttachmentInfo createColorAttachment(VkImageView imageView);
	VkRenderingAttachmentInfo createDepthAttachment(VkImageView imageView);
}
