#pragma once
#include <windows.h>

void WaitForCompute();
void WaitForDirect();

int DX12Setup(HWND wndHandle);

void DX12Free();

void ComputeLoop();

void DirectLoop();

void TerminateLoops();
