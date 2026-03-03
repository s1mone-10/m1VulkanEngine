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
		bool loadMesh(fastgltf::Mesh &gltfMesh);

		bool loadImage(fastgltf::Image &image, Engine &engine);

		bool loadMaterial(fastgltf::Material &material);

	private:
		fastgltf::Asset _asset;

		std::vector<std::unique_ptr<Mesh>> meshes;
		// std::vector<std::shared_ptr<Node>> nodes;
		// std::vector<AllocatedImage> images;
		std::vector<std::shared_ptr<Material>> materials;
		std::vector<std::shared_ptr<Image>> images;
		std::vector<std::shared_ptr<Texture>> textures;
	};
}
