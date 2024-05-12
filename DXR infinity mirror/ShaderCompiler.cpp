#include "ShaderCompiler.h"

ShaderCompiler::ShaderCompiler()
{

}

ShaderCompiler::~ShaderCompiler()
{
	SafeRelease(compiler_);
	SafeRelease(library_);
	SafeRelease(includeHandler_);
	SafeRelease(linker_);
}

HRESULT ShaderCompiler::init()
{
#ifdef _WIN64
	HMODULE dll = LoadLibraryA("dxcompiler_x64.dll");
#else
	HMODULE dll = LoadLibraryA("dxcompiler_x86.dll");
#endif

	DxcCreateInstanceProc pfnDxcCreateInstance = DxcCreateInstanceProc(GetProcAddress(dll, "DxcCreateInstance"));
	HRESULT hr = E_FAIL;

	if (SUCCEEDED(hr = pfnDxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&compiler_))))
	{
		if (SUCCEEDED(hr = pfnDxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&library_))))
		{
			if (SUCCEEDED(library_->CreateIncludeHandler(&includeHandler_)))
			{
				if (SUCCEEDED(hr = pfnDxcCreateInstance(CLSID_DxcLinker, IID_PPV_ARGS(&linker_))))
				{

				}
			}
		}
	}

	return hr;
}

HRESULT ShaderCompiler::compileFromFile(ShaderCompilationDesc* desc, IDxcBlob** ppResult)
{
	HRESULT hr = E_FAIL;

	if (desc)
	{
		IDxcBlobEncoding* source = nullptr;
		if (SUCCEEDED(hr = library_->CreateBlobFromFile(desc->FilePath, nullptr, &source)))
		{
			IDxcOperationResult* pResult = nullptr;
			if (SUCCEEDED(hr = compiler_->Compile(
				source,									// program text
				desc->FilePath,							// file name, mostly for error messages
				desc->EntryPoint,						// entry point function
				desc->TargetProfile,					// target profile
				desc->CompileArguments.data(),          // compilation arguments
				(UINT)desc->CompileArguments.size(),	// number of compilation arguments
				desc->Defines.data(),					// define arguments
				(UINT)desc->Defines.size(),				// number of define arguments
				includeHandler_,						// handler for #include directives
				&pResult)))
			{
				HRESULT hrCompile = E_FAIL;
				if (SUCCEEDED(hr = pResult->GetStatus(&hrCompile)))
				{
					if (SUCCEEDED(hrCompile))
					{
						if (ppResult)
						{
							pResult->GetResult(ppResult);
							hr = S_OK;
						}
						else
						{
							hr = E_FAIL;
						}
					}
					else
					{
						IDxcBlobEncoding* pPrintBlob = nullptr;
						if (SUCCEEDED(pResult->GetErrorBuffer(&pPrintBlob)))
						{
							// We can use the library to get our preferred encoding.
							IDxcBlobEncoding* pPrintBlob16 = nullptr;
							library_->GetBlobAsUtf16(pPrintBlob, &pPrintBlob16);

							MessageBox(0, (LPCWSTR)pPrintBlob16->GetBufferPointer(), L"", 0);

							SafeRelease(pPrintBlob);
							SafeRelease(pPrintBlob16);
						}
					}

					SafeRelease(pResult);
				}
			}
		}
	}

	return hr;
}