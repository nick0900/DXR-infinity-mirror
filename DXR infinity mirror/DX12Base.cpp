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
			AccelerationStructureBuffers BottomBuffers;

			uint64_t ConservativeTopSize;
			AccelerationStructureBuffers TopBuffers;
		}
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

#define NameInterface(dxInterface) dxInterface->SetName(L#dxInterface)

#define NameInterfaceIndex(dxInterface, i) dxInterface->SetName(L#dxInterface L" Index:" L#i)

int CreateDirect3DDevice();
int CreateCommandInterfaces();
int CreateSwapChain(HWND wndHandle);
int CreateFenceAndEventHandle();
int CreateRenderTargets();
int CreateAccelerationStructures();

int DX12Setup(HWND wndHandle)
{
	if (CreateDirect3DDevice() != 0) return 1;

	if (CreateCommandInterfaces() != 0) return 1;

	if (CreateSwapChain(wndHandle) != 0) return 1;

	if (CreateFenceAndEventHandle() != 0) return 1;

	//if (CreateRenderTargets()) return 1; //Not needed for pure raytracing

	if (CreateAccelerationStructures() != 0) return 1;
	//		CreateRaytracingPipelineState();
	//		CreateShaderResources();

			//last
	//		CreateShaderTables();
	
	return 0;
}

void DX12Free()
{
	//SafeRelease(&mpOutputResource);
	//SafeRelease(&gRTDescriptorHeap);
	//SafeRelease(&gRTPipelineState);
	//SafeRelease(&gGlobalRootSig);


	SafeRelease(&Base::Resources::Backbuffers::Dx12RTVDescriptorHeap);
	for (int i = 0; i < NUM_SWAP_BUFFERS; i++)
	{
		SafeRelease(&Base::Resources::Backbuffers::Dx12RTVResources[i]);
	}
	
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

		cqd.Type = D3D12_COMMAND_LIST_TYPE_COPY;
		if (FAILED(Base::Dx12Device->CreateCommandQueue(&cqd, IID_PPV_ARGS(&Base::Queues::Compute::Dx12Queue)))) break;
		NameInterface(Base::Queues::Compute::Dx12Queue);

		//Create command allocator. The command allocator object corresponds
		//to the underlying allocations in which GPU commands are stored.
		if (FAILED(Base::Dx12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Base::Queues::Direct::Dx12CommandAllocator)))) break;
		NameInterface(Base::Queues::Direct::Dx12CommandAllocator);

		if (FAILED(Base::Dx12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Base::Queues::Compute::Dx12CommandAllocator)))) break;
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
			D3D12_COMMAND_LIST_TYPE_DIRECT,
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

void createBottomLevelAS(ID3D12GraphicsCommandList4* pCmdList, D3D12_RAYTRACING_GEOMETRY_DESC* geomDesc, uint32_t numDescs)
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

	Base::Resources::DXR::BottomBuffers.pScratch = createBuffer(info.ScratchDataSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_UNORDERED_ACCESS, defaultHeapProps);
	Base::Resources::DXR::BottomBuffers.pResult = createBuffer(info.ResultDataMaxSizeInBytes, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE, defaultHeapProps);

	// Create the bottom-level AS
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_DESC asDesc = {};
	asDesc.Inputs = inputs;
	asDesc.DestAccelerationStructureData = Base::Resources::DXR::BottomBuffers.pResult->GetGPUVirtualAddress();
	asDesc.ScratchAccelerationStructureData = Base::Resources::DXR::BottomBuffers.pScratch->GetGPUVirtualAddress();

	pCmdList->BuildRaytracingAccelerationStructure(&asDesc, 0, nullptr);

	// We need to insert a UAV barrier before using the acceleration structures in a raytracing operation
	D3D12_RESOURCE_BARRIER uavBarrier = {};
	uavBarrier.Type = D3D12_RESOURCE_BARRIER_TYPE_UAV;
	uavBarrier.UAV.pResource = Base::Resources::DXR::BottomBuffers.pResult;
	pCmdList->ResourceBarrier(1, &uavBarrier);
}

