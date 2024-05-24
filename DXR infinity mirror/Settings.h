#pragma once
#include <windows.h>
#include <d3d12.h>

// Window init
#define APPLICATION_NAME L"Infinimirror_Test" //The name of the application and window
const unsigned int SCREEN_WIDTH = 3840; //Width of application.
const unsigned int SCREEN_HEIGHT = 2160;	//Height of application.
//

// DX config
const D3D_FEATURE_LEVEL MINIMUM_FEATURE_LEVEL = D3D_FEATURE_LEVEL_12_1;
const unsigned int NUM_SWAP_BUFFERS = 2;

const unsigned int MAX_RAY_DEPTH = 31;

const DWORD EVENT_TIMEOUT_MILLISECONDS = 1000; //one second

//Shader Names
#define RAY_GEN_SHADER_NAME L"rayGen";
#define MISS_SHADER_NAME L"miss";

#define MIRROR_HIT_GROUP_SHADER_NAME L"HitGroup_Mirror";
#define MIRROR_CLOSEST_HIT_SHADER_NAME L"closestHit_mirror";
#define EDGES_HIT_GROUP_SHADER_NAME L"HitGroup_edges";
#define EDGES_CLOSEST_HIT_SHADER_NAME L"closestHit_edges";
//

// Test Setup
#define MODEL_FILEPATH "mirrorTest.fbx"
//#define MODEL_FILEPATH "mirrorTestSmooth.fbx"

#define MODEL_PARTS 2 //do not modify, current geometry loading is unfinished and assumes value 2
