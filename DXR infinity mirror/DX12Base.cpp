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
	
	ID3D12CommandQueue* Dx12MainQueue;
	ID3D12CommandQueue* Dx12CopyQueue;
	ID3D12CommandAllocator* Dx12CommandAllocator;
	ID3D12GraphicsCommandList4* Dx12CommandList4;

	IDXGISwapChain4* DxgiSwapChain4;
	
}

int CreateDirect3DDevice();
int CreateCommandInterfaces();
int CreateSwapChain(HWND wndHandle);

int DX12Setup(HWND wndHandle)
{
	if (CreateDirect3DDevice() != 0) return 1;

	if (CreateCommandInterfaces() != 0) return 1;

	if (CreateSwapChain(wndHandle) != 0) return 1;

	//CreateFenceAndEventHandle();						//4. Create Fence and Event handle

	//CreateRenderTargets();								//5. Create render targets for backbuffer
}

void DX12Free()
{
	//SafeRelease(&gFence);

	//SafeRelease(&mpOutputResource);
	//SafeRelease(&gRTDescriptorHeap);
	//SafeRelease(&gRTPipelineState);
	//SafeRelease(&gGlobalRootSig);


	//SafeRelease(&gRenderTargetsHeap);
	//for (int i = 0; i < NUM_SWAP_BUFFERS; i++)
	//{
	//	SafeRelease(&gRenderTargets[i]);
	//}

	SafeRelease(&Base::DxgiSwapChain4);

	SafeRelease(&Base::Dx12CommandList4);
	SafeRelease(&Base::Dx12CommandAllocator);
	SafeRelease(&Base::Dx12CopyQueue);
	SafeRelease(&Base::Dx12MainQueue);

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
		if (FAILED(Base::Dx12Device->CreateCommandQueue(&cqd, IID_PPV_ARGS(&Base::Dx12MainQueue)))) break;

		cqd.Type = D3D12_COMMAND_LIST_TYPE_COPY;
		if (FAILED(Base::Dx12Device->CreateCommandQueue(&cqd, IID_PPV_ARGS(&Base::Dx12CopyQueue)))) break;

		//Create command allocator. The command allocator object corresponds
		//to the underlying allocations in which GPU commands are stored.
		if (FAILED(Base::Dx12Device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&Base::Dx12CommandAllocator)))) break;

		//Create command list.
		if (FAILED(Base::Dx12Device->CreateCommandList(
			0,
			D3D12_COMMAND_LIST_TYPE_DIRECT,
			Base::Dx12CommandAllocator,
			nullptr,
			IID_PPV_ARGS(&Base::Dx12CommandList4)))) break;

		//Command lists are created in the recording state. Since there is nothing to
		//record right now and the main loop expects it to be closed, we close it.
		Base::Dx12CommandList4->Close();

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
		Base::Dx12MainQueue,
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