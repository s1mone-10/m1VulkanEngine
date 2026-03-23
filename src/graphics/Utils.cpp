#include "Utils.hpp"
#include "Engine.hpp"
#include "Texture.hpp"
#include "Queue.hpp"
#include "Sampler.hpp"

#include <stb_image.h>

#include <fstream>


namespace m1
{
    void Utils::copyBuffer(const Device& device, const Buffer& srcBuffer, const Buffer& dstBuffer, VkDeviceSize size)
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

    void Utils::uploadToDeviceBuffer(const Device& device, const Buffer& dstBuffer, VkDeviceSize size, void* data)
    {
        // Create a staging buffer accessible to CPU to upload the data
        Buffer stagingBuffer{ device, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT};

        // Copy data to the staging buffer
        stagingBuffer.copyDataToBuffer(data);

        // Copy staging buffer to destination buffer
        copyBuffer(device, stagingBuffer, dstBuffer, size);
    }

	std::unique_ptr<Texture> Utils::loadEquirectangularHDRMap(const Engine& engine, const std::string& filePath)
    {
    	int width, height, nrComponents;
    	// TODO loadf -> float
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

	int Utils::getBytesPerPixel(VkFormat format)
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

    	return 0;
    }

	std::vector<char> Utils::readFile(const std::string& filename)
    {
    	// ate: Start reading at the end of the file
    	std::ifstream file(filename, std::ios::ate | std::ios::binary);

    	if (!file.is_open())
    	{
    		Log::Get().Error("failed to open file: " + filename);
    		throw std::runtime_error("failed to open file: " + filename);
    	}

    	size_t fileSize = (size_t)file.tellg();
    	std::vector<char> buffer(fileSize);

    	file.seekg(0);
    	file.read(buffer.data(), fileSize);

    	file.close();

    	return buffer;
    }
}
