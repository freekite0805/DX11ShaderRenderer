#pragma once

#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d11.lib")

#include <dxgi.h>
#include <d3dcommon.h>
#include <d3d11.h>
#include <d3dx10math.h>


const char kAppName[] = "Clouds-App";
const int kAppWidth = 640;
const int kAppHeight = 480;

class RendererD3D11
{
public:
	RendererD3D11();
	~RendererD3D11();

	bool SetupWindow(HINSTANCE hInstance, int nCmdShow);
	bool SetupDevice(const char* vsFileName, const char* psFileName);
	void DestroyDevice();
	void DestroyWindow();
	void Render();

private:
	HINSTANCE m_HInst;
	HWND m_HWnd;
	ID3D11Device* m_Device;
	ID3D11DeviceContext* m_Context;
	IDXGISwapChain* m_SwapChain;

	D3D11_VIEWPORT m_Viewport;

	ID3D11Texture2D* m_BackBuffer;
	ID3D11RenderTargetView* m_RenderTargetView;
	ID3D11Buffer* m_ToyBuffer;
	ID3D11SamplerState* m_SamplerSmooth;
	ID3D11VertexShader* m_ToyVS;
	ID3D11PixelShader* m_ToyPS;

	bool LoadCompiledShadersFromDisk(const char* vsFileName, const char* psFileName);
	bool ReadFileToBytes(const char* fileName, char** bufferOut, size_t* bufLenOut);
};