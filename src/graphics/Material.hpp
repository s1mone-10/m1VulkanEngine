#pragma once

#include "Texture.hpp"

#include <string>
#include "glm_config.hpp"
#include "Pipeline.hpp"

namespace m1
{
    struct Material
    {
    private:
		static constexpr float DIELECTRIC_F0 = 0.04f;
    	inline static uint32_t nameSuffix;

    public:
	    // Blinn-Phong properties constructor
	    explicit Material(const std::string& name,
			float shininess,
		    const glm::vec4& baseColor = glm::vec4(1.0f),
		    const glm::vec3& specularColor = glm::vec3(1.0f),
		    const glm::vec3& ambientColor = glm::vec3(1.0f),
		    const std::string& diffuseTexturePath = "",
		    const std::string& specularTexturePath = "") :
    			name(name),
		    	baseColor(baseColor),
		    	specularColor(specularColor),
		    	ambientColor(ambientColor),
		    	shininess(shininess),
		    	diffuseTexturePath(diffuseTexturePath),
		    	specularTexturePath(specularTexturePath), metallicFactor(1), roughnessFactor(1)
	    {
	    	if (name.empty())
	    		this->name = "Material_" + std::to_string(nameSuffix++);

	    	// extract pbr properties (code from AI not verified)
	    	const float specularIntensity = glm::clamp((specularColor.r + specularColor.g + specularColor.b) / 3.0f, 0.0f, 1.0f);

	    	metallicFactor = glm::clamp((specularIntensity - DIELECTRIC_F0) / (1.0f - DIELECTRIC_F0), 0.0f, 1.0f);

	    	// map Blinn-Phong shininess [1..256] -> roughness [1..0.05] (high shininess => low roughness)
	    	const float normalizedShininess = glm::clamp((shininess - 1.0f) / 255.0f, 0.0f, 1.0f);
	    	roughnessFactor = glm::clamp(1.0f - normalizedShininess, 0.05f, 1.0f);
	    }

    	// Physical-based rendering properties constructor
    	explicit Material(const std::string& name,
			const glm::vec4& baseColor = glm::vec4(1.0f),
			float metallicFactor = 1,
			float roughnessFactor = 1,
			const glm::vec3& emissiveFactor = glm::vec3(1.0f)
			) :
				name(name),
				baseColor(baseColor),
    			metallicFactor(metallicFactor),
				roughnessFactor(roughnessFactor),
    			emissiveFactor(emissiveFactor)
	    {
	    	if (name.empty())
	    		this->name = "Material_" + std::to_string(nameSuffix++);

	    	// extract blinn-phong properties (code from AI not verified)
	    	specularColor = glm::mix(glm::vec3(DIELECTRIC_F0), glm::vec3(baseColor), metallicFactor);

	    	ambientColor = glm::vec3(baseColor) * 0.1f; // simple ambient approximation

	    	// inverse mapping roughness [1..0.05] -> shininess [1..256]
	    	const float clampedRoughness = glm::clamp(roughnessFactor, 0.05f, 1.0f);
	    	const float normalizedShininess = 1.0f - clampedRoughness;
	    	shininess = glm::mix(1.0f, 256.0f, normalizedShininess);
	    }

		VkDescriptorSet getDescriptorSet(PipelineType pipeLineType) const
		{
			return pipeLineType == PipelineType::PbrLighting ? descriptorSetPbr : descriptorSet;
		}

		// Properties
	    std::string name;
	    glm::vec4 baseColor; // used both in phong and PBR

    	// Blinn-phong properties
	    glm::vec3 specularColor;
	    glm::vec3 ambientColor;
	    float shininess;
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
    };
}
