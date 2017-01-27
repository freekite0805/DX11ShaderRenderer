#include "Renderer.h"
#include <xnamath.h>
#include <windowsx.h>
#include <fstream>

#define IDI_ICON 101

struct ToyShaderBuffer {
	XMFLOAT2 resolution;    // viewport resolution (in pixels)
	float time;				// shader playback time (in seconds)
	float aspect;			// cached aspect of viewport
	XMFLOAT4 mouse;         // mouse pixel coords. xy: current (if MLB down), zw: click
}gToyBuffer;

namespace PingPong
{
	ID3D11Texture2D*                TEXs[2];
	ID3D11ShaderResourceView*       SRVs[2];
	ID3D11RenderTargetView*         RTVs[2];
	int frontBufferIdx = 0;
	int backBufferIdx = 1;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	int mouseX = GET_X_LPARAM(lParam);
	int mouseY = GET_Y_LPARAM(lParam);

	bool isShaderModified = false;

	switch (message)
	{
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		EndPaint(hWnd, &ps);
		break;

	case WM_KEYUP:
		switch (wParam)
		{
		case VK_ESCAPE:
			PostQuitMessage(0);
			break;
		}
		break;

	case WM_MOUSEMOVE:
		gToyBuffer.mouse.x = (float)mouseX;
		gToyBuffer.mouse.y = (float)mouseY;
		break;

	case WM_DESTROY:
		PostQuitMessage(0);
		break;

	default:
		return DefWindowProc(hWnd, message, wParam, lParam);
	}
	return 0;
}

RendererD3D11::RendererD3D11()
{
}

RendererD3D11::~RendererD3D11()
{
	DestroyDevice();
	DestroyWindow();
}

bool RendererD3D11::SetupWindow(HINSTANCE hInstance, int nCmdShow)
{
	WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	wcex.lpfnWndProc = WndProc;
	wcex.hInstance = hInstance;
	wcex.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON));
	wcex.hCursor = LoadCursor(NULL, IDC_CROSS);
	wcex.lpszClassName = kAppName;
	if (!RegisterClassEx(&wcex))
		return false;

	m_HInst = hInstance;
	RECT rc = { 0,0,kAppWidth,kAppHeight };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, false);
	m_HWnd = CreateWindow(kAppName, kAppName, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		rc.right - rc.left, rc.bottom - rc.top,
		NULL, NULL, hInstance,
		NULL);
	if (!m_HWnd)
		return false;

	ShowWindow(m_HWnd, nCmdShow);
	return true;
}

bool RendererD3D11::SetupDevice(const char* vsFileName, const char* psFileName)
{
	RECT rc;
	GetClientRect(m_HWnd, &rc);
	unsigned width = rc.right - rc.left;
	unsigned height = rc.bottom - rc.top;

	m_Viewport = CD3D11_VIEWPORT(0.0f, 0.0f, (float)width, (float)height);

	gToyBuffer.resolution.x = static_cast<float>(width);
	gToyBuffer.resolution.y = static_cast<float>(height);
	gToyBuffer.aspect = (float)width / (float)height;

	unsigned createDeviceFlags = 0;
#ifdef _DEBUG
	createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

	DXGI_SWAP_CHAIN_DESC sd;
	ZeroMemory(&sd, sizeof(sd));
	sd.BufferCount = 1;
	sd.BufferDesc.Width = width;
	sd.BufferDesc.Height = height;
	sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	sd.BufferDesc.RefreshRate.Numerator = 60;
	sd.BufferDesc.RefreshRate.Denominator = 1;
	sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	sd.OutputWindow = m_HWnd;
	sd.SampleDesc.Count = 1;
	sd.SampleDesc.Quality = 0;
	sd.Windowed = TRUE;

	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
	if (FAILED(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, createDeviceFlags,
		&featureLevel, 1, D3D11_SDK_VERSION, &sd, &m_SwapChain, &m_Device, NULL, &m_Context)))
		return false;

	if (FAILED(m_SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&m_BackBuffer)))
		return false;

	if (FAILED(m_Device->CreateRenderTargetView(m_BackBuffer, NULL, &m_RenderTargetView)))
		return false;

	CD3D11_TEXTURE2D_DESC texDesc(sd.BufferDesc.Format,
		sd.BufferDesc.Width, sd.BufferDesc.Height,
		1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

	// Create PingPong RTV + SRV
	for (int i = 0; i < 2; i++)
	{
		m_Device->CreateTexture2D(&texDesc, NULL, &PingPong::TEXs[i]);
		m_Device->CreateRenderTargetView(PingPong::TEXs[i], NULL, &PingPong::RTVs[i]);
		m_Device->CreateShaderResourceView(PingPong::TEXs[i], NULL, &PingPong::SRVs[i]);
		((ID3D11Texture2D*)PingPong::TEXs[i])->Release();
	}

	ID3DBlob* pVSBlob = NULL;

	CD3D11_BUFFER_DESC desc(sizeof(ToyShaderBuffer), D3D11_BIND_CONSTANT_BUFFER);
	if (FAILED(m_Device->CreateBuffer(&desc, NULL, &m_ToyBuffer)))
		return false;

	if (!LoadCompiledShadersFromDisk(vsFileName, psFileName))
		return false;

	CD3D11_SAMPLER_DESC sampDesc(D3D11_DEFAULT);
	sampDesc.AddressU = sampDesc.AddressV = sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	if (FAILED(m_Device->CreateSamplerState(&sampDesc, &m_SamplerSmooth)))
		return false;

	m_Context->RSSetViewports(1, &m_Viewport);
	m_Context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	return true;

}

