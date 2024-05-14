#include "DX12Base.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#pragma comment (lib, "d3d12.lib")
#pragma comment (lib, "DXGI.lib")
#include <DirectXMath.h>


#include "Settings.h"
#include "GenericIncludes.h"
#include "WindowsHelper.h"
#include "SceneObject.h"
#include "ShaderCompiler.h"

const WCHAR* sRayGen = RAY_GEN_SHADER_NAME;
const WCHAR* sMiss = MISS_SHADER_NAME;

const WCHAR* sHitGroupMirror = MIRROR_HIT_GROUP_SHADER_NAME;
const WCHAR* sClosestHitMirror = MIRROR_CLOSEST_HIT_SHADER_NAME;
const WCHAR* sHitGroupEdges = EDGES_HIT_GROUP_SHADER_NAME;
const WCHAR* sClosestHitEdges = EDGES_CLOSEST_HIT_SHADER_NAME;

template<class Interface>
inline void SafeRelease(Interface** ppInterfaceToRelease)
{
	if (*ppInterfaceToRelease != NULL)
	{
		(*ppInterfaceToRelease)->Release();

		(*ppInterfaceToRelease) = NULL;
	}
}

struct AccelerationStructureBuffers
{
	ID3D12Resource1* pScratch = nullptr;
	ID3D12Resource1* pResult = nullptr;
	ID3D12Resource1* pInstanceDesc = nullptr;    // Used only for top-level AS

	~AccelerationStructureBuffers()
	{
		SafeRelease(&pScratch);
		SafeRelease(&pResult);
		SafeRelease(&pInstanceDesc);
	}
};

struct ShaderTableData
{
	UINT64 SizeInBytes;
	UINT32 StrideInBytes;
	ID3D12Resource1* Resource = nullptr;

	~ShaderTableData()
	{
		SafeRelease(&Resource);
	}
};

namespace Base
{
	ID3D12Device5* Dx12Device;
	IDXGISwapChain4* DxgiSwapChain4;
	
	namespace Queues
	{
		namespace Direct
		{
			ID3D12CommandQueue* Dx12Queue;
			ID3D12CommandAllocator* Dx12CommandAllocator;
			ID3D12GraphicsCommandList4* Dx12CommandList4;
		}

		namespace Compute
		{
			ID3D12CommandQueue* Dx12Queue;
			ID3D12CommandAllocator* Dx12CommandAllocator;
			ID3D12GraphicsCommandList4* Dx12CommandList4;
		}
	}

	namespace Synchronization
	{
		ID3D12Fence1* Dx12Fence;
		UINT64 Dx12FenceValue;
		HANDLE Dx12FenceEvent;
	}
	
	
	namespace Resources
	{
		namespace Backbuffers
		{
			ID3D12DescriptorHeap* Dx12RTVDescriptorHeap;
			UINT Dx12RTVDescriptorStride;
			ID3D12Resource1* Dx12RTVResources[NUM_SWAP_BUFFERS];
		}

		namespace DXR
		{
			AccelerationStructureBuffers BottomBuffers[2] = { {}, {} };

			uint64_t ConservativeTopSize;
			AccelerationStructureBuffers TopBuffers{};

			ID3D12RootSignature* Dx12GlobalRS;

			ID3D12DescriptorHeap* Dx12RTDescriptorHeap;
			ID3D12Resource1* Dx12OutputResource;
			D3D12_CPU_DESCRIPTOR_HANDLE Dx12OutputUAV_CPUHandle;
			D3D12_CPU_DESCRIPTOR_HANDLE Dx12Accelleration_CPUHandle;

			namespace Shaders
			{
				ShaderTableData RayGenShaderTable{};
				ShaderTableData MissShaderTable{};
				ShaderTableData HitGroupShaderTable{};
			}
		}
	}

	namespace States
	{
		ID3D12StateObject* DXRPipelineState;
	}
}

void WaitForCompute()
{
	//WAITING FOR EACH FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
	//This is code implemented as such for simplicity. The cpu could for example be used
	//for other tasks to prepare the next frame while the current one is being rendered.

	//Signal and increment the fence value.
	const UINT64 fence = Base::Synchronization::Dx12FenceValue;
	Base::Queues::Compute::Dx12Queue->Signal(Base::Synchronization::Dx12Fence, fence);
	Base::Synchronization::Dx12FenceValue++;

	//Wait until command queue is done.
	if (Base::Synchronization::Dx12Fence->GetCompletedValue() < fence)
	{
		Base::Synchronization::Dx12Fence->SetEventOnCompletion(fence, Base::Synchronization::Dx12FenceEvent);
		WaitForSingleObject(Base::Synchronization::Dx12FenceEvent, INFINITE);
	}
}

void WaitForDirect()
{
	//WAITING FOR EACH FRAME TO COMPLETE BEFORE CONTINUING IS NOT BEST PRACTICE.
	//This is code implemented as such for simplicity. The cpu could for example be used
	//for other tasks to prepare the next frame while the current one is being rendered.

	//Signal and increment the fence value.
	const UINT64 fence = Base::Synchronization::Dx12FenceValue;
	Base::Queues::Direct::Dx12Queue->Signal(Base::Synchronization::Dx12Fence, fence);
	Base::Synchronization::Dx12FenceValue++;

	//Wait until command queue is done.
	if (Base::Synchronization::Dx12Fence->GetCompletedValue() < fence)
	{
		Base::Synchronization::Dx12Fence->SetEventOnCompletion(fence, Base::Synchronization::Dx12FenceEvent);
		WaitForSingleObject(Base::Synchronization::Dx12FenceEvent, INFINITE);
	}
}

#define NameInterface(dxInterface) dxInterface->SetName(L#dxInterface)

#define NameInterfaceIndex(dxInterface, i) dxInterface->SetName(L#dxInterface L" Index:" L#i)

int CreateDirect3DDevice();
int CreateCommandInterfaces();
int CreateSwapChain(HWND wndHandle);
int CreateFenceAndEventHandle();
int CreateRenderTargets();
int CreateAccelerationStructures();
int CreateRaytracingPipelineState();
int CreateShaderResources();
int CreateShaderTables();

int DX12Setup(HWND wndHandle)
{
	if (CreateDirect3DDevice() != 0) return 1;

	if (CreateCommandInterfaces() != 0) return 1;

	if (CreateSwapChain(wndHandle) != 0) return 1;

	if (CreateFenceAndEventHandle() != 0) return 1;

	if (CreateRenderTargets()) return 1;

	if (CreateAccelerationStructures() != 0) return 1;

	if (CreateRaytracingPipelineState() != 0) return 1;

	if (CreateShaderResources() != 0) return 1;

	//last
	if (CreateShaderTables() != 0) return 1;
	
	return 0;
}

