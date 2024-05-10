#include "DX12Base.h"

#include <d3d12.h>
#include <dxgi1_6.h>
#pragma comment (lib, "d3d12.lib")
#pragma comment (lib, "DXGI.lib")

#include "Settings.h"
#include "GenericIncludes.h"
#include "WindowsHelper.h"

namespace Base
{
	ID3D12Device* Dx12Device;
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

	}
}

template<class Interface>
inline void SafeRelease(Interface** ppInterfaceToRelease)
{
	if (*ppInterfaceToRelease != NULL)
	{
		(*ppInterfaceToRelease)->Release();

		(*ppInterfaceToRelease) = NULL;
	}
}

#define NameInterface(dxInterface) dxInterface->SetName(L#dxInterface)

#define NameInterfaceIndex(dxInterface, i) dxInterface->SetName(L#dxInterface L" Index:" L#i)

int CreateDirect3DDevice();
int CreateCommandInterfaces();
int CreateSwapChain(HWND wndHandle);
int CreateFenceAndEventHandle();
int CreateRenderTargets();

int DX12Setup(HWND wndHandle)
{
	if (CreateDirect3DDevice() != 0) return 1;

	if (CreateCommandInterfaces() != 0) return 1;

	if (CreateSwapChain(wndHandle) != 0) return 1;

	if (CreateFenceAndEventHandle() != 0) return 1;

	//if (CreateRenderTargets()) return 1; //Not needed for pure raytracing
	
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