void RendererD3D11::DestroyDevice()
{
	if (m_Context)
		m_Context->ClearState();
}

void RendererD3D11::DestroyWindow()
{
	if (m_HWnd)
		::DestroyWindow(m_HWnd);
	UnregisterClass(kAppName, m_HInst);
}

void RendererD3D11::Render()
{
	static DWORD frameStart = GetTickCount();
	gToyBuffer.time = (GetTickCount() - frameStart) / 1000.0f;

	const float clearColour[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
	m_Context->ClearRenderTargetView(PingPong::RTVs[PingPong::frontBufferIdx], clearColour);

	ID3D11RenderTargetView* pRTVs[] = { PingPong::RTVs[PingPong::frontBufferIdx] };
	m_Context->OMSetRenderTargets(_countof(pRTVs), pRTVs, NULL);

	m_Context->UpdateSubresource(m_ToyBuffer, 0, NULL, &gToyBuffer, 0, 0);

	ID3D11Buffer* pBuffers[] = { m_ToyBuffer };
	m_Context->PSSetConstantBuffers(0, _countof(pBuffers), pBuffers);

	ID3D11ShaderResourceView* pSRVs[] = { PingPong::SRVs[PingPong::backBufferIdx] };
	m_Context->PSSetShaderResources(0, _countof(pSRVs), pSRVs);

	ID3D11SamplerState* pSamplers[] = { m_SamplerSmooth };
	m_Context->PSSetSamplers(0, _countof(pSamplers), pSamplers);

	m_Context->Draw(3, 0);

	ID3D11ShaderResourceView* pZeroSRVs[8] = { NULL };
	m_Context->PSSetShaderResources(1, _countof(pZeroSRVs), pZeroSRVs);

	m_Context->CopyResource(m_BackBuffer, PingPong::TEXs[PingPong::frontBufferIdx]);

	m_SwapChain->Present(0, 0);

	std::swap(PingPong::frontBufferIdx, PingPong::backBufferIdx);
}

bool RendererD3D11::LoadCompiledShadersFromDisk(const char* vsFileName, const char* psFileName)
{
	size_t size = 0;
	char* buffer;
	if (!ReadFileToBytes(vsFileName, &buffer, &size))
		return false;

	HRESULT res = m_Device->CreateVertexShader(buffer, size, NULL, &m_ToyVS);
	free(buffer);
	size = 0;

	if (FAILED(res))
		return false;

	if (!ReadFileToBytes(psFileName, &buffer, &size))
		return false;

	res = m_Device->CreatePixelShader(buffer, size, NULL, &m_ToyPS);
	free(buffer);
	size = 0;

	if (FAILED(res))
		return false;

	m_Context->VSSetShader(m_ToyVS, NULL, 0);
	m_Context->PSSetShader(m_ToyPS, NULL, 0);

	return true;
}

bool RendererD3D11::ReadFileToBytes(const char* fileName, char** bufferOut, size_t* bufLenOut)
{
	std::ifstream stream;
	stream.open(fileName, std::ifstream::in | std::ifstream::binary);
	if (!stream.good())
		return false;

	stream.seekg(0, std::ios::end);
	*bufLenOut = size_t(stream.tellg());
	*bufferOut = new char[*bufLenOut];
	stream.seekg(0, std::ios::beg);
	stream.read(bufferOut[0], *bufLenOut);
	stream.close();

	if (*bufferOut == NULL)
		return false;

	return true;
}