void DX12Free()
{
	SafeRelease(&Base::Resources::DXR::Dx12OutputResource);
	SafeRelease(&Base::Resources::DXR::Dx12RTDescriptorHeap);
	SafeRelease(&Base::States::DXRPipelineState);
	SafeRelease(&Base::Resources::DXR::Dx12GlobalRS);


	SafeRelease(&Base::Resources::Backbuffers::Dx12RTVDescriptorHeap);
	for (int i = 0; i < NUM_SWAP_BUFFERS; i++)
	{
		SafeRelease(&Base::Resources::Backbuffers::Dx12RTVResources[i]);
	}
	
	CloseHandle(Base::Synchronization::Dx12FenceEvent);
	SafeRelease(&Base::Synchronization::Dx12Fence);

	SafeRelease(&Base::DxgiSwapChain4);

	SafeRelease(&Base::Queues::Compute::Dx12CommandList4);
	SafeRelease(&Base::Queues::Compute::Dx12CommandAllocator);
	SafeRelease(&Base::Queues::Direct::Dx12CommandList4);
	SafeRelease(&Base::Queues::Direct::Dx12CommandAllocator);
	SafeRelease(&Base::Queues::Compute::Dx12Queue);
	SafeRelease(&Base::Queues::Direct::Dx12Queue);

	SafeRelease(&Base::Dx12Device);
}

IDXGIAdapter1* GetAdapterSupportingDXR()
{
	//dxgi1_6 is only needed for the initialization process using the adapter.
	IDXGIFactory6* factory = nullptr;

	//First a factory is created to iterate through the adapters available.
	CreateDXGIFactory(IID_PPV_ARGS(&factory));

	IDXGIAdapter1* adapter = nullptr;
	UINT adapterIndex = 0;
	while (SUCCEEDED(factory->EnumAdapters1(adapterIndex++, &adapter)))
	{
		// Create device and check to see if the adapter supports DXR
		ID3D12Device* pDevice = nullptr;
		if (SUCCEEDED(D3D12CreateDevice(adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&pDevice))))
		{
			D3D12_FEATURE_DATA_D3D12_OPTIONS5 features5;
			HRESULT hr = pDevice->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &features5, sizeof(D3D12_FEATURE_DATA_D3D12_OPTIONS5));
			SafeRelease(&pDevice);
			
			if (SUCCEEDED(hr))
			{
				if (features5.RaytracingTier != D3D12_RAYTRACING_TIER_NOT_SUPPORTED)
				{
					break; //found one!
				}
			}
		}

		SafeRelease(&adapter);
	}

	SafeRelease(&factory);

	return adapter;
}

int CreateDirect3DDevice()
{
#ifdef _DEBUG
	//Enable the D3D12 debug layer.
	ID3D12Debug* debugController = nullptr;

#ifdef STATIC_LINK_DEBUGSTUFF
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
	}
	SafeRelease(debugController);
#else
	HMODULE mD3D12 = GetModuleHandle(L"D3D12.dll");
	PFN_D3D12_GET_DEBUG_INTERFACE f = (PFN_D3D12_GET_DEBUG_INTERFACE)GetProcAddress(mD3D12, "D3D12GetDebugInterface");
	if (SUCCEEDED(f(IID_PPV_ARGS(&debugController))))
	{
		debugController->EnableDebugLayer();
	}
	SafeRelease(&debugController);
#endif
#endif

	IDXGIAdapter1* adapter_supporting_dxr = GetAdapterSupportingDXR();
	if (adapter_supporting_dxr != nullptr)
	{
		// Create device and check to see if the adapter supports DXR
		if (SUCCEEDED(D3D12CreateDevice(adapter_supporting_dxr, MINIMUM_FEATURE_LEVEL, IID_PPV_ARGS(&Base::Dx12Device))))
		{
			std::cout << "Successfully created DXR compatible device\n";
			SafeRelease(&adapter_supporting_dxr);
			NameInterface(Base::Dx12Device);
			return 0;
		}
		std::cerr << "Error: No DXR compatible device matching minimum feature level\n";
		SafeRelease(&adapter_supporting_dxr);
		return 1;
	}
	std::cerr << "Error: No DXR compatible device available\n";
	return 1;
}


int CreateCommandInterfaces()
{
	do
	{
		//Describe and create the command queue.
		D3D12_COMMAND_QUEUE_DESC cqd = {};
		cqd.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		if (FAILED(Base::Dx12Device->CreateCommandQueue(&cqd, IID_PPV_ARGS(&Base::Queues::Direct::Dx12Queue)))) break;
		NameInterface(Base::Queues::Direct::Dx12Queue);

		cqd.Type = D3D12_COMMAND_LIST_TYPE_COMPUTE;
		if (FAILED(Base::Dx12Device->CreateCommandQueue(&cqd, IID_PPV_ARGS(&Base::Queues::Compute::Dx12Queue)))) break;
		NameInterface(Base::Queues::Compute::Dx12Queue);

		//Create command allocator. The command allocator object corresponds
		//to the underlying allocations in which GPU commands are stored.
		if (FAILED(Base::Dx12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Base::Queues::Direct::Dx12CommandAllocator)))) break;
		NameInterface(Base::Queues::Direct::Dx12CommandAllocator);

		if (FAILED(Base::Dx12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_COMPUTE, IID_PPV_ARGS(&Base::Queues::Compute::Dx12CommandAllocator)))) break;
		NameInterface(Base::Queues::Compute::Dx12CommandAllocator);

		//Create command list.
		if (FAILED(Base::Dx12Device->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			Base::Queues::Direct::Dx12CommandAllocator,
			nullptr,
			IID_PPV_ARGS(&Base::Queues::Direct::Dx12CommandList4)))) break;
		NameInterface(Base::Queues::Direct::Dx12CommandList4);

		if (FAILED(Base::Dx12Device->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_COMPUTE,
			Base::Queues::Compute::Dx12CommandAllocator,
			nullptr,
			IID_PPV_ARGS(&Base::Queues::Compute::Dx12CommandList4)))) break;
		NameInterface(Base::Queues::Compute::Dx12CommandList4);

		//Command lists are created in the recording state. Since there is nothing to
		//record right now and the main loop expects it to be closed, we close it.
		Base::Queues::Direct::Dx12CommandList4->Close();
		Base::Queues::Compute::Dx12CommandList4->Close();

		std::cout << "Command Queues setup successful\n";
		return 0;

	} while (false);
	
	std::cout << "Command Queues setup failed\n";
	return 1;
}


int CreateSwapChain(HWND wndHandle)
{
	IDXGIFactory5* factory = nullptr;
	CreateDXGIFactory(IID_PPV_ARGS(&factory));

	//Create swap chain.
	DXGI_SWAP_CHAIN_DESC1 scDesc = {};
	scDesc.Width = 0;
	scDesc.Height = 0;
	scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	scDesc.Stereo = FALSE;
	scDesc.SampleDesc.Count = 1;
	scDesc.SampleDesc.Quality = 0;
	scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scDesc.BufferCount = NUM_SWAP_BUFFERS;
	scDesc.Scaling = DXGI_SCALING_NONE;
	scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	scDesc.Flags = 0;
	scDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;

	IDXGISwapChain1* swapChain1 = nullptr;
	if (SUCCEEDED(factory->CreateSwapChainForHwnd(
		Base::Queues::Direct::Dx12Queue,
		wndHandle,
		&scDesc,
		nullptr,
		nullptr,
		&swapChain1)))
	{
		if (SUCCEEDED(swapChain1->QueryInterface(IID_PPV_ARGS(&Base::DxgiSwapChain4))))
		{
			Base::DxgiSwapChain4->Release();
		}
		else
		{
			std::cerr << "Error: Failed to get SwapChain4\n";
			SafeRelease(&factory);
			return 1;
		}
	}
	else
	{
		std::cerr << "Error: Failed to create SwapChain1\n";
		SafeRelease(&factory);
		return 1;
	}
	std::cout << "SwapChain setup successful\n";
	SafeRelease(&factory);
	return 0;
}


int CreateFenceAndEventHandle()
{
	if (FAILED(Base::Dx12Device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&Base::Synchronization::Dx12Fence))))
	{
		std::cerr << "Error: Fence creation failed\n";
		return 1;
	}
	NameInterface(Base::Synchronization::Dx12Fence);
	Base::Synchronization::Dx12FenceValue = 1;
	//Create an event handle to use for GPU synchronization.
	Base::Synchronization::Dx12FenceEvent = CreateEvent(0, false, false, 0);

	std::cout << "Fence setup successful\n";
	return 0;
}


int CreateRenderTargets()
{
	//Create descriptor heap for render target views.
	D3D12_DESCRIPTOR_HEAP_DESC dhd = {};
	dhd.NumDescriptors = NUM_SWAP_BUFFERS;
	dhd.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;

	if (FAILED(Base::Dx12Device->CreateDescriptorHeap(&dhd, IID_PPV_ARGS(&Base::Resources::Backbuffers::Dx12RTVDescriptorHeap))))
	{
		std::cerr << "Error: Backbuffer RTV heap creation failed\n";
		return 1;
	}
	NameInterface(Base::Resources::Backbuffers::Dx12RTVDescriptorHeap);

	//Create resources for the render targets.
	Base::Resources::Backbuffers::Dx12RTVDescriptorStride = Base::Dx12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	D3D12_CPU_DESCRIPTOR_HANDLE cdh = Base::Resources::Backbuffers::Dx12RTVDescriptorHeap->GetCPUDescriptorHandleForHeapStart();

	//One RTV for each frame.
	for (UINT n = 0; n < NUM_SWAP_BUFFERS; n++)
	{
		if (FAILED(Base::DxgiSwapChain4->GetBuffer(n, IID_PPV_ARGS(&Base::Resources::Backbuffers::Dx12RTVResources[n]))))
		{
			std::cerr << "Error: Getting backbuffer failed\n";
			return 1;
		}
		NameInterfaceIndex(Base::Resources::Backbuffers::Dx12RTVResources[n], n);
		Base::Dx12Device->CreateRenderTargetView(Base::Resources::Backbuffers::Dx12RTVResources[n], nullptr, cdh);
		cdh.ptr += Base::Resources::Backbuffers::Dx12RTVDescriptorStride;
	}
	std::cout << "Backbuffer RTVs setup successful\n";
	return 0;
}

static const D3D12_HEAP_PROPERTIES uploadHeapProperties =
{
	D3D12_HEAP_TYPE_UPLOAD,
	D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	D3D12_MEMORY_POOL_UNKNOWN,
	0,
	0,
};

static const D3D12_HEAP_PROPERTIES defaultHeapProps =
{
	D3D12_HEAP_TYPE_DEFAULT,
	D3D12_CPU_PAGE_PROPERTY_UNKNOWN,
	D3D12_MEMORY_POOL_UNKNOWN,
	0,
	0
};

ID3D12Resource1* createBuffer(uint64_t size, D3D12_RESOURCE_FLAGS flags, D3D12_RESOURCE_STATES initState, const D3D12_HEAP_PROPERTIES& heapProps)
{
	D3D12_RESOURCE_DESC bufDesc = {};
	bufDesc.Alignment = 0;
	bufDesc.DepthOrArraySize = 1;
	bufDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufDesc.Flags = flags;
	bufDesc.Format = DXGI_FORMAT_UNKNOWN;
	bufDesc.Height = 1;
	bufDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
	bufDesc.MipLevels = 1;
	bufDesc.SampleDesc.Count = 1;
	bufDesc.SampleDesc.Quality = 0;
	bufDesc.Width = size;

	ID3D12Resource1* pBuffer = nullptr;
	Base::Dx12Device->CreateCommittedResource(&heapProps, D3D12_HEAP_FLAG_NONE, &bufDesc, initState, nullptr, IID_PPV_ARGS(&pBuffer));
	return pBuffer;
}

ID3D12Resource1* createTriangleVB(MeshGeometry* mesh)
{
	// For simplicity, we create the vertex buffer on the upload heap, but that's not required
	ID3D12Resource1* pBuffer = createBuffer(sizeof(Vertex) * mesh->numVertecies, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, uploadHeapProperties);
	uint8_t* pData;
	pBuffer->Map(0, nullptr, (void**)&pData);
	memcpy(pData, mesh->vertecies.get(), sizeof(Vertex) * mesh->numVertecies);
	pBuffer->Unmap(0, nullptr);
	return pBuffer;
}

ID3D12Resource1* createTriangleIB(MeshGeometry* mesh)
{
	// For simplicity, we create the index buffer on the upload heap, but that's not required
	ID3D12Resource1* pBuffer = createBuffer(sizeof(uint32_t) * mesh->numIndecies, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, uploadHeapProperties);
	uint8_t* pData;
	pBuffer->Map(0, nullptr, (void**)&pData);
	memcpy(pData, mesh->indecies.get(), sizeof(uint32_t) * mesh->numIndecies);
	pBuffer->Unmap(0, nullptr);
	return pBuffer;
}

void SetupGeometryDesc(D3D12_RAYTRACING_GEOMETRY_DESC* geomDesc, ID3D12Resource1* vertexBuffer, uint32_t numVertecies, ID3D12Resource1* indexBuffer, uint32_t numIndecies)
{
	geomDesc->Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;

	geomDesc->Triangles.VertexBuffer.StartAddress = vertexBuffer->GetGPUVirtualAddress();
	geomDesc->Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
	geomDesc->Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	geomDesc->Triangles.VertexCount = numVertecies;

	geomDesc->Triangles.IndexBuffer = indexBuffer->GetGPUVirtualAddress();
	geomDesc->Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
	geomDesc->Triangles.IndexCount = numIndecies;

	geomDesc->Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;
}

void createBottomLevelAS(ID3D12GraphicsCommandList4* pCmdList, D3D12_RAYTRACING_GEOMETRY_DESC* geomDesc, uint32_t numDescs, AccelerationStructureBuffers* ASBuffers)
{
	// Get the size requirements for the scratch and AS buffers
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	inputs.NumDescs = numDescs;
	inputs.pGeometryDescs = geomDesc;
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL;

	D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info = {};
	Base::Dx12Device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

	// Create the buffers. They need to support UAV, and since we are going to immediately use them, we create them with an unordered-access state

	ASBuffers->pScratch = createBuffer(info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, defaultHeapProps);
	ASBuffers->pResult = createBuffer(info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, defaultHeapProps);

	// Create the bottom-level AS
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
	asDesc.Inputs = inputs;
	asDesc.DestAccelerationStructureData = ASBuffers->pResult->GetGPUVirtualAddress();
	asDesc.ScratchAccelerationStructureData = ASBuffers->pScratch->GetGPUVirtualAddress();

	pCmdList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

	// We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
	D3D12_RESOURCE_BARRIER uavBarrier = {};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = ASBuffers->pResult;
	pCmdList->ResourceBarrier(1, &uavBarrier);
}

void createTopLevelAS(ID3D12GraphicsCommandList4* pCmdList)
{
	// First, get the size of the TLAS buffers and create them
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	inputs.NumDescs = MODEL_PARTS;
	inputs.Type = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL;

	// on first call, create the buffer
	if (Base::Resources::DXR::TopBuffers.pInstanceDesc == nullptr)
	{
		D3D12_RAYTRACING_ACCELERATION_STRUCTURE_PREBUILD_INFO info;
		Base::Dx12Device->GetRaytracingAccelerationStructurePrebuildInfo(&inputs, &info);

		// Create the buffers
		if (Base::Resources::DXR::TopBuffers.pScratch == nullptr)
		{
			Base::Resources::DXR::TopBuffers.pScratch = createBuffer(info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, defaultHeapProps);
		}

		if (Base::Resources::DXR::TopBuffers.pResult == nullptr)
		{
			Base::Resources::DXR::TopBuffers.pResult = createBuffer(info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, defaultHeapProps);
		}
		Base::Resources::DXR::ConservativeTopSize = info.ResultDataMaxSizeInBytes;

		Base::Resources::DXR::TopBuffers.pInstanceDesc = createBuffer(
			sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * MODEL_PARTS,
			D3D12_RESOURCE_FLAG_NONE,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			uploadHeapProperties);
	}

	D3D12_RAYTRACING_INSTANCE_DESC* pInstanceDesc;
	Base::Resources::DXR::TopBuffers.pInstanceDesc->Map(0, nullptr, (void**)&pInstanceDesc);

	static float rotY = 0;
	rotY += 0.001f;
	for (int i = 0; i < MODEL_PARTS; i++)
	{
		pInstanceDesc->InstanceID = i;                            // exposed to the shader via InstanceID()
		pInstanceDesc->InstanceContributionToHitGroupIndex = i;   // offset inside the shader-table. we only have a single geometry, so the offset 0
		pInstanceDesc->Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;

		
		//apply transform
		DirectX::XMFLOAT3X4 m;
		DirectX::XMStoreFloat3x4(&m, DirectX::XMMatrixScaling(0.5f, 0.5f, 0.5f) * DirectX::XMMatrixRotationY(0.25f + rotY) * DirectX::XMMatrixTranslation(0, 0, 0));
		memcpy(pInstanceDesc->Transform, &m, sizeof(pInstanceDesc->Transform));

		pInstanceDesc->AccelerationStructure = Base::Resources::DXR::BottomBuffers[i].pResult->GetGPUVirtualAddress();
		pInstanceDesc->InstanceMask = 0xFF;

		pInstanceDesc++;
	}
	// Unmap
	Base::Resources::DXR::TopBuffers.pInstanceDesc->Unmap(0, nullptr);

	// Create the TLAS
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
	asDesc.Inputs = inputs;
	asDesc.Inputs.InstanceDescs = Base::Resources::DXR::TopBuffers.pInstanceDesc->GetGPUVirtualAddress();
	asDesc.DestAccelerationStructureData = Base::Resources::DXR::TopBuffers.pResult->GetGPUVirtualAddress();
	asDesc.ScratchAccelerationStructureData = Base::Resources::DXR::TopBuffers.pScratch->GetGPUVirtualAddress();

	pCmdList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

	// UAV barrier needed before using the acceleration structures in a raytracing operation
	D3D12_RESOURCE_BARRIER uavBarrier = {};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = Base::Resources::DXR::TopBuffers.pResult;
	pCmdList->ResourceBarrier(1, &uavBarrier);
}

int CreateAccelerationStructures()
{
	Base::Queues::Compute::Dx12CommandAllocator->Reset();
	Base::Queues::Compute::Dx12CommandList4->Reset(Base::Queues::Compute::Dx12CommandAllocator, nullptr);

	SceneObject infiniMirror = LoadSceneObjectFile("mirrorTest.fbx");
	if (infiniMirror.sceneObjectData == Scene_Object_Data_Null)
	{
		std::cerr << "Error: Failed loading model test\n";
		return 1;
	}

	ID3D12Resource1* mpVertexBuffer1 = createTriangleVB(&infiniMirror.meshGeometries[0]);
	ID3D12Resource1* mpIndexBuffer1 = createTriangleIB(&infiniMirror.meshGeometries[0]);
	ID3D12Resource1* mpVertexBuffer2 = createTriangleVB(&infiniMirror.meshGeometries[1]);
	ID3D12Resource1* mpIndexBuffer2 = createTriangleIB(&infiniMirror.meshGeometries[1]);

	D3D12_RAYTRACING_GEOMETRY_DESC geomDesc[2] = {};

	SetupGeometryDesc(&geomDesc[0], mpVertexBuffer1, infiniMirror.meshGeometries[0].numVertecies, mpIndexBuffer1, infiniMirror.meshGeometries[0].numIndecies);
	SetupGeometryDesc(&geomDesc[1], mpVertexBuffer2, infiniMirror.meshGeometries[1].numVertecies, mpIndexBuffer2, infiniMirror.meshGeometries[1].numIndecies);

	createBottomLevelAS(Base::Queues::Compute::Dx12CommandList4, geomDesc, 1, &Base::Resources::DXR::BottomBuffers[0]);
	createBottomLevelAS(Base::Queues::Compute::Dx12CommandList4, geomDesc + 1, 1, &Base::Resources::DXR::BottomBuffers[1]);
	createTopLevelAS(Base::Queues::Compute::Dx12CommandList4);

	Base::Queues::Compute::Dx12CommandList4->Close();
	ID3D12CommandList* listsToExec[] = { Base::Queues::Compute::Dx12CommandList4 };
	Base::Queues::Compute::Dx12Queue->ExecuteCommandLists(_countof(listsToExec), listsToExec);

	WaitForCompute();

	SafeRelease(&mpVertexBuffer1);
	SafeRelease(&mpVertexBuffer2);
	SafeRelease(&mpIndexBuffer1);
	SafeRelease(&mpIndexBuffer2);

	std::cout << "DXR Acceleration Structures setup successful\n";
	return 0;
}

