#include "log/Log.hpp"
#include "graphics/Engine.hpp"
#include "graphics/SceneObject.hpp"
#include "geometry/Vertex.hpp"

//libs
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

void loadScene(m1::Engine& engine);
void loadObj(m1::Engine& engine, const std::string &path);
void loadCubes(m1::Engine& engine, const uint32_t numCubes);

int main()
{
    m1::Log::Get().Info("Application starting");
    m1::Engine app;

    try
    {
        loadScene(app);
        app.run();
    }
    catch (const std::exception &e)
    {
        m1::Log::Get().Error(e.what());
        return EXIT_FAILURE;
    }

    m1::Log::Get().Info("Application finished successfully");
    return EXIT_SUCCESS;
}

void loadScene(m1::Engine& engine)
{
    const std::string MODEL_PATH = "../resources/viking_room.obj";
    //const std::string MODEL_PATH = "../resources/colored_cube.obj";
    //const std::string MODEL_PATH = "../resources/smooth_vase.obj";
    //const std::string MODEL_PATH = "../resources/flat_vase.obj";
    const std::string TEXTURE_PATH = "../resources/viking_room.png";


    loadCubes(engine, 1);
    //loadObj(engine, MODEL_PATH);
}

void loadObj(m1::Engine& engine, const std::string &path)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warn, err;

    if (!tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err, path.c_str()))
    {
        throw std::runtime_error(warn + err);
    }
    
    std::unordered_map<m1::Vertex, uint32_t> uniqueVertices{};
    auto sceneObj = m1::SceneObject::createSceneObject();

    for (const auto& shape : shapes)
    {
        for (const auto& index : shape.mesh.indices)
        {
            m1::Vertex vertex{};

            if (index.vertex_index >= 0)
            {
                vertex.pos = {
                    attrib.vertices[3 * index.vertex_index + 0],
                    attrib.vertices[3 * index.vertex_index + 1],
                    attrib.vertices[3 * index.vertex_index + 2],
                };

                vertex.color = {
                    attrib.colors[3 * index.vertex_index + 0],
                    attrib.colors[3 * index.vertex_index + 1],
                    attrib.colors[3 * index.vertex_index + 2],
                };
            }

            if (index.normal_index >= 0)
            {
                vertex.normal = {
                    attrib.normals[3 * index.normal_index + 0],
                    attrib.normals[3 * index.normal_index + 1],
                    attrib.normals[3 * index.normal_index + 2],
                };
            }

            if (index.texcoord_index >= 0)
            {
                vertex.texCoord = {
                    attrib.texcoords[2 * index.texcoord_index + 0],
                    1.0f - attrib.texcoords[2 * index.texcoord_index + 1]
                };
            }

            if (uniqueVertices.count(vertex) == 0)
            {
                uniqueVertices[vertex] = static_cast<uint32_t>(sceneObj->Mesh->Vertices.size());
                sceneObj->Mesh->Vertices.push_back(vertex);
            }

            sceneObj->Mesh->Indices.push_back(uniqueVertices[vertex]);
        }
    }

    engine.addSceneObject(std::move(sceneObj));
}

void loadCubes(m1::Engine &engine, const uint32_t numCubes)
{
    auto sceneObj = m1::SceneObject::createSceneObject();

    sceneObj->setMesh(m1::Mesh::createCube());

    engine.addSceneObject(std::move(sceneObj));
}