void createTopLevelAS(ID3D12GraphicsCommandList4* pCmdList, ID3D12Resource1* pBottomLevelAS)
{
	uint32_t instanceCount = 3;

	// First, get the size of the TLAS buffers and create them
	D3D12_BUILD_RAYTRACING_ACCELERATION_STRUCTURE_INPUTS inputs = {};
	inputs.DescsLayout = D3D12_ELEMENTS_LAYOUT_ARRAY;
	inputs.Flags = D3D12_RAYTRACING_ACCELERATION_STRUCTURE_BUILD_FLAG_NONE;
	inputs.NumDescs = instanceCount;
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
			sizeof(D3D12_RAYTRACING_INSTANCE_DESC) * instanceCount,
			D3D12_RESOURCE_FLAG_NONE,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			uploadHeapProperties);
	}

	D3D12_RAYTRACING_INSTANCE_DESC* pInstanceDesc;
	Base::Resources::DXR::TopBuffers.pInstanceDesc->Map(0, nullptr, (void**)&pInstanceDesc);

	static float rotY = 0;
	rotY += 0.001f;
	for (int i = 0; i < instanceCount; i++)
	{
		pInstanceDesc->InstanceID = i;                            // exposed to the shader via InstanceID()
		pInstanceDesc->InstanceContributionToHitGroupIndex = i;   // offset inside the shader-table. we only have a single geometry, so the offset 0
		pInstanceDesc->Flags = D3D12_RAYTRACING_INSTANCE_FLAG_NONE;

		
		//apply transform
		DirectX::XMFLOAT3X4 m;
		DirectX::XMStoreFloat3x4(&m, DirectX::XMMatrixRotationY(i * 0.25f + rotY) * DirectX::XMMatrixTranslation(-2.0f + i * 2, 0, 0));
		memcpy(pInstanceDesc->Transform, &m, sizeof(pInstanceDesc->Transform));

		pInstanceDesc->AccelerationStructure = pBottomLevelAS->GetGPUVirtualAddress();
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
	geomDesc[0].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;

	geomDesc[0].Triangles.VertexBuffer.StartAddress = mpVertexBuffer1->GetGPUVirtualAddress();
	geomDesc[0].Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
	geomDesc[0].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	geomDesc[0].Triangles.VertexCount = infiniMirror.meshGeometries[0].numVertecies;

	geomDesc[0].Triangles.IndexBuffer = mpIndexBuffer1->GetGPUVirtualAddress();
	geomDesc[0].Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
	geomDesc[0].Triangles.IndexCount = infiniMirror.meshGeometries[0].numIndecies;

	geomDesc[0].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

	geomDesc[1].Type = D3D12_RAYTRACING_GEOMETRY_TYPE_TRIANGLES;

	geomDesc[1].Triangles.VertexBuffer.StartAddress = mpVertexBuffer2->GetGPUVirtualAddress();
	geomDesc[1].Triangles.VertexBuffer.StrideInBytes = sizeof(Vertex);
	geomDesc[1].Triangles.VertexFormat = DXGI_FORMAT_R32G32B32_FLOAT;
	geomDesc[1].Triangles.VertexCount = infiniMirror.meshGeometries[1].numVertecies;

	geomDesc[1].Triangles.IndexBuffer = mpIndexBuffer2->GetGPUVirtualAddress();
	geomDesc[1].Triangles.IndexFormat = DXGI_FORMAT_R32_UINT;
	geomDesc[1].Triangles.IndexCount = infiniMirror.meshGeometries[1].numIndecies;

	geomDesc[1].Flags = D3D12_RAYTRACING_GEOMETRY_FLAG_OPAQUE;

	createBottomLevelAS(Base::Queues::Compute::Dx12CommandList4, geomDesc, ARRAYSIZE(geomDesc));
	createTopLevelAS(Base::Queues::Compute::Dx12CommandList4, Base::Resources::DXR::BottomBuffers.pResult);

	Base::Queues::Compute::Dx12CommandList4->Close();
	ID3D12CommandList* listsToExec[] = { Base::Queues::Compute::Dx12CommandList4 };
	Base::Queues::Compute::Dx12Queue->ExecuteCommandLists(_countof(listsToExec), listsToExec);

	WaitForCompute();

	SafeRelease(&mpVertexBuffer1);
	SafeRelease(&mpVertexBuffer2);
	SafeRelease(&mpIndexBuffer1);
	SafeRelease(&mpIndexBuffer2);

	std::cout << "DXR Acceleration Structures setup successful\n"
	return 0;
}
