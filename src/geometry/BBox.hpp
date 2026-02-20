#pragma once

//libs
#include "graphics/glm_config.hpp"

namespace m1
{
    struct BBox
    {
        glm::vec3 min{ std::numeric_limits<float>::max() };
        glm::vec3 max{ std::numeric_limits<float>::lowest() };

        void merge(const glm::vec3& point)
        {
            min = glm::min(min, point);
            max = glm::max(max, point);
        }

        void merge(const BBox& other)
        {
            min = glm::min(min, other.min);
            max = glm::max(max, other.max);
        }

        glm::vec3 getCenter() const { return (min + max) * 0.5f; }
        glm::vec3 getExtent() const { return max - min; }
    	std::array<glm::vec3, 8> getCorners() const
        {
        	return std::array<glm::vec3, 8> {
        		glm::vec3(min.x, min.y, min.z),
				glm::vec3(max.x, min.y, min.z),
				glm::vec3(min.x, max.y, min.z),
				glm::vec3(max.x, max.y, min.z),
				glm::vec3(min.x, min.y, max.z),
				glm::vec3(max.x, min.y, max.z),
				glm::vec3(min.x, max.y, max.z),
				glm::vec3(max.x, max.y, max.z)
         	};
        }
    };
}