#pragma once
#include <windows.h>
#include "GenericIncludes.h"

void WaitForCompute();
void WaitForDirect();

int DX12Setup(HWND wndHandle);

void DX12Free();

void ComputeLoop();

void DirectLoop();

void TerminateLoops();

bool TestRunning();

void WriteResults(std::string savePath);