ID3D12RootSignature* createRayGenLocalRootSignature()
{
	D3D12_DESCRIPTOR_RANGE range[2]{};
	D3D12_ROOT_PARAMETER rootParams[1]{};

	range[0].BaseShaderRegister = 0;
	range[0].NumDescriptors = 1;
	range[0].RegisterSpace = 0;
	range[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_UAV;
	range[0].OffsetInDescriptorsFromTableStart = 0;

	// gRtScene
	range[1].BaseShaderRegister = 0;
	range[1].NumDescriptors = 1;
	range[1].RegisterSpace = 0;
	range[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
	range[1].OffsetInDescriptorsFromTableStart = 1;

	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootParams[0].DescriptorTable.NumDescriptorRanges = _countof(range);
	rootParams[0].DescriptorTable.pDescriptorRanges = range;

	// Create the desc
	D3D12_ROOT_SIGNATURE_DESC desc = {};
	desc.NumParameters = _countof(rootParams);
	desc.pParameters = rootParams;
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;


	ID3DBlob* pSigBlob = nullptr;
	ID3DBlob* pErrorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &pSigBlob, &pErrorBlob);
	if (FAILED(hr))
	{
		MessageBoxA(0, (char*)pErrorBlob->GetBufferPointer(), "", 0);
		//std::string msg = convertBlobToString(pErrorBlob.GetInterfacePtr());
		//logError(msg);
		return nullptr;
	}
	ID3D12RootSignature* pRootSig;
	Base::Dx12Device->CreateRootSignature(0, pSigBlob->GetBufferPointer(), pSigBlob->GetBufferSize(), IID_PPV_ARGS(&pRootSig));

	return pRootSig;
}

ID3D12RootSignature* createHitGroupLocalRootSignature()
{
	D3D12_ROOT_PARAMETER rootParams[1]{};

	//float3 ShaderTableColor;
	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParams[0].Constants.RegisterSpace = 1;
	rootParams[0].Constants.ShaderRegister = 0;
	rootParams[0].Constants.Num32BitValues = 3;

	D3D12_ROOT_SIGNATURE_DESC desc = {};
	desc.NumParameters = _countof(rootParams);
	desc.pParameters = rootParams;
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;


	ID3DBlob* pSigBlob;
	ID3DBlob* pErrorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &pSigBlob, &pErrorBlob);
	if (FAILED(hr))
	{
		//std::string msg = convertBlobToString(pErrorBlob.GetInterfacePtr());
		//logError(msg);
		return nullptr;
	}
	ID3D12RootSignature* pRootSig;
	Base::Dx12Device->CreateRootSignature(0, pSigBlob->GetBufferPointer(), pSigBlob->GetBufferSize(), IID_PPV_ARGS(&pRootSig));

	return pRootSig;
}

ID3D12RootSignature* createMissLocalRootSignature()
{
	D3D12_ROOT_SIGNATURE_DESC desc{};
	desc.NumParameters = 0;
	desc.pParameters = nullptr;
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_LOCAL_ROOT_SIGNATURE;

	ID3DBlob* pSigBlob;
	ID3DBlob* pErrorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &pSigBlob, &pErrorBlob);
	if (FAILED(hr))
	{
		//std::string msg = convertBlobToString(pErrorBlob.GetInterfacePtr());
		//logError(msg);
		return nullptr;
	}
	ID3D12RootSignature* pRootSig;
	Base::Dx12Device->CreateRootSignature(0, pSigBlob->GetBufferPointer(), pSigBlob->GetBufferSize(), IID_PPV_ARGS(&pRootSig));

	return pRootSig;
}

ID3D12RootSignature* createGlobalRootSignature()
{
	D3D12_ROOT_PARAMETER rootParams[1]{};

	//float3 ShaderTableColor;
	rootParams[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
	rootParams[0].Constants.RegisterSpace = 0;
	rootParams[0].Constants.ShaderRegister = 0;
	rootParams[0].Constants.Num32BitValues = 1;

	D3D12_ROOT_SIGNATURE_DESC desc = {};
	desc.NumParameters = _countof(rootParams);
	desc.pParameters = rootParams;
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_NONE;

	ID3DBlob* pSigBlob;
	ID3DBlob* pErrorBlob;
	HRESULT hr = D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &pSigBlob, &pErrorBlob);
	if (FAILED(hr))
	{
		//std::string msg = convertBlobToString(pErrorBlob.GetInterfacePtr());
		//logError(msg);
		return nullptr;
	}
	ID3D12RootSignature* pRootSig;
	Base::Dx12Device->CreateRootSignature(0, pSigBlob->GetBufferPointer(), pSigBlob->GetBufferSize(), IID_PPV_ARGS(&pRootSig));

	return pRootSig;
}

