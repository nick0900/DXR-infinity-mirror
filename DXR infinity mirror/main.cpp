#include <windows.h>

#include "GenericIncludes.h"
#include "Settings.h"
#include "WindowsHelper.h"
#include "DX12Base.h"


int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
#ifdef _DEBUG
	AllocConsole();
	FILE* pCin;
	FILE* pCout;
	FILE* pCerr;
	freopen_s(&pCin, "conin$", "r", stdin);
	freopen_s(&pCout, "conout$", "w", stdout);
	freopen_s(&pCerr, "conout$", "w", stderr);
	printf("Debugging Window:\n");
#endif

	MSG msg = { 0 };
	HWND wndHandle = InitWindow(hInstance);

	do
	{
		if (wndHandle)
		{
			if (DX12Setup(wndHandle) != 0)
			{
				std::cerr << "Failed setup, exiting application\n";
				break;
			}

			//		CreateAccelerationStructures();
			//		CreateRaytracingPipelineState();
			//		CreateShaderResources();

					//last
			//		CreateShaderTables();


			//		WaitForGpu();

			ShowWindow(wndHandle, nCmdShow);
			while (WM_QUIT != msg.message)
			{
				//			if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
				//			{
				//				TranslateMessage(&msg);
				//				DispatchMessage(&msg);
				//			}
				//			else
				//			{
				//				UINT backBufferIndex = gSwapChain4->GetCurrentBackBufferIndex();

				//				Update(backBufferIndex);
				//				Render(backBufferIndex);
				//			}
			}
		}
	} while (false);

	//Wait for GPU execution to be done and then release all interfaces.
	//WaitForGpu();
	//CloseHandle(gEventHandle);

	DX12Free();

#ifdef _DEBUG
	system("pause");
#endif

	return (int)msg.wParam;
}

