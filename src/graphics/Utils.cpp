#include "Utils.hpp"
#include "Engine.hpp"
#include "Device.hpp"
#include "Buffer.hpp"
#include "Texture.hpp"
#include "Queue.hpp"
#include "Sampler.hpp"


#include <stb_image.h>

#include <fstream>

namespace m1
{
    void copyBuffer(const Device& device, const Buffer& srcBuffer, const Buffer& dstBuffer, VkDeviceSize size)
    {
        // Memory transfer operations are executed using command buffers.
        
        // Begin one-time command
        VkCommandBuffer commandBuffer = device.getGraphicsQueue().beginOneTimeCommand();

        // Copy buffer
        VkBufferCopy copyRegion{};
        copyRegion.srcOffset = 0; // Optional
        copyRegion.dstOffset = 0; // Optional
        copyRegion.size = size;
        vkCmdCopyBuffer(commandBuffer, srcBuffer.getVkBuffer(), dstBuffer.getVkBuffer(), 1, &copyRegion);

        // Execute the command
        device.getGraphicsQueue().endOneTimeCommand(commandBuffer);
    }

    void uploadToDeviceBuffer(const Device& device, const Buffer& dstBuffer, VkDeviceSize size, const void* data)
    {
        // Create a staging buffer accessible to CPU to upload the data
        Buffer stagingBuffer{ device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT};

        // Copy data to the staging buffer
        stagingBuffer.copyDataToBuffer(data);

        // Copy staging buffer to destination buffer
        copyBuffer(device, stagingBuffer, dstBuffer, size);
    }

	std::unique_ptr<Texture> loadEquirectangularHDRMap(const Engine& engine, const std::string& filePath)
    {
    	int width, height, nrComponents;
    	auto* data = stbi_loadf(filePath.c_str(), &width, &height, &nrComponents, 4);
    	if (data)
    	{
    		auto samplerCreateInfo = Sampler::getDefaultCreateInfo();
    		samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    		samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    		TextureParams params
    		{
    			.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
    			.format = VK_FORMAT_R32G32B32A32_SFLOAT,
    			.samplerCreateInfo = &samplerCreateInfo
    		};

    		auto text = engine.createTexture(params, data);

    		//auto text = std::make_unique<Texture>(device, params);

    		stbi_image_free(data);

    		return text;
    	}

	    Log::Get().Warning("Failed to load HDR image.");
	    return nullptr;
    }

	int getBytesPerPixel(VkFormat format)
    {
    	switch (format)
    	{
    		case VK_FORMAT_R8_SINT:
    		case VK_FORMAT_R8_UNORM:
    			return 1;
    		case VK_FORMAT_R16_SFLOAT:
    			return 2;
    		case VK_FORMAT_R16G16_SFLOAT:
    		case VK_FORMAT_R16G16_SNORM:
    		case VK_FORMAT_B8G8R8A8_UNORM:
    		case VK_FORMAT_R8G8B8A8_UNORM:
    		case VK_FORMAT_R8G8B8A8_SNORM:
    		case VK_FORMAT_R8G8B8A8_SRGB:
    			return 4;
    		case VK_FORMAT_R16G16B16A16_SFLOAT:
    			return 4 * sizeof(uint16_t);
    		case VK_FORMAT_R32G32B32_SFLOAT:
    			return 3 * sizeof(float);
    		case VK_FORMAT_R8G8B8_SRGB:
    			return 3;
    		case VK_FORMAT_R32G32B32A32_SFLOAT:
    			return 4 * sizeof(float);
    		default:
    			printf("Unknown format %d\n", format);
    			exit(1);
    	}
    }

	std::vector<char> readFile(const std::string& filename)
    {
    	// ate: Start reading at the end of the file
    	std::ifstream file(filename, std::ios::ate | std::ios::binary);

    	if (!file.is_open())
    	{
    		Log::Get().Error("failed to open file: " + filename);
    		throw std::runtime_error("failed to open file: " + filename);
    	}

    	size_t fileSize = file.tellg();
    	std::vector<char> buffer(fileSize);

    	file.seekg(0);
    	file.read(buffer.data(), fileSize);

    	file.close();

    	return buffer;
    }

	glm::mat4 perspectiveProjection(float fov, float aspectRatio, float near, float far)
    {
    	auto perspective = glm::perspective(fov, aspectRatio, near, far);

    	// flip the sign of the Y scaling factor because GLM was originally designed for OpenGL, where the Y coordinate of the clip coordinates is inverted
    	perspective[1][1] *= -1;

    	return perspective;
    }

	glm::mat4 orthoProjection(float left, float right, float bottom, float top, float near, float far)
    {
    	auto ortho = glm::ortho(left, right, bottom, top, near, far);

    	// flip the sign of the Y scaling factor because GLM was originally designed for OpenGL, where the Y coordinate of the clip coordinates is inverted
    	ortho[1][1] *= -1;

    	return ortho;
    }

	VkWriteDescriptorSet initVkWriteDescriptorSet(VkDescriptorSet dstSet, uint32_t dstBinding, VkDescriptorType descriptorType,
		VkDescriptorBufferInfo* pBufferInfo, VkDescriptorImageInfo* pImageInfo)
    {
    	return {
    		.sType           = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
			.dstSet          = dstSet,
			.dstBinding      = dstBinding,
			.dstArrayElement = 0,
			.descriptorCount = 1,
			.descriptorType  = descriptorType,
    		.pImageInfo		 = pImageInfo,
    		.pBufferInfo     = pBufferInfo,
		};
    }

	void transitionImageLayout(VkCommandBuffer commandBuffer, VkImage image, uint32_t mipLevels, VkImageLayout currentLayout,
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

	void getStageAndAccessMaskForLayout(VkImageLayout layout, VkPipelineStageFlags& stageMask, VkAccessFlags& accessMask)
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

	uint32_t computeMipLevels(uint32_t width, uint32_t height)
    {
    	return static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1;
    }

	VkImageUsageFlags getTextureImageUsageFlags()
    {
    	// source (for creating mipmaps) and destination for data transfer, sampled for shader read
    	return VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    }
}