int CreateRaytracingPipelineState()
{
	D3D12_STATE_SUBOBJECT soMem[100]{};
	UINT numSubobjects = 0;
	auto nextSubobject = [&]()
	{
		return soMem + numSubobjects++;
	};

	ShaderCompiler dxilCompiler;
	dxilCompiler.init();

	ShaderCompilationDesc shaderDesc;

	//D3DCOMPILE_IEEE_STRICTNESS
	shaderDesc.CompileArguments.push_back(L"/Gis");

	//Vertex shader
	shaderDesc.FilePath = L"RayTracingShaders.hlsl";
	shaderDesc.EntryPoint = L"";
	shaderDesc.TargetProfile = L"lib_6_3";

	IDxcBlob* pShaders = nullptr;
	dxilCompiler.compileFromFile(&shaderDesc, &pShaders);

	//Init DXIL subobject
	D3D12_EXPORT_DESC dxilExports[] = {
		sRayGen, nullptr, D3D12_EXPORT_FLAG_NONE,
		sMiss, nullptr, D3D12_EXPORT_FLAG_NONE,
		sClosestHitMirror, nullptr, D3D12_EXPORT_FLAG_NONE,
		sClosestHitEdges, nullptr, D3D12_EXPORT_FLAG_NONE,
	};
	D3D12_DXIL_LIBRARY_DESC dxilLibraryDesc;
	dxilLibraryDesc.DXILLibrary.pShaderBytecode = pShaders->GetBufferPointer();
	dxilLibraryDesc.DXILLibrary.BytecodeLength = pShaders->GetBufferSize();
	dxilLibraryDesc.pExports = dxilExports;
	dxilLibraryDesc.NumExports = _countof(dxilExports);

	D3D12_STATE_SUBOBJECT* soDXIL = nextSubobject();
	soDXIL->Type = D3D12_STATE_SUBOBJECT_TYPE_DXIL_LIBRARY;
	soDXIL->pDesc = &dxilLibraryDesc;


	//Init hit group Mirror
	D3D12_HIT_GROUP_DESC hitGroupDescMirror;
	hitGroupDescMirror.AnyHitShaderImport = nullptr;
	hitGroupDescMirror.ClosestHitShaderImport = sClosestHitMirror;
	hitGroupDescMirror.HitGroupExport = sHitGroupMirror;
	hitGroupDescMirror.IntersectionShaderImport = nullptr;
	hitGroupDescMirror.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;

	D3D12_STATE_SUBOBJECT* soHitGroupMirror = nextSubobject();
	soHitGroupMirror->Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
	soHitGroupMirror->pDesc = &hitGroupDescMirror;

	//Init hit group Edges
	D3D12_HIT_GROUP_DESC hitGroupDescEdges;
	hitGroupDescEdges.AnyHitShaderImport = nullptr;
	hitGroupDescEdges.ClosestHitShaderImport = sClosestHitEdges;
	hitGroupDescEdges.HitGroupExport = sHitGroupEdges;
	hitGroupDescEdges.IntersectionShaderImport = nullptr;
	hitGroupDescEdges.Type = D3D12_HIT_GROUP_TYPE_TRIANGLES;

	D3D12_STATE_SUBOBJECT* soHitGroupEdges = nextSubobject();
	soHitGroupEdges->Type = D3D12_STATE_SUBOBJECT_TYPE_HIT_GROUP;
	soHitGroupEdges->pDesc = &hitGroupDescEdges;

	//Init rayGen local root signature
	ID3D12RootSignature* rayGenLocalRoot = createRayGenLocalRootSignature();
	D3D12_STATE_SUBOBJECT* soRayGenLocalRoot = nextSubobject();
	soRayGenLocalRoot->Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
	soRayGenLocalRoot->pDesc = &rayGenLocalRoot;

	//Bind local root signature to rayGen shader
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION rayGenLocalRootAssociation;
	LPCWSTR rayGenLocalRootAssociationShaderNames[] = { sRayGen };
	rayGenLocalRootAssociation.pExports = rayGenLocalRootAssociationShaderNames;
	rayGenLocalRootAssociation.NumExports = _countof(rayGenLocalRootAssociationShaderNames);
	rayGenLocalRootAssociation.pSubobjectToAssociate = soRayGenLocalRoot; //<-- address to local root subobject

	D3D12_STATE_SUBOBJECT* soRayGenLocalRootAssociation = nextSubobject();
	soRayGenLocalRootAssociation->Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
	soRayGenLocalRootAssociation->pDesc = &rayGenLocalRootAssociation;


	//Init hit group local root signature
	ID3D12RootSignature* hitGroupLocalRoot = createHitGroupLocalRootSignature();
	D3D12_STATE_SUBOBJECT* soHitGroupLocalRoot = nextSubobject();
	soHitGroupLocalRoot->Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
	soHitGroupLocalRoot->pDesc = &hitGroupLocalRoot;


	//Bind local root signature to hit group shaders
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION hitGroupLocalRootAssociation;
	LPCWSTR hitGroupLocalRootAssociationShaderNames[] = { sClosestHitMirror, sClosestHitEdges };
	hitGroupLocalRootAssociation.pExports = hitGroupLocalRootAssociationShaderNames;
	hitGroupLocalRootAssociation.NumExports = _countof(hitGroupLocalRootAssociationShaderNames);
	hitGroupLocalRootAssociation.pSubobjectToAssociate = soHitGroupLocalRoot; //<-- address to local root subobject

	D3D12_STATE_SUBOBJECT* soHitGroupLocalRootAssociation = nextSubobject();
	soHitGroupLocalRootAssociation->Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
	soHitGroupLocalRootAssociation->pDesc = &hitGroupLocalRootAssociation;


	//Init miss local root signature
	ID3D12RootSignature* missLocalRoot = createMissLocalRootSignature();
	D3D12_STATE_SUBOBJECT* soMissLocalRoot = nextSubobject();
	soMissLocalRoot->Type = D3D12_STATE_SUBOBJECT_TYPE_LOCAL_ROOT_SIGNATURE;
	soMissLocalRoot->pDesc = &missLocalRoot;


	//Bind local root signature to miss shader
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION missLocalRootAssociation;
	LPCWSTR missLocalRootAssociationShaderNames[] = { sMiss };
	missLocalRootAssociation.pExports = missLocalRootAssociationShaderNames;
	missLocalRootAssociation.NumExports = _countof(missLocalRootAssociationShaderNames);
	missLocalRootAssociation.pSubobjectToAssociate = soMissLocalRoot; //<-- address to local root subobject

	D3D12_STATE_SUBOBJECT* soMissLocalRootAssociation = nextSubobject();
	soMissLocalRootAssociation->Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
	soMissLocalRootAssociation->pDesc = &missLocalRootAssociation;


	//Init shader config
	D3D12_RAYTRACING_SHADER_CONFIG shaderConfig = {};
	shaderConfig.MaxAttributeSizeInBytes = sizeof(float) * 2;
	shaderConfig.MaxPayloadSizeInBytes = sizeof(float) * 3;

	D3D12_STATE_SUBOBJECT* soShaderConfig = nextSubobject();
	soShaderConfig->Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_SHADER_CONFIG;
	soShaderConfig->pDesc = &shaderConfig;

	//Bind the payload size to the programs
	D3D12_SUBOBJECT_TO_EXPORTS_ASSOCIATION shaderConfigAssociation;
	const WCHAR* shaderNamesToConfig[] = { sMiss, sClosestHitMirror, sClosestHitEdges, sRayGen };
	shaderConfigAssociation.pExports = shaderNamesToConfig;
	shaderConfigAssociation.NumExports = _countof(shaderNamesToConfig);
	shaderConfigAssociation.pSubobjectToAssociate = soShaderConfig; //<-- address to shader config subobject

	D3D12_STATE_SUBOBJECT* soShaderConfigAssociation = nextSubobject();
	soShaderConfigAssociation->Type = D3D12_STATE_SUBOBJECT_TYPE_SUBOBJECT_TO_EXPORTS_ASSOCIATION;
	soShaderConfigAssociation->pDesc = &shaderConfigAssociation;


	//Init pipeline config
	D3D12_RAYTRACING_PIPELINE_CONFIG pipelineConfig;
	pipelineConfig.MaxTraceRecursionDepth = 1;

	D3D12_STATE_SUBOBJECT* soPipelineConfig = nextSubobject();
	soPipelineConfig->Type = D3D12_STATE_SUBOBJECT_TYPE_RAYTRACING_PIPELINE_CONFIG;
	soPipelineConfig->pDesc = &pipelineConfig;



	//Init global root signature
	Base::Resources::DXR::Dx12GlobalRS = createGlobalRootSignature();

	D3D12_STATE_SUBOBJECT* soGlobalRoot = nextSubobject();
	soGlobalRoot->Type = D3D12_STATE_SUBOBJECT_TYPE_GLOBAL_ROOT_SIGNATURE;
	soGlobalRoot->pDesc = &Base::Resources::DXR::Dx12GlobalRS;


	// Create the state
	D3D12_STATE_OBJECT_DESC desc;
	desc.NumSubobjects = numSubobjects;
	desc.pSubobjects = soMem;
	desc.Type = D3D12_STATE_OBJECT_TYPE_RAYTRACING_PIPELINE;

	HRESULT hr = Base::Dx12Device->CreateStateObject(&desc, IID_PPV_ARGS(&Base::States::DXRPipelineState));


	//release local root signatures
	SafeRelease(&rayGenLocalRoot);
	SafeRelease(&hitGroupLocalRoot);
	SafeRelease(&missLocalRoot);

	std::cout << "Ray tracing pipeline state setup done\n";
	return 0;
}


int CreateShaderResources()
{
	D3D12_DESCRIPTOR_HEAP_DESC heapDescriptorDesc = {};
	heapDescriptorDesc.NumDescriptors = 10;
	heapDescriptorDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	heapDescriptorDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	Base::Dx12Device->CreateDescriptorHeap(&heapDescriptorDesc, IID_PPV_ARGS(&Base::Resources::DXR::Dx12RTDescriptorHeap));

	// Create the output resource. The dimensions and format should match the swap-chain
	D3D12_RESOURCE_DESC resDesc = {};
	resDesc.DepthOrArraySize = 1;
	resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // The backbuffer is actually DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, but sRGB formats can't be used with UAVs. We will convert to sRGB ourselves in the shader
	resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
	resDesc.Width = SCREEN_WIDTH;
	resDesc.Height = SCREEN_HEIGHT;
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	resDesc.MipLevels = 1;
	resDesc.SampleDesc.Count = 1;
	Base::Dx12Device->CreateCommittedResource(&defaultHeapProps, D3D12_HEAP_FLAG_NONE, &resDesc, D3D12_RESOURCE_STATE_COPY_SOURCE, nullptr, IID_PPV_ARGS(&Base::Resources::DXR::Dx12OutputResource)); // Starting as copy-source to simplify onFrameRender()

	// Create the UAV. Based on the root signature we created it should be the first entry
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	Base::Resources::DXR::Dx12OutputUAV_CPUHandle = Base::Resources::DXR::Dx12RTDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	Base::Dx12Device->CreateUnorderedAccessView(Base::Resources::DXR::Dx12OutputResource, nullptr, &uavDesc, Base::Resources::DXR::Dx12OutputUAV_CPUHandle);

	// Create the TLAS SRV right after the UAV. Note that we are using a different SRV desc here
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.RaytracingAccelerationStructure.Location = Base::Resources::DXR::TopBuffers.pResult->GetGPUVirtualAddress();

	Base::Resources::DXR::Dx12Accelleration_CPUHandle = Base::Resources::DXR::Dx12RTDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	Base::Resources::DXR::Dx12Accelleration_CPUHandle.ptr += Base::Dx12Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	Base::Dx12Device->CreateShaderResourceView(nullptr, &srvDesc, Base::Resources::DXR::Dx12Accelleration_CPUHandle);

	std::cout << "Shader descriptors setup done\n";
	return 0;
}


