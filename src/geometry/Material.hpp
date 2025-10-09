#pragma once

#include <string>
#include <array>

namespace m1
{
    class Material
    {
    public:
        // Constructor
        Material(
            const std::array<float, 3>& diffuseColor = { 1.0f, 1.0f, 1.0f },
            const std::array<float, 3>& specularColor = { 1.0f, 1.0f, 1.0f },
            const std::array<float, 3>& ambientColor = { 0.2f, 0.2f, 0.2f },
            float shininess = 32.0f,
            float opacity = 1.0f,
            const std::string& texture = "") : diffuseColor(diffuseColor),
            specularColor(specularColor),
            ambientColor(ambientColor),
            shininess(shininess),
            opacity(opacity),
            texture(texture)
        {
        };

        // Getters
        inline const std::array<float, 3>& getDiffuseColor() const { return diffuseColor; }
        inline const std::array<float, 3>& getSpecularColor() const { return specularColor; }
        inline const std::array<float, 3>& getAmbientColor() const { return ambientColor; }
        inline float getShininess() const { return shininess; }
        inline float getOpacity() const { return opacity; }
        inline const std::string& getTexture() const { return texture; }

        // Setters
        inline void setDiffuseColor(const std::array<float, 3>& color) { diffuseColor = color; }
        inline void setSpecularColor(const std::array<float, 3>& color) { specularColor = color; }
        inline void setAmbientColor(const std::array<float, 3>& color) { ambientColor = color; }
        inline void setShininess(float value) { shininess = value; }
        inline void setOpacity(float value) { opacity = value; }
        inline void setTexture(const std::string& tex) { texture = tex; }

    private:
        std::array<float, 3> diffuseColor;
        std::array<float, 3> specularColor;
        std::array<float, 3> ambientColor;
        float shininess;
        float opacity;
        std::string texture;
    };
}