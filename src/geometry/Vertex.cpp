#include "Vertex.hpp"

namespace m1
{
    VkVertexInputBindingDescription Vertex::getBindingDescription()
    {
        // A vertex binding describes at which rate to load data from memory throughout the vertices
        VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0; // the index in the array of bindings
        bindingDescription.stride = sizeof(Vertex); // number of bytes from one entry to the next
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX; // move to the next data entry after each vertex
        
        return bindingDescription;
    }

	std::vector<VkVertexInputAttributeDescription> Vertex::getAttributeDescriptions()
    {
    	std::vector<VkVertexInputAttributeDescription> attributeDescriptions;
    	attributeDescriptions.reserve(4);

    	VkVertexInputAttributeDescription attr{};

        attr.binding = 0;
        attr.location = 0; // input location in vertex shaders
        attr.format = VK_FORMAT_R32G32B32_SFLOAT;
        attr.offset = offsetof(Vertex, pos);
    	attributeDescriptions.push_back(attr);

        attr.binding = 0;
        attr.location = 1;
        attr.format = VK_FORMAT_R32G32B32_SFLOAT;
        attr.offset = offsetof(Vertex, color);
    	attributeDescriptions.push_back(attr);

        attr.binding = 0;
        attr.location = 2;
        attr.format = VK_FORMAT_R32G32B32_SFLOAT;
        attr.offset = offsetof(Vertex, normal);
    	attributeDescriptions.push_back(attr);

        attr.binding = 0;
        attr.location = 3;
        attr.format = VK_FORMAT_R32G32_SFLOAT;
        attr.offset = offsetof(Vertex, texCoord);
    	attributeDescriptions.push_back(attr);

        return attributeDescriptions;
    };
}