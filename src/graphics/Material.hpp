#pragma once

#include "Texture.hpp"

#include <string>
#include "glm_config.hpp"

namespace m1
{
    struct Material
    {
		static constexpr float DIELECTRIC_F0 = 0.04f;

	    // Constructor
	    explicit Material(const std::string& name,
		    const glm::vec4& baseColor = glm::vec4(1.0f),
		    const glm::vec3& specularColor = glm::vec3(1.0f),
		    const glm::vec3& ambientColor = glm::vec3(1.0f),
		    float shininess = 32.0f,
		    float opacity = 1.0f,
		    const std::string& diffuseTexturePath = "",
		    const std::string& specularTexturePath = "",
    		const glm::vec3& emissiveFactor = glm::vec3(1.0f)) :
    			name(name),
		    	baseColor(baseColor),
		    	specularColor(specularColor),
		    	ambientColor(ambientColor),
    			emissiveFactor(emissiveFactor),
		    	shininess(shininess),
		    	opacity(opacity),
		    	diffuseTexturePath(diffuseTexturePath),
		    	specularTexturePath(specularTexturePath), metallicFactor(1), roughnessFactor(1) {}

		// Properties
	    std::string name;
	    glm::vec4 baseColor; // used both in phong and PBR
	    glm::vec3 specularColor;
	    glm::vec3 ambientColor;
	    float shininess;
	    float opacity; // TODO: cos'e'? c'e' a in base color
	    std::string diffuseTexturePath;
	    std::string specularTexturePath;

	    // PBR properties
	    float metallicFactor;
	    float roughnessFactor;
	    glm::vec3 emissiveFactor;

	    // graphics resources
	    uint32_t uboIndex = -1;
	    std::shared_ptr<Texture> baseColorMap;
	    std::shared_ptr<Texture> specularMap;
	    std::shared_ptr<Texture> normalMap;
	    std::shared_ptr<Texture> metallicRoughnessMap;
	    std::shared_ptr<Texture> occlusionMap;
	    std::shared_ptr<Texture> emissiveMap;
	    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
	    VkDescriptorSet descriptorSetPbr = VK_NULL_HANDLE;

		void syncPbrFromBlinnPhong()
		{
			const float specularIntensity = glm::clamp((specularColor.r + specularColor.g + specularColor.b) / 3.0f, 0.0f, 1.0f);

			metallicFactor = glm::clamp((specularIntensity - DIELECTRIC_F0) / (1.0f - DIELECTRIC_F0), 0.0f, 1.0f);

			// map Blinn-Phong shininess [1..256] -> roughness [1..0.05] (high shininess => low roughness)
			const float normalizedShininess = glm::clamp((shininess - 1.0f) / 255.0f, 0.0f, 1.0f);
			roughnessFactor = glm::clamp(1.0f - normalizedShininess, 0.05f, 1.0f);
		}

		void syncBlinnPhongFromPbr()
		{
			specularColor = glm::mix(glm::vec3(DIELECTRIC_F0), baseColor.rgb, metallicFactor);

			// inverse mapping roughness [1..0.05] -> shininess [1..256]
			const float clampedRoughness = glm::clamp(roughnessFactor, 0.05f, 1.0f);
			const float normalizedShininess = 1.0f - clampedRoughness;
			shininess = glm::mix(1.0f, 256.0f, normalizedShininess);
		}
    };
}
