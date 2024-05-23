#include <windows.h>

#include "GenericIncludes.h"
#include "Settings.h"
#include "WindowsHelper.h"
#include "DX12Base.h"
#include "SceneObject.h"


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
				std::cerr << "Failed DX12 and raytracing setup, exiting application\n";
				break;
			}

			WaitForCompute();
			WaitForDirect();

			ShowWindow(wndHandle, nCmdShow);

			std::thread computeLoop(ComputeLoop);
			std::thread directLoop(DirectLoop);
			while (WM_QUIT != msg.message)
			{
				if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}

			TerminateLoops();
			computeLoop.join();
			directLoop.join();
		}
	} while (false);

	//Wait for GPU execution to be done and then release all interfaces.
	WaitForCompute();
	WaitForDirect();

	DX12Free();

#ifdef _DEBUG
	system("pause");
#endif

	return (int)msg.wParam;
}

