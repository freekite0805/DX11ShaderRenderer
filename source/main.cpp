#include <windows.h>
#include "rend/Renderer.h"

int APIENTRY WinMain(HINSTANCE hInstance,
	HINSTANCE hPrevInstance,
	LPSTR     lpCmdLine,
	int       nCmdShow)
{
	RendererD3D11 renderer = RendererD3D11();
	if (!renderer.SetupWindow(hInstance, nCmdShow)) {
		return 0;
	}

	if (!renderer.SetupDevice("./bin/shader/Toy.vs.cso", "./bin/shader/Toy.ps.cso")) {
		renderer.DestroyDevice();
		return 0;
	}

	MSG msg = { 0 };
	while (WM_QUIT != msg.message) {
		if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else {
			renderer.Render();
			Sleep(10);
		}
	}

	renderer.DestroyDevice();
	return (int)msg.wParam;
}