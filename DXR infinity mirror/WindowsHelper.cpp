#include "WindowsHelper.h"

#include "Settings.h"

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}

	return DefWindowProc(hWnd, message, wParam, lParam);
}

HWND InitWindow(HINSTANCE hInstance)
{
	WNDCLASSEX wcex = { 0 };
	wcex.cbSize = sizeof(WNDCLASSEX);
	wcex.lpfnWndProc = WndProc;
	wcex.hInstance = hInstance;
	wcex.lpszClassName = APPLICATION_NAME;
	if (!RegisterClassEx(&wcex))
	{
		return false;
	}

	RECT rc2 = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
	AdjustWindowRect(&rc2, WS_OVERLAPPEDWINDOW, false);

	DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
	DWORD exStyle = WS_EX_OVERLAPPEDWINDOW;

	RECT rc = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
	AdjustWindowRectEx(&rc, style, FALSE, exStyle);

	HWND hwnd = CreateWindowEx(
		exStyle,
		APPLICATION_NAME,
		APPLICATION_NAME,
		style,
		CW_USEDEFAULT,
		CW_USEDEFAULT,
		rc.right - rc.left,
		rc.bottom - rc.top,
		nullptr,
		nullptr,
		hInstance,
		nullptr);

	if (hwnd)
	{
		POINT position{ 25, 0 };
		RECT rc = { 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT };
		AdjustWindowRectEx(&rc, style, FALSE, exStyle);
		MoveWindow(
			hwnd,
			position.x, position.y,
			rc.right - rc.left,
			rc.bottom - rc.top,
			true
		);

		GetClientRect(hwnd, &rc);
		rc = rc;
	}

	return hwnd;
}
