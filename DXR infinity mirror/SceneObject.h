#pragma once
#include "GenericIncludes.h"
#include "Transform.h"

struct Vertex 
{
	float pos[3];
	float norm[3];
	float uv[2];

	Vertex(const std::array<float, 3>& position, const std::array<float, 3>& normal, const std::array<float, 2>& uvCoords)
	{
		for (int i = 0; i < 3; i++)
		{
			pos[i] = position[i];
			norm[i] = normal[i];
		}
		for (int i = 0; i < 2; i++)
		{
			uv[i] = uvCoords[i];
		}
	}
};

enum Scene_Object_Data 
{
	Scene_Object_Data_Null = 0,
	Scene_Object_Data_Meshes = 1,
	Scene_Object_Data_Materials = 2,
	Scene_Object_Data_All = 1|2
};

struct MeshGeometry
{
	uint32_t numVertecies;
	std::shared_ptr<Vertex> vertecies;

	uint32_t numIndecies;
	std::shared_ptr<uint32_t> indecies;
};

struct SceneObject
{
	uint32_t sceneObjectData;

	std::vector<MeshGeometry> meshGeometries;
	Transform transform;
};

SceneObject LoadSceneObjectFile(std::string file, Scene_Object_Data dataToLoad = Scene_Object_Data_All);
