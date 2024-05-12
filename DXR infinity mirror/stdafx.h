#pragma once

#pragma warning(disable : 4099)

#ifdef NTDDI_VERSION
#undef NTDDI_VERSION
#endif

#define NTDDI_VERSION NTDDI_WIN10_TH2

#include <windows.h>
#include <Windowsx.h>
#include <Commctrl.h>

#include <d3d11_4.h>
#include <d2d1_3.h>
#include <d3d12.h>
#include <d3d11on12.h>
#include <dxgi1_6.h>
#include <dxgidebug.h>

#include <d3dCompiler.h>

#include <DirectXMath.h>
using namespace DirectX;

#include <tchar.h>
#include <sstream>
#include <string>
#include <array>
#include <vector>
#include <map>
#include <fstream>
#include <algorithm>
#include <iomanip>

#define align_to(_alignment, _val) (((_val + _alignment - 1) / _alignment) * _alignment)

// Safe release for interfaces
template<class Interface>
inline void SafeRelease(Interface *& pInterfaceToRelease)
{
	if (pInterfaceToRelease != nullptr)
	{
		pInterfaceToRelease->Release();

		pInterfaceToRelease = nullptr;
	}
}

template<class Interface>
inline void SafeDelete(Interface *& pInterfaceToDelete)
{
	if (pInterfaceToDelete != nullptr)
	{
		delete pInterfaceToDelete;
		pInterfaceToDelete = nullptr;
	}
}

#define PI (3.14159265358979323846f)

#pragma comment (lib, "d3d12.lib")
#pragma comment (lib, "DXGI.lib")

#pragma comment(lib, "d3d11.lib")
#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dxguid.lib")
#pragma comment(lib, "d3dcompiler.lib")

#pragma comment(lib, "Comctl32.lib")
//#pragma comment (lib,"dxerr.lib")

//////////////////////////////////////////////////////////////////////////
// to find memory leaks
//////////////////////////////////////////////////////////////////////////
#define _CRTDBG_MAP_ALLOC
#include <stdlib.h>
#include <crtdbg.h>

#ifdef _DEBUG
#define myMalloc(s)       _malloc_dbg(s, _NORMAL_BLOCK, __FILE__, __LINE__)
#define myCalloc(c, s)    _calloc_dbg(c, s, _NORMAL_BLOCK, __FILE__, __LINE__)
#define myRealloc(p, s)   _realloc_dbg(p, s, _NORMAL_BLOCK, __FILE__, __LINE__)
#define myExpand(p, s)    _expand_dbg(p, s, _NORMAL_BLOCK, __FILE__, __LINE__)
#define myFree(p)         _free_dbg(p, _NORMAL_BLOCK)
#define myMemSize(p)      _msize_dbg(p, _NORMAL_BLOCK)
#define myNew new(_NORMAL_BLOCK, __FILE__, __LINE__)
#define myDelete delete  // Set to dump leaks at the program exit.
#define myInitMemoryCheck() \
	_CrtSetDbgFlag( _CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF)
#define myDumpMemoryLeaks() \
	_CrtDumpMemoryLeaks()
#else
#define myMalloc malloc
#define myCalloc calloc
#define myRealloc realloc
#define myExpand _expand
#define myFree free
#define myMemSize _msize
#define myNew new
#define myDelete delete
#define myInitMemoryCheck()
#define myDumpMemoryLeaks()
#endif 
//////////////////////////////////////////////////////////////////////////

#if defined(DEBUG) | defined(_DEBUG)
#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define LINE_STRING STRINGIZE(__LINE__)

#define HR_INTERNAL(x, y, z)									\
	{													\
		HRESULT hr = (x);								\
		if (FAILED(hr))									\
		{												\
			cleanup();									\
														\
			MessageBoxA(								\
				0,										\
				#x " failed in\n" z ", line: " ## y,	\
				"FAILED",								\
				0);										\
			return hr;									\
		}												\
	}

#define HR(x) HR_INTERNAL(x, LINE_STRING, __FILE__)
#else
#define HR(x) (x);
#endif

/*
#if defined(DEBUG) | defined(_DEBUG)
#ifndef HR
#define HR(x)														\
	{																\
		HRESULT hr = (x);											\
		if (FAILED(hr))												\
		{															\
			LPWSTR output;                                    		\
			FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM |				\
			FORMAT_MESSAGE_IGNORE_INSERTS |							\
			FORMAT_MESSAGE_ALLOCATE_BUFFER,							\
			NULL,													\
			hr,														\
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),				\
			(LPTSTR)&output,										\
			0,														\
			NULL);													\
			MessageBox(NULL, output, L"Error", MB_OK);				\
		}															\
	}
#endif
#else
#ifndef HR
#define HR(x) (x)
#endif
#endif 


#endif
*/