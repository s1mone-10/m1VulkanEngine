#pragma once

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

#include "geometry/Mesh.hpp"
#include "graphics/Material.hpp"

namespace  m1
{
	class Engine;
	class Sampler;
	class Image;

	class GltfReader
	{
	public:
		bool loadGltf(Engine& engine, const std::filesystem::path &path);

	private:
		fastgltf::Asset _asset;
		std::vector<std::unique_ptr<Mesh>> meshes;
		std::vector<std::unique_ptr<Material>> materials;
		std::vector<std::shared_ptr<Image>> images;
		std::vector<std::shared_ptr<Texture>> textures;
		std::vector<std::shared_ptr<Sampler>> samplers;

		void loadSamplers(Engine& engine);
		bool loadMesh(const fastgltf::Mesh& gltfMesh);
		std::shared_ptr<Image> loadImage(fastgltf::Image& image, Engine& engine, VkFormat format);
		std::shared_ptr<Texture> loadTexture(Engine& engine, const fastgltf::TextureInfo& textureIndex, VkFormat format);
		bool loadMaterial(fastgltf::Material& gltfMaterial, Engine& engine);
	};
}
