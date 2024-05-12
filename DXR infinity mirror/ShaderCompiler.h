#pragma once

#include "stdafx.h"
#include <dxcapi.h>

struct ShaderCompilationDesc
{
	LPCWSTR FilePath;
	LPCWSTR EntryPoint;
	LPCWSTR TargetProfile;
	std::vector<LPCWSTR> CompileArguments;
	std::vector<DxcDefine> Defines;
};

class ShaderCompiler
{
	IDxcLibrary* library_ = nullptr;
	IDxcCompiler* compiler_ = nullptr;
	IDxcLinker* linker_ = nullptr;
	IDxcIncludeHandler* includeHandler_ = nullptr;

public:
	ShaderCompiler();
	~ShaderCompiler();

	HRESULT init();

	HRESULT compileFromFile(ShaderCompilationDesc* desc, IDxcBlob** ppResult);
};