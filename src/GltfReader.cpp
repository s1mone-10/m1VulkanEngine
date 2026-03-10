#include <chrono>
#include <iostream>

#include <GltfReader.hpp>
#include <stb_image.h>

#include "graphics/Engine.hpp"
#include "graphics/Sampler.hpp"
#include "graphics/SceneObject.hpp"
#include "graphics/Material.hpp"
#include "graphics/Texture.hpp"
#include "graphics/Image.hpp"

namespace m1
{
	bool GltfReader::loadGltf(Engine &engine, const std::filesystem::path &path)
	{
		if (!std::filesystem::exists(path))
		{
			std::cout << "Failed to find " << path << '\n';
			return false;
		}

		if constexpr (std::is_same_v<std::filesystem::path::value_type, wchar_t>)
		{
			std::wcout << "Loading " << path << '\n';
		}
		else
		{
			std::cout << "Loading " << path << '\n';
		}

		// Parse the glTF file and get the constructed asset
		{
			static constexpr auto supportedExtensions =
					fastgltf::Extensions::KHR_mesh_quantization |
					fastgltf::Extensions::KHR_texture_transform |
					fastgltf::Extensions::KHR_materials_variants;

			fastgltf::Parser parser(supportedExtensions);

			constexpr auto gltfOptions =
					fastgltf::Options::DontRequireValidAssetMember |
					fastgltf::Options::AllowDouble |
					fastgltf::Options::LoadExternalBuffers |
					fastgltf::Options::LoadExternalImages |
					fastgltf::Options::GenerateMeshIndices;

			auto gltfFile = fastgltf::MappedGltfFile::FromPath(path);
			if (!static_cast<bool>(gltfFile))
			{
				std::cerr << "Failed to open glTF file: " << fastgltf::getErrorMessage(gltfFile.error()) << '\n';
				return false;
			}

			auto asset = parser.loadGltf(gltfFile.get(), path.parent_path(), gltfOptions);
			if (asset.error() != fastgltf::Error::None)
			{
				std::cerr << "Failed to load glTF: " << fastgltf::getErrorMessage(asset.error()) << '\n';
				return false;
			}

			_asset = std::move(asset.get());

			// load samplers
			loadSamplers(engine);

			// load images
			for (auto &image: _asset.images)
				loadImage(image, engine);

			// load materials
			for (auto &material: _asset.materials)
				loadMaterial(material, engine);

			// TODO node and node transformation??
			for (auto &mesh: _asset.meshes)
				loadMesh(mesh);

			// add meshes and material to the engine
			for (auto& mesh: meshes)
			{
				auto sceneObj = SceneObject::createSceneObject();
				sceneObj->setMesh(std::move(mesh));
				engine.addSceneObject(std::move(sceneObj));
			}

			for (auto& pippo: materials)
			{
				engine.addMaterial(std::move(pippo));
			}
		}

		return true;
	}

	void GltfReader::loadSamplers(Engine& engine)
	{
		auto extract_filter = [&](fastgltf::Filter filter)
		{
			switch (filter)
			{
				// nearest samplers
				case fastgltf::Filter::Nearest:
				case fastgltf::Filter::NearestMipMapNearest:
				case fastgltf::Filter::NearestMipMapLinear:
					return VK_FILTER_NEAREST;

				// linear samplers
				case fastgltf::Filter::Linear:
				case fastgltf::Filter::LinearMipMapNearest:
				case fastgltf::Filter::LinearMipMapLinear:
				default:
					return VK_FILTER_LINEAR;
			}
		};

		auto extract_mipmap_mode = [&](fastgltf::Filter filter)
		{
			switch (filter)
			{
				case fastgltf::Filter::NearestMipMapNearest:
				case fastgltf::Filter::LinearMipMapNearest:
					return VK_SAMPLER_MIPMAP_MODE_NEAREST;

				case fastgltf::Filter::NearestMipMapLinear:
				case fastgltf::Filter::LinearMipMapLinear:
				default:
					return VK_SAMPLER_MIPMAP_MODE_LINEAR;
			}
		};

		for (fastgltf::Sampler& sampler: _asset.samplers)
		{
			VkSamplerCreateInfo samplerCreateInfo = {.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO, .pNext = nullptr};
			samplerCreateInfo.maxLod = VK_LOD_CLAMP_NONE;
			samplerCreateInfo.minLod = 0;

			samplerCreateInfo.magFilter = extract_filter(sampler.magFilter.value_or(fastgltf::Filter::Nearest));
			samplerCreateInfo.minFilter = extract_filter(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

			samplerCreateInfo.mipmapMode = extract_mipmap_mode(sampler.minFilter.value_or(fastgltf::Filter::Nearest));

			samplers.push_back(std::make_shared<Sampler>(engine.getDevice(), &samplerCreateInfo));
		}
	}

	bool GltfReader::loadMesh(const fastgltf::Mesh& gltfMesh)
	{
		for (const auto& primitive: gltfMesh.primitives)
		{
			if (primitive.type != fastgltf::PrimitiveType::Triangles)
				continue;

			auto mesh = std::make_unique<Mesh>();

			// Position
			auto position = primitive.findAttribute("POSITION");
			assert(position != primitive.attributes.end());
			// A mesh primitive is required to hold the POSITION attribute.
			assert(primitive.indicesAccessor.has_value()); // we should always have indices

			auto& positionAccessor = _asset.accessors[position->accessorIndex];
			if (!positionAccessor.bufferViewIndex.has_value())
				continue;
			std::vector<Vertex> vertices(positionAccessor.count);

			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(_asset, positionAccessor,
			                                                          [&](fastgltf::math::fvec3 pos, std::size_t idx)
			                                                          {
				                                                          vertices[idx].pos = glm::vec3(
					                                                          pos.x(), pos.y(), pos.z());
			                                                          });

			// Material
			std::size_t baseColorTexcoordIndex = 0;
			if (primitive.materialIndex.has_value())
			{
				auto matIndex = primitive.materialIndex.value();
				mesh->setMaterialName(materials[matIndex]->name);
				auto& gltfMaterial = _asset.materials[matIndex];

				auto& baseColorTexture = gltfMaterial.pbrData.baseColorTexture;
				if (baseColorTexture.has_value())
				{
					auto& texture = _asset.textures[baseColorTexture->textureIndex];
					if (!texture.imageIndex.has_value())
						return false;

					if (baseColorTexture->transform && baseColorTexture->transform->texCoordIndex.has_value())
						baseColorTexcoordIndex = baseColorTexture->transform->texCoordIndex.value();
					else
						baseColorTexcoordIndex = gltfMaterial.pbrData.baseColorTexture->texCoordIndex;
				}
			}

			// TexCoord
			auto texcoordAttribute = std::string("TEXCOORD_") + std::to_string(baseColorTexcoordIndex);
			auto texCoord = primitive.findAttribute(texcoordAttribute);
			if (texCoord != primitive.attributes.end())
			{
				auto& texCoordAccessor = _asset.accessors[texCoord->accessorIndex];

				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(_asset, texCoordAccessor,
					[&](fastgltf::math::fvec2 uv, std::size_t idx)
					{
						vertices[idx].texCoord = glm::vec2(
							uv.x(), uv.y());
					});

			}

			// Normal
			auto normal = primitive.findAttribute("NORMAL");
			if (normal != primitive.attributes.end())
			{
				const auto& normalAccessor = _asset.accessors[normal->accessorIndex];
				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(_asset, normalAccessor,
					[&](fastgltf::math::fvec3 nor,
				std::size_t idx)
					{
						vertices[idx].normal = glm::vec3(
							nor.x(), nor.y(), nor.z());
					});
			}

			// vertex colors
			auto color = primitive.findAttribute("COLOR_0");
			if (color != primitive.attributes.end())
			{
				const auto& colorAccessor = _asset.accessors[color->accessorIndex];
				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(_asset, colorAccessor,
				                                                          [&](fastgltf::math::fvec4 col,
				                                                              std::size_t idx)
				                                                          {
					                                                          vertices[idx].color = glm::vec3(
						                                                          col.x(), col.y(), col.z());
				                                                          });
			}

			// Indices
			auto& indexAccessor = _asset.accessors[primitive.indicesAccessor.value()];
			if (!indexAccessor.bufferViewIndex.has_value())
				return false;

			std::vector<uint32_t> indices(indexAccessor.count);
			fastgltf::iterateAccessorWithIndex<std::uint32_t>(_asset, indexAccessor,
			                                                  [&](std::uint32_t index, std::size_t idx)
			                                                  {
				                                                  indices[idx] = index;
			                                                  });

			mesh->Vertices = std::move(vertices);
			mesh->Indices = std::move(indices);

			meshes.push_back(std::move(mesh));
		}

		return true;
	}

	bool GltfReader::loadImage(fastgltf::Image& image, Engine& engine)
	{
		auto createImage = [&](unsigned char* data, int width, int height)
		{
			ImageParams params
			{
				.extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
				.format = VK_FORMAT_R8G8B8A8_UNORM,
				.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
				.mipLevels = 1
			};
			return engine.createImage(params, data);
		};

		// TODO use KTX2 library?

		std::visit(fastgltf::visitor{
			           [](auto &arg) {},
			           [&](fastgltf::sources::URI &filePath)
			           {
				           assert(filePath.fileByteOffset == 0); // We don't support offsets with stbi.
				           assert(filePath.uri.isLocalPath()); // We're only capable of loading local files.
				           int width, height, nrChannels;

				           const std::string path(filePath.uri.path().begin(), filePath.uri.path().end());
				           // Thanks C++.
				           unsigned char *data = stbi_load(path.c_str(), &width, &height, &nrChannels, 4);

				           images.push_back(std::move(createImage(data, width, height)));

				           stbi_image_free(data);
			           },
			           [&](fastgltf::sources::Array &vector)
			           {
				           int width, height, nrChannels;
				           unsigned char *data = stbi_load_from_memory(
					           reinterpret_cast<const stbi_uc *>(vector.bytes.data()),
					           static_cast<int>(vector.bytes.size()), &width, &height, &nrChannels, 4);

				           images.push_back(std::move(createImage(data, width, height)));

				           stbi_image_free(data);
			           },
			           [&](fastgltf::sources::BufferView &view)
			           {
				           auto &bufferView = _asset.bufferViews[view.bufferViewIndex];
				           auto &buffer = _asset.buffers[bufferView.bufferIndex];
				           // Yes, we've already loaded every buffer into some GL buffer. However, with GL it's simpler
				           // to just copy the buffer data again for the texture. Besides, this is just an example.
				           std::visit(fastgltf::visitor{
					                      // We only care about VectorWithMime here, because we specify LoadExternalBuffers, meaning
					                      // all buffers are already loaded into a vector.
					                      [](auto &arg) {},
					                      [&](fastgltf::sources::Array &vector)
					                      {
						                      int width, height, nrChannels;
						                      unsigned char *data = stbi_load_from_memory(
							                      reinterpret_cast<const stbi_uc *>(
								                      vector.bytes.data() + bufferView.byteOffset),
							                      static_cast<int>(bufferView.byteLength), &width, &height, &nrChannels,
							                      4);

						                      images.push_back(std::move(createImage(data, width, height)));

						                      stbi_image_free(data);
					                      }
				                      }, buffer.data);
			           },
		           }, image.data);

		return true;
	}

	bool GltfReader::loadMaterial(fastgltf::Material& gltfMaterial, Engine& engine)
	{
		auto myMaterial = std::make_unique<Material>(gltfMaterial.name.c_str());

		auto& pbrData = gltfMaterial.pbrData;
		myMaterial->baseColor.r = pbrData.baseColorFactor[0];
		myMaterial->baseColor.g = pbrData.baseColorFactor[1];
		myMaterial->baseColor.b = pbrData.baseColorFactor[2];
		myMaterial->baseColor.a = pbrData.baseColorFactor[3];

		myMaterial->metallicFactor = pbrData.metallicFactor;
		myMaterial->roughnessFactor = pbrData.roughnessFactor;
		myMaterial->emissiveFactor = 1.f; // gltfMaterial.emissiveFactor; TODO is a vec3

		// TODO
		// MaterialPass passType = MaterialPass::MainColor;
		// if (material.alphaMode == fastgltf::AlphaMode::Blend)
		// {
		// 	passType = MaterialPass::Transparent;
		// }

		// grab textures
		if (pbrData.baseColorTexture.has_value())
		{
			auto texture = _asset.textures[pbrData.baseColorTexture.value().textureIndex];
			size_t imgIndex = texture.imageIndex.value();
			size_t samplerIndex = texture.samplerIndex.value();
			myMaterial->baseColorMap = std::make_unique<Texture>(engine.getDevice(), images[imgIndex], samplers[samplerIndex]);
		}

		if (pbrData.metallicRoughnessTexture.has_value())
		{
			auto texture = _asset.textures[pbrData.metallicRoughnessTexture.value().textureIndex];
			size_t imgIndex = texture.imageIndex.value();
			size_t samplerIndex = texture.samplerIndex.value();
			myMaterial->metallicRoughnessMap = std::make_unique<Texture>(engine.getDevice(), images[imgIndex], samplers[samplerIndex]);
		}

		if (gltfMaterial.normalTexture.has_value())
		{
			auto texture = _asset.textures[gltfMaterial.normalTexture.value().textureIndex];
			size_t imgIndex = texture.imageIndex.value();
			size_t samplerIndex = texture.samplerIndex.value();
			myMaterial->normalMap = std::make_unique<Texture>(engine.getDevice(), images[imgIndex], samplers[samplerIndex]);
		}

		if (gltfMaterial.occlusionTexture.has_value())
		{
			auto texture = _asset.textures[gltfMaterial.occlusionTexture.value().textureIndex];
			size_t imgIndex = texture.imageIndex.value();
			size_t samplerIndex = texture.samplerIndex.value();
			myMaterial->occlusionMap = std::make_unique<Texture>(engine.getDevice(), images[imgIndex], samplers[samplerIndex]);
		}

		if (gltfMaterial.emissiveTexture.has_value())
		{
			auto texture = _asset.textures[gltfMaterial.emissiveTexture.value().textureIndex];
			size_t imgIndex = texture.imageIndex.value();
			size_t samplerIndex = texture.samplerIndex.value();
			myMaterial->emissiveMap = std::make_unique<Texture>(engine.getDevice(), images[imgIndex], samplers[samplerIndex]);
		}

		materials.push_back(std::move(myMaterial));

		return true;
	}
}
