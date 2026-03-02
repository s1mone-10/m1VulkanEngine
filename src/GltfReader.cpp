#include <chrono>
#include <iostream>

#include <GltfReader.hpp>
#include <stb_image.h>

#include "graphics/Engine.hpp"
#include "graphics/SceneObject.hpp"

enum MaterialUniformFlags : std::uint32_t
{
	None = 0 << 0,
	HasBaseColorTexture = 1 << 0,
};

struct MaterialUniforms
{
	fastgltf::math::fvec4 baseColorFactor;
	float alphaCutoff = 0.f;
	std::uint32_t flags = 0;

	fastgltf::math::fvec2 padding;
};

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
		} else
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
			if (!bool(gltfFile))
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

			// We load images first.
			// TODO load sampler, texture. gltf use PBR
			for (auto &image: _asset.images)
				loadImage(image, engine);

			for (auto &material: _asset.materials)
				loadMaterial(material);

			// TODO node and node transformation??
			for (auto &mesh: _asset.meshes)
				loadMesh(mesh);

			for (auto &mesh: meshes)
			{
				auto sceneObj = SceneObject::createSceneObject();
				sceneObj->setMesh(mesh);
				engine.addSceneObject(std::move(sceneObj));
			}
		}

		return true;
	}

	bool GltfReader::loadMesh(fastgltf::Mesh &gltfMesh)
	{
		for (const auto &primitive: gltfMesh.primitives)
		{
			if (primitive.type != fastgltf::PrimitiveType::Triangles)
				continue;

			// Position
			auto position = primitive.findAttribute("POSITION");
			assert(position != primitive.attributes.end());
			// A mesh primitive is required to hold the POSITION attribute.
			assert(primitive.indicesAccessor.has_value()); // we should always have indices

			auto &positionAccessor = _asset.accessors[position->accessorIndex];
			if (!positionAccessor.bufferViewIndex.has_value())
				continue;
			std::vector<Vertex> vertices(positionAccessor.count);

			fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(_asset, positionAccessor,
			                                                          [&](fastgltf::math::fvec3 pos, std::size_t idx)
			                                                          {
				                                                          vertices[idx].pos = glm::vec3(
					                                                          pos.x(), pos.y(), pos.z());
			                                                          });

			// TODO
			std::size_t baseColorTexcoordIndex = 0;
			// if (primitive.materialIndex.has_value())
			// {
			// 	primitive.materialUniformsIndex = it->materialIndex.value() + 1; // Adjust for default material
			// 	auto& material = viewer->asset.materials[it->materialIndex.value()];
			//
			// 	auto& baseColorTexture = material.pbrData.baseColorTexture;
			// 	if (baseColorTexture.has_value()) {
			// 		auto& texture = viewer->asset.textures[baseColorTexture->textureIndex];
			// 		if (!texture.imageIndex.has_value())
			// 			return false;
			// 		primitive.albedoTexture = viewer->textures[texture.imageIndex.value()].texture;
			//
			// 		if (baseColorTexture->transform && baseColorTexture->transform->texCoordIndex.has_value()) {
			// 			baseColorTexcoordIndex = baseColorTexture->transform->texCoordIndex.value();
			// 		} else {
			// 			baseColorTexcoordIndex = material.pbrData.baseColorTexture->texCoordIndex;
			// 		}
			// 	}
			// } else {
			// 	primitive.materialUniformsIndex = 0;
			// }

			// Normal
			auto normal = primitive.findAttribute("NORMAL");
			if (normal != primitive.attributes.end())
			{
				const auto &normalAccessor = _asset.accessors[normal->accessorIndex];
				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec3>(_asset, normalAccessor,
				                                                          [&](fastgltf::math::fvec3 nor,
				                                                              std::size_t idx)
				                                                          {
					                                                          vertices[idx].normal = glm::vec3(
						                                                          nor.x(), nor.y(), nor.z());
				                                                          });
			}

			// TexCoord
			auto texcoordAttribute = std::string("TEXCOORD_") + std::to_string(baseColorTexcoordIndex);
			auto texCoord = primitive.findAttribute(texcoordAttribute);
			if (texCoord != primitive.attributes.end())
			{
				// Tex coord
				auto &texCoordAccessor = _asset.accessors[texCoord->accessorIndex];

				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec2>(_asset, texCoordAccessor,
				                                                          [&](fastgltf::math::fvec2 uv, std::size_t idx)
				                                                          {
					                                                          vertices[idx].texCoord = glm::vec2(
						                                                          uv.x(), uv.y());
				                                                          });
			}

			// vertex colors
			auto color = primitive.findAttribute("COLOR_0");
			if (color != primitive.attributes.end())
			{
				const auto &colorAccessor = _asset.accessors[color->accessorIndex];
				fastgltf::iterateAccessorWithIndex<fastgltf::math::fvec4>(_asset, colorAccessor,
				                                                          [&](fastgltf::math::fvec4 col,
				                                                              std::size_t idx)
				                                                          {
					                                                          vertices[idx].color = glm::vec3(
						                                                          col.x(), col.y(), col.z());
				                                                          });
			}

			// Indices
			auto &indexAccessor = _asset.accessors[primitive.indicesAccessor.value()];
			if (!indexAccessor.bufferViewIndex.has_value())
				return false;

			std::vector<uint32_t> indices(indexAccessor.count);
			fastgltf::iterateAccessorWithIndex<std::uint32_t>(_asset, indexAccessor,
			                                                  [&](std::uint32_t index, std::size_t idx)
			                                                  {
				                                                  indices[idx] = index;
			                                                  });

			std::shared_ptr<Mesh> mesh = std::make_shared<Mesh>();
			mesh->Vertices = std::move(vertices);
			mesh->Indices = std::move(indices);

			meshes.push_back(mesh);
		}

		return true;
	}

	bool GltfReader::loadImage(fastgltf::Image &image, Engine &engine)
	{
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

				           TextureParams params
				           {
					           .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
					           .format = VK_FORMAT_R8G8B8A8_UNORM
				           };
				           auto texture = engine.createTexture(params, data);
				           textures.push_back(std::move(texture));

				           stbi_image_free(data);
			           },
			           [&](fastgltf::sources::Array &vector)
			           {
				           int width, height, nrChannels;
				           unsigned char *data = stbi_load_from_memory(
					           reinterpret_cast<const stbi_uc *>(vector.bytes.data()),
					           static_cast<int>(vector.bytes.size()), &width, &height, &nrChannels, 4);

				           TextureParams params
				           {
					           .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height)},
					           .format = VK_FORMAT_R8G8B8A8_UNORM
				           };
				           auto texture = engine.createTexture(params, data);

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

						                      TextureParams params
						                      {
							                      .extent = {
								                      static_cast<uint32_t>(width), static_cast<uint32_t>(height)
							                      },
							                      .format = VK_FORMAT_R8G8B8A8_UNORM
						                      };
						                      auto texture = engine.createTexture(params, data);
						                      textures.push_back(std::move(texture));

						                      stbi_image_free(data);
					                      }
				                      }, buffer.data);
			           },
		           }, image.data);

		return true;
	}

	bool GltfReader::loadMaterial(fastgltf::Material &material)
	{
		auto myMaterial = std::make_unique<Material>(material.name.c_str());

		materials.push_back(std::move(myMaterial));

		// MaterialUniforms uniforms = {};
		// uniforms.alphaCutoff = material.alphaCutoff;
		//
		// uniforms.baseColorFactor = material.pbrData.baseColorFactor;
		// if (material.pbrData.baseColorTexture.has_value())
		// {
		// 	uniforms.flags |= MaterialUniformFlags::HasBaseColorTexture;
		// }
		//
		// viewer->materials.emplace_back(uniforms);
		return true;
	}

	//
	// bool loadCamera(Viewer* viewer, fastgltf::Camera& camera) {
	// 	// The following matrix math is for the projection matrices as defined by the glTF spec:
	// 	// https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#projection-matrices
	// 	std::visit(fastgltf::visitor {
	// 		[&](fastgltf::Camera::Perspective& perspective) {
	// 			fastgltf::math::fmat4x4 mat(0.0f);
	//
	// 			assert(viewer->windowDimensions[0] != 0 && viewer->windowDimensions[1] != 0);
	// 			auto aspectRatio = perspective.aspectRatio.value_or(
	// 				static_cast<float>(viewer->windowDimensions[0]) / static_cast<float>(viewer->windowDimensions[1]));
	// 			mat[0][0] = 1.f / (aspectRatio * tan(0.5f * perspective.yfov));
	// 			mat[1][1] = 1.f / (tan(0.5f * perspective.yfov));
	// 			mat[2][3] = -1;
	//
	// 			if (perspective.zfar.has_value()) {
	// 				// Finite projection matrix
	// 				mat[2][2] = (*perspective.zfar + perspective.znear) / (perspective.znear - *perspective.zfar);
	// 				mat[3][2] = (2 * *perspective.zfar * perspective.znear) / (perspective.znear - *perspective.zfar);
	// 			} else {
	// 				// Infinite projection matrix
	// 				mat[2][2] = -1;
	// 				mat[3][2] = -2 * perspective.znear;
	// 			}
	// 			viewer->cameras.emplace_back(mat);
	// 		},
	// 		[&](fastgltf::Camera::Orthographic& orthographic) {
	// 			fastgltf::math::fmat4x4 mat(1.0f);
	// 			mat[0][0] = 1.f / orthographic.xmag;
	// 			mat[1][1] = 1.f / orthographic.ymag;
	// 			mat[2][2] = 2.f / (orthographic.znear - orthographic.zfar);
	// 			mat[3][2] = (orthographic.zfar + orthographic.znear) / (orthographic.znear - orthographic.zfar);
	// 			viewer->cameras.emplace_back(mat);
	// 		},
	// 	}, camera.camera);
	// 	return true;
	// }
	//
	// void drawMesh(Viewer* viewer, std::size_t meshIndex, fastgltf::math::fmat4x4 matrix) {
	// 	auto& mesh = viewer->meshes[meshIndex];
	//
	// 	glBindBuffer(GL_DRAW_INDIRECT_BUFFER, mesh.drawsBuffer);
	//
	// 	glUniformMatrix4fv(viewer->modelMatrixUniform, 1, GL_FALSE, &matrix[0][0]);
	//
	// 	for (auto i = 0U; i < mesh.primitives.size(); ++i) {
	// 		auto& prim = mesh.primitives[i];
	// 		auto& gltfPrimitive = viewer->asset.meshes[meshIndex].primitives[i];
	//
	// 		std::size_t materialIndex;
	// 		auto& mappings = gltfPrimitive.mappings;
	// 		if (!mappings.empty() && mappings[viewer->materialVariant].has_value()) {
	// 			materialIndex = mappings[viewer->materialVariant].value() + 1; // Adjust for default material
	// 		} else {
	// 			materialIndex = prim.materialUniformsIndex;
	// 		}
	//
	// 		auto& material = viewer->materialBuffers[materialIndex];
	// 		glBindTextureUnit(0, prim.albedoTexture);
	// 		glBindBufferBase(GL_UNIFORM_BUFFER, 0, material);
	// 		glBindVertexArray(prim.vertexArray);
	//
	// 		// Update texture transform uniforms
	// 		glUniform2f(viewer->uvOffsetUniform, 0, 0);
	// 		glUniform2f(viewer->uvScaleUniform, 1.f, 1.f);
	// 		glUniform1f(viewer->uvRotationUniform, 0);
	// 		if (materialIndex != 0) {
	// 			auto& gltfMaterial = viewer->asset.materials[materialIndex - 1];
	// 			if (gltfMaterial.pbrData.baseColorTexture.has_value() && gltfMaterial.pbrData.baseColorTexture->transform) {
	// 				auto& transform = gltfMaterial.pbrData.baseColorTexture->transform;
	// 				glUniform2f(viewer->uvOffsetUniform, transform->uvOffset[0], transform->uvOffset[1]);
	// 				glUniform2f(viewer->uvScaleUniform, transform->uvScale[0], transform->uvScale[1]);
	// 				glUniform1f(viewer->uvRotationUniform, static_cast<float>(transform->rotation));
	// 			}
	// 		}
	//
	// 		glDrawElementsIndirect(prim.primitiveType, prim.indexType,
	// 							   reinterpret_cast<const void*>(i * sizeof(Primitive)));
	// 	}
	// }
	//
	// void updateCameraNodes(Viewer* viewer, std::vector<fastgltf::Node*>& cameraNodes, std::size_t nodeIndex) {
	// 	// This function recursively traverses the node hierarchy starting with the node at nodeIndex
	// 	// to find any nodes holding cameras.
	// 	auto& node = viewer->asset.nodes[nodeIndex];
	//
	// 	if (node.cameraIndex.has_value()) {
	// 		if (node.name.empty()) {
	// 			// Always have a non-empty string for the ImGui UI
	// 			node.name = std::string("Camera ") + std::to_string(cameraNodes.size());
	// 		}
	// 		cameraNodes.emplace_back(&node);
	// 	}
	//
	// 	for (auto& child : node.children) {
	// 		updateCameraNodes(viewer, cameraNodes, child);
	// 	}
	// }
}
