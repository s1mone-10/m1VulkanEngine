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

		void setMesh(std::unique_ptr<Mesh> mesh) { Mesh = std::move(mesh); }
		void setMaterial(Material* material) { Material = material; }

		uint64_t Id;
		glm::mat4 Transform{ 1.0f };
		std::unique_ptr<Mesh> Mesh = nullptr;
		Material* Material = nullptr;

	private:
		explicit SceneObject(const uint64_t id) : Id{ id } { Mesh = std::make_unique<m1::Mesh>();}
	};
}