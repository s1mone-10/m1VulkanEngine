#pragma once

#include "Texture.hpp"

#include <string>
#include "glm_config.hpp"

namespace m1
{
    struct Material
    {
	    // Constructor
        Material(
        	const std::string& name,
	        const glm::vec4& diffuseColor = glm::vec4(1.0f),
	        const glm::vec3& specularColor = glm::vec3(.5f, .5f, .5f),
	        const glm::vec3& ambientColor = glm::vec3(.1f, .1f, .1f),
	        float shininess = 32.0f,
	        float opacity = 1.0f,
	        const std::string& diffuseTexturePath = "",
	        const std::string& specularTexturePath = "") : name(name),
	                                                       baseColor(diffuseColor),
	                                                       specularColor(specularColor),
	                                                       ambientColor(ambientColor),
	                                                       shininess(shininess),
	                                                       opacity(opacity),
	                                                       diffuseTexturePath(diffuseTexturePath),
	                                                       specularTexturePath(specularTexturePath) {}

	    std::string name;
	    glm::vec4 baseColor; // used both in phong and PBR
	    glm::vec3 specularColor;
	    glm::vec3 ambientColor;
	    float shininess;
	    float opacity;
	    std::string diffuseTexturePath;
	    std::string specularTexturePath;

	    // PBR properties
	    float metallicFactor;
	    float roughnessFactor;
	    float emissiveFactor;

	    // graphics resources
	    uint32_t uboIndex = -1;
	    std::shared_ptr<Texture> baseColorMap;
	    std::shared_ptr<Texture> specularMap;
	    std::shared_ptr<Texture> metallicRoughnessMap;
	    std::shared_ptr<Texture> normalMap;
	    std::shared_ptr<Texture> occlusionMap;
	    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    };
}
