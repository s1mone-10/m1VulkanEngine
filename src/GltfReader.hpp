#pragma once

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

#include "geometry/Mesh.hpp"
#include "graphics/Material.hpp"

namespace  m1
{
	class Engine;

	class GltfReader
	{
	public:
		bool loadGltf(Engine& engine, const std::filesystem::path &path);

	private:
		fastgltf::Asset _asset;
		std::vector<std::shared_ptr<Mesh>> meshes;
		std::vector<std::unique_ptr<Material>> materials;
		std::vector<std::shared_ptr<Image>> images;
		std::vector<VkSampler> samplers;

		void loadSamplers(Engine& engine);
		bool loadMesh(fastgltf::Mesh& gltfMesh);
		bool loadImage(fastgltf::Image& image, Engine& engine);
		bool loadMaterial(fastgltf::Material& material, Engine& engine);
	};
}