int CreateShaderTables()
{
	ID3D12StateObjectProperties* pRtsoProps = nullptr;
	if (SUCCEEDED(Base::States::DXRPipelineState->QueryInterface(IID_PPV_ARGS(&pRtsoProps))))
	{
		//raygen
		{

			struct alignas(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT) RAY_GEN_SHADER_TABLE_DATA
			{
				unsigned char ShaderIdentifier[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES];
				UINT64 RTVDescriptor;
			} tableData;

			memcpy(tableData.ShaderIdentifier, pRtsoProps->GetShaderIdentifier(sRayGen), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
			tableData.RTVDescriptor = Base::Resources::DXR::Dx12RTDescriptorHeap->GetGPUDescriptorHandleForHeapStart().ptr;

			//how big is the biggest?
			union MaxSize
			{
				RAY_GEN_SHADER_TABLE_DATA data0;
			};

			Base::Resources::DXR::Shaders::RayGenShaderTable.StrideInBytes = sizeof(MaxSize);
			Base::Resources::DXR::Shaders::RayGenShaderTable.SizeInBytes = Base::Resources::DXR::Shaders::RayGenShaderTable.StrideInBytes * 1; //<-- only one for now...
			Base::Resources::DXR::Shaders::RayGenShaderTable.Resource = createBuffer(Base::Resources::DXR::Shaders::RayGenShaderTable.SizeInBytes, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, uploadHeapProperties);

			// Map the buffer
			void* pData;
			Base::Resources::DXR::Shaders::RayGenShaderTable.Resource->Map(0, nullptr, &pData);
			memcpy(pData, &tableData, sizeof(tableData));
			Base::Resources::DXR::Shaders::RayGenShaderTable.Resource->Unmap(0, nullptr);
		}

		//miss
		{

			struct alignas(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT) MISS_SHADER_TABLE_DATA
			{
				unsigned char ShaderIdentifier[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES];
			} tableData;

			memcpy(tableData.ShaderIdentifier, pRtsoProps->GetShaderIdentifier(sMiss), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);

			//how big is the biggest?
			union MaxSize
			{
				MISS_SHADER_TABLE_DATA data0;
			};

			Base::Resources::DXR::Shaders::MissShaderTable.StrideInBytes = sizeof(MaxSize);
			Base::Resources::DXR::Shaders::MissShaderTable.SizeInBytes = Base::Resources::DXR::Shaders::MissShaderTable.StrideInBytes * 1; //<-- only one for now...
			Base::Resources::DXR::Shaders::MissShaderTable.Resource = createBuffer(Base::Resources::DXR::Shaders::MissShaderTable.SizeInBytes, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, uploadHeapProperties);

			// Map the buffer
			void* pData;
			Base::Resources::DXR::Shaders::MissShaderTable.Resource->Map(0, nullptr, &pData);
			memcpy(pData, &tableData, sizeof(tableData));
			Base::Resources::DXR::Shaders::MissShaderTable.Resource->Unmap(0, nullptr);
		}

		//hit program
		{

			struct alignas(D3D12_RAYTRACING_SHADER_TABLE_BYTE_ALIGNMENT) HIT_GROUP_SHADER_TABLE_DATA
			{
				unsigned char ShaderIdentifier[D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES];
				float ShaderTableColor[3];
			} tableData[MODEL_PARTS]{};

			const float luminance = 2.0f / 3.0f;

			memcpy(tableData[0].ShaderIdentifier, pRtsoProps->GetShaderIdentifier(sHitGroupMirror), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
			tableData[0].ShaderTableColor[0] = luminance;
			tableData[0].ShaderTableColor[1] = luminance;
			tableData[0].ShaderTableColor[2] = luminance;

			memcpy(tableData[1].ShaderIdentifier, pRtsoProps->GetShaderIdentifier(sHitGroupEdges), D3D12_SHADER_IDENTIFIER_SIZE_IN_BYTES);
			tableData[1].ShaderTableColor[0] = luminance;
			tableData[1].ShaderTableColor[1] = luminance;
			tableData[1].ShaderTableColor[2] = luminance;

			//how big is the biggest?
			union MaxSize
			{
				HIT_GROUP_SHADER_TABLE_DATA data0;
			};

			Base::Resources::DXR::Shaders::HitGroupShaderTable.StrideInBytes = sizeof(MaxSize);
			Base::Resources::DXR::Shaders::HitGroupShaderTable.SizeInBytes = Base::Resources::DXR::Shaders::HitGroupShaderTable.StrideInBytes * MODEL_PARTS;
			Base::Resources::DXR::Shaders::HitGroupShaderTable.Resource = createBuffer(Base::Resources::DXR::Shaders::HitGroupShaderTable.SizeInBytes, D3D12_RESOURCE_FLAG_NONE, D3D12_RESOURCE_STATE_GENERIC_READ, uploadHeapProperties);

			// Map the buffer
			void* pData;
			Base::Resources::DXR::Shaders::HitGroupShaderTable.Resource->Map(0, nullptr, &pData);
			memcpy(pData, &tableData, sizeof(tableData));
			Base::Resources::DXR::Shaders::HitGroupShaderTable.Resource->Unmap(0, nullptr);
		}

		pRtsoProps->Release();
	}

	std::cout << "Shader tables setup done\n";
	return 0;
}

void SetResourceTransitionBarrier(ID3D12GraphicsCommandList* commandList, ID3D12Resource* resource,
	D3D12_RESOURCE_STATES StateBefore, D3D12_RESOURCE_STATES StateAfter)
{
	D3D12_RESOURCE_BARRIER barrierDesc = {};

	barrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
	barrierDesc.Transition.pResource = resource;
	barrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
	barrierDesc.Transition.StateBefore = StateBefore;
	barrierDesc.Transition.StateAfter = StateAfter;

	commandList->ResourceBarrier(1, &barrierDesc);
}


void Update()
{

}

void Render()
{
	UINT backBufferIndex = Base::DxgiSwapChain4->GetCurrentBackBufferIndex();

	//Command list allocators can only be reset when the associated command lists have
	//finished execution on the GPU; fences are used to ensure this (See WaitForGpu method)
	Base::Queues::Compute::Dx12CommandAllocator->Reset();
	Base::Queues::Compute::Dx12CommandList4->Reset(Base::Queues::Compute::Dx12CommandAllocator, nullptr);

	//Set constant buffer descriptor heap
	ID3D12DescriptorHeap* descriptorHeaps[] = { Base::Resources::DXR::Dx12RTDescriptorHeap };
	Base::Queues::Compute::Dx12CommandList4->SetDescriptorHeaps(ARRAYSIZE(descriptorHeaps), descriptorHeaps);

	//hack to update every frame...
	createTopLevelAS(Base::Queues::Compute::Dx12CommandList4);

	// Let's raytrace
	SetResourceTransitionBarrier(Base::Queues::Compute::Dx12CommandList4, Base::Resources::DXR::Dx12OutputResource, D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
	D3D12_DISPATCH_RAYS_DESC raytraceDesc = {};
	raytraceDesc.Width = SCREEN_WIDTH;
	raytraceDesc.Height = SCREEN_HEIGHT;
	raytraceDesc.Depth = 1;

	//set shader tables
	raytraceDesc.RayGenerationShaderRecord.StartAddress = Base::Resources::DXR::Shaders::RayGenShaderTable.Resource->GetGPUVirtualAddress();
	raytraceDesc.RayGenerationShaderRecord.SizeInBytes = Base::Resources::DXR::Shaders::RayGenShaderTable.SizeInBytes;

	raytraceDesc.MissShaderTable.StartAddress = Base::Resources::DXR::Shaders::MissShaderTable.Resource->GetGPUVirtualAddress();
	raytraceDesc.MissShaderTable.StrideInBytes = Base::Resources::DXR::Shaders::MissShaderTable.StrideInBytes;
	raytraceDesc.MissShaderTable.SizeInBytes = Base::Resources::DXR::Shaders::MissShaderTable.SizeInBytes;

	raytraceDesc.HitGroupTable.StartAddress = Base::Resources::DXR::Shaders::HitGroupShaderTable.Resource->GetGPUVirtualAddress();
	raytraceDesc.HitGroupTable.StrideInBytes = Base::Resources::DXR::Shaders::HitGroupShaderTable.StrideInBytes;
	raytraceDesc.HitGroupTable.SizeInBytes = Base::Resources::DXR::Shaders::HitGroupShaderTable.SizeInBytes;

	// Bind the empty root signature
	Base::Queues::Compute::Dx12CommandList4->SetComputeRootSignature(Base::Resources::DXR::Dx12GlobalRS);

	// Set float RedChannel; in global root signature
	float redColor = 1.0f;
	Base::Queues::Compute::Dx12CommandList4->SetComputeRoot32BitConstant(0, *reinterpret_cast<UINT*>(&redColor), 0);

	// Dispatch
	Base::Queues::Compute::Dx12CommandList4->SetPipelineState1(Base::States::DXRPipelineState);
	Base::Queues::Compute::Dx12CommandList4->DispatchRays(&raytraceDesc);

	//Close the list to prepare it for execution.
	Base::Queues::Compute::Dx12CommandList4->Close();


	Base::Queues::Direct::Dx12CommandAllocator->Reset();
	Base::Queues::Direct::Dx12CommandList4->Reset(Base::Queues::Direct::Dx12CommandAllocator, nullptr);

	// Copy the results to the back-buffer
	SetResourceTransitionBarrier(Base::Queues::Direct::Dx12CommandList4, Base::Resources::DXR::Dx12OutputResource, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_COPY_SOURCE);
	SetResourceTransitionBarrier(Base::Queues::Direct::Dx12CommandList4, Base::Resources::Backbuffers::Dx12RTVResources[backBufferIndex], D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST);
	Base::Queues::Direct::Dx12CommandList4->CopyResource(Base::Resources::Backbuffers::Dx12RTVResources[backBufferIndex], Base::Resources::DXR::Dx12OutputResource);


	//Indicate that the back buffer will now be used to present.
	SetResourceTransitionBarrier(Base::Queues::Direct::Dx12CommandList4, Base::Resources::Backbuffers::Dx12RTVResources[backBufferIndex], D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT);

	//Close the list to prepare it for execution.
	Base::Queues::Direct::Dx12CommandList4->Close();

	{
		//Execute the command list.
		ID3D12CommandList* listsToExecute[] = { Base::Queues::Compute::Dx12CommandList4 };
		Base::Queues::Compute::Dx12Queue->ExecuteCommandLists(ARRAYSIZE(listsToExecute), listsToExecute);
	}

	WaitForCompute();

	{
		//Execute the command list.
		ID3D12CommandList* listsToExecute[] = { Base::Queues::Direct::Dx12CommandList4 };
		Base::Queues::Direct::Dx12Queue->ExecuteCommandLists(ARRAYSIZE(listsToExecute), listsToExecute);
	}

	//Present the frame.
	DXGI_PRESENT_PARAMETERS pp = {};
	Base::DxgiSwapChain4->Present1(0, 0, &pp);

	WaitForDirect(); //Wait for GPU to finish.
	//NOT BEST PRACTICE, only used as such for simplicity.
}
