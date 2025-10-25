#pragma once

#include <string>
#include "glm_config.hpp"

namespace m1
{
    class Material
    {
    public:
        // Constructor
        Material(
        	const std::string& name,
            const glm::vec3& diffuseColor = glm::vec3(1.0f, 1.0f, 1.0f),
            const glm::vec3& specularColor = glm::vec3(1.0f, 1.0f, 1.0f),
            const glm::vec3& ambientColor = glm::vec3(0.2f, 0.2f, 0.2f),
            float shininess = 32.0f,
            float opacity = 1.0f,
            const std::string& texture = "") :
			name(name),
    		diffuseColor(diffuseColor),
            specularColor(specularColor),
            ambientColor(ambientColor),
            shininess(shininess),
            opacity(opacity),
            texture(texture)
        {
        };

        // Getters
    	const std::string& getName() const { return name; }
        const glm::vec3& getDiffuseColor() const { return diffuseColor; }
        const glm::vec3& getSpecularColor() const { return specularColor; }
        const glm::vec3& getAmbientColor() const { return ambientColor; }
        float getShininess() const { return shininess; }
        float getOpacity() const { return opacity; }
        const std::string& getTexture() const { return texture; }
        uint32_t getUboIndex() const { return uboIndex; }

        // Setters
        void setDiffuseColor(const glm::vec3& color) { diffuseColor = color; }
        void setSpecularColor(const glm::vec3& color) { specularColor = color; }
        void setAmbientColor(const glm::vec3& color) { ambientColor = color; }
        void setShininess(float value) { shininess = value; }
        void setOpacity(float value) { opacity = value; }
        void setTexture(const std::string& tex) { texture = tex; }
    	void setUboIndex(uint32_t index) { uboIndex = index; }

    private:
    	std::string name;
        glm::vec3 diffuseColor;
        glm::vec3 specularColor;
        glm::vec3 ambientColor;
        float shininess;
        float opacity;
        std::string texture;
    	uint32_t uboIndex;
    };
}