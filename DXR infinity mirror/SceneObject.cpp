#include "SceneObject.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

SceneObject LoadSceneObjectFile(std::string file, Scene_Object_Data dataToLoad)
{
    Assimp::Importer importer;

    SceneObject output;
    output.sceneObjectData = Scene_Object_Data_Null;

    const aiScene* scene = importer.ReadFile(file, aiProcess_Triangulate);

    if (scene == nullptr)
    {
        std::cerr << "Error: Failed importing file: " << file << ". " << importer.GetErrorString() << std::endl;
        return output;
    }

    if (dataToLoad & Scene_Object_Data_Meshes)
    {
        for (uint32_t i = 0; i < scene->mNumMeshes; i++)
        {
            aiMesh* mesh = scene->mMeshes[i];

            if (mesh->mPrimitiveTypes != aiPrimitiveType_TRIANGLE)
            {
                std::cerr << "Error: failed parsing mesh " << i << " of file " << file << ". Unsupported primitive type\n";
                break;
            }

            if ((mesh->mNumVertices <= 0) || (mesh->mNumFaces <= 0))
            {
                std::cerr << "Error: failed parsing mesh " << i << " of file " << file << ". Incompatible data structure\n";
                break;
            }

            if (mesh->GetNumUVChannels() > 1)
            {
                std::cerr << "Warning: parsing mesh " << i << " of file " << file << ". Unutilized UV channels\n";
            }

            MeshGeometry subGeometry;
            subGeometry.numVertecies = mesh->mNumVertices;
            subGeometry.numIndecies = mesh->mNumFaces * 3; //each face is a triangle

            subGeometry.vertecies = std::shared_ptr<Vertex>((Vertex*)malloc(sizeof(Vertex) * subGeometry.numVertecies));
            subGeometry.indecies = std::shared_ptr<uint32_t>((uint32_t*)malloc(sizeof(uint32_t) * subGeometry.numIndecies));

            for (uint32_t j = 0; j < mesh->mNumVertices; j++)
            {
                if (mesh->GetNumUVChannels() > 0)
                    subGeometry.vertecies.get()[j] = Vertex({ mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z }, 
                                                            { mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z }, 
                                                            { mesh->mTextureCoords[0][j].x, mesh->mTextureCoords[0][j].y});
                else
                    subGeometry.vertecies.get()[j] = Vertex({ mesh->mVertices[j].x, mesh->mVertices[j].y, mesh->mVertices[j].z },
                                                            { mesh->mNormals[j].x, mesh->mNormals[j].y, mesh->mNormals[j].z },
                                                            { 0.0f, 0.0f });
            }

            for (uint32_t j = 0; j < mesh->mNumFaces; j++)
            {
                subGeometry.indecies.get()[j * 3] = mesh->mFaces[j].mIndices[0];
                subGeometry.indecies.get()[j * 3 + 1] = mesh->mFaces[j].mIndices[1];
                subGeometry.indecies.get()[j * 3 + 2] = mesh->mFaces[j].mIndices[2];
            }

            output.meshGeometries.push_back(subGeometry);
            output.sceneObjectData |= Scene_Object_Data_Meshes;
        }
    }
    
    return output;
}
