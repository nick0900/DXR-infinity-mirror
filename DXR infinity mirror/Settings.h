#pragma once
#include <windows.h>
#include <d3d12.h>

// Window init
#define APPLICATION_NAME L"Infinimirror_Test" //The name of the application and window
const unsigned int SCREEN_WIDTH = 640; //Width of application.
const unsigned int SCREEN_HEIGHT = 480;	//Height of application.
//

// DX config
const D3D_FEATURE_LEVEL MINIMUM_FEATURE_LEVEL = D3D_FEATURE_LEVEL_12_1;
const unsigned int NUM_SWAP_BUFFERS = 2; //Number of buffers

//Shader Names
#define RAY_GEN_SHADER_NAME L"rayGen";
#define MISS_SHADER_NAME L"miss";

#define MIRROR_HIT_GROUP_SHADER_NAME L"HitGroup_Mirror";
#define MIRROR_CLOSEST_HIT_SHADER_NAME L"closestHit_mirror";
#define EDGES_HIT_GROUP_SHADER_NAME L"HitGroup_edges";
#define EDGES_CLOSEST_HIT_SHADER_NAME L"closestHit_edges";
//

#define MODEL_PARTS 2
