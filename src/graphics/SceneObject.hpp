#pragma once

// libs
#include "glm_config.hpp"

// std
#include <memory>
#include <unordered_map>

namespace m1
{
	class Material; // forward declaration
	class Mesh;

	class SceneObject
	{
	public:
		static std::unique_ptr<SceneObject> createSceneObject()
		{
			static uint64_t currentId = 0;
			return std::unique_ptr<SceneObject>(new SceneObject(currentId++));
		}

		uint64_t Id;
		glm::mat4 Transform{ 1.0f };
		Mesh Mesh{};
		Material* Material = nullptr;

	private:
		SceneObject(uint64_t id) : Id{ id } {}
	};
}