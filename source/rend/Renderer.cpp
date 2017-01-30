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
	ID3D11Texture2D*                tex[2];
	ID3D11ShaderResourceView*       srv[2];
	ID3D11RenderTargetView*         rtv[2];
	int frontBufferIdx = 0;
	int backBufferIdx = 1;
}

LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
	PAINTSTRUCT ps;
	HDC hdc;

	int mouseX = GET_X_LPARAM(lParam);
	int mouseY = GET_Y_LPARAM(lParam);

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
	// Start by creating a WNDCLASSEX structure with it's size information 
	// this will allow us to set the properties of our window class
	WNDCLASSEX wcex = { sizeof(WNDCLASSEX) };
	// we want to redraw the entire window if a movement or size adjustment changes the width/height of the window
	wcex.style = CS_HREDRAW | CS_VREDRAW;
	// A pointer to our windows messaging function
	wcex.lpfnWndProc = WndProc;
	// Our windows instance
	wcex.hInstance = hInstance;
	// Class Icon - NULL provides the system default icon
	wcex.hIcon = NULL;
	// What sort of cursor we want to use
	wcex.hCursor = LoadCursor(NULL, IDC_ARROW);
	// Name of our class - constant so we can de-register our class at the end of the program
	wcex.lpszClassName = kAppName;
	// Win32 API call to register our class
	if (!RegisterClassEx(&wcex))
		return false;

	m_HInst = hInstance;
	// Define the size of our window
	RECT rc = { 0,0,kAppWidth,kAppHeight };
	AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, false);
	// Create our window using the Rect we just defined, app name for the title bar and our class name
	m_HWnd = CreateWindow(kAppName, kAppName, WS_OVERLAPPEDWINDOW,
		CW_USEDEFAULT, CW_USEDEFAULT,
		rc.right - rc.left, rc.bottom - rc.top,
		NULL, NULL, hInstance,
		NULL);
	if (!m_HWnd)
		return false;

	// Win32 API call to Display the window for the user
	ShowWindow(m_HWnd, nCmdShow);
	return true;
}

bool RendererD3D11::SetupDevice(const char* vsFileName, const char* psFileName)
{
	// Calculate client width and height
	RECT rc;
	GetClientRect(m_HWnd, &rc);
	unsigned width = rc.right - rc.left;
	unsigned height = rc.bottom - rc.top;

	// Convenient method for creating a D3D viewport in my window
	m_Viewport = CD3D11_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));

	// Fill in the resolution and aspect ratio values for the buffer that will be passed over to the shader program when run
	gToyBuffer.resolution.x = static_cast<float>(width);
	gToyBuffer.resolution.y = static_cast<float>(height);
	gToyBuffer.aspect = gToyBuffer.resolution.x / gToyBuffer.resolution.y;

	// Create our swap chain definition, No AA or anything too fancy, this is standard boilerplate code that can be found in almost every D3D program
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

	// Call a d3d function to create our swap chain, device and a context so we can start doing things in 3D
	D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
	if (FAILED(D3D11CreateDeviceAndSwapChain(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL, 0,
		&featureLevel, 1, D3D11_SDK_VERSION, &sd, &m_SwapChain, &m_Device, NULL, &m_Context)))
		return false;

	// Our method for drawing is fairly simple - we will draw our frame to a texture buffer, then display that buffer in our window while we write to our second texture for the next frame.
	if (FAILED(m_SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&m_BackBuffer)))
		return false;

	if (FAILED(m_Device->CreateRenderTargetView(m_BackBuffer, NULL, &m_RenderTargetView)))
		return false;

	CD3D11_TEXTURE2D_DESC texDesc(sd.BufferDesc.Format,
		sd.BufferDesc.Width, sd.BufferDesc.Height,
		1, 1, D3D11_BIND_SHADER_RESOURCE | D3D11_BIND_RENDER_TARGET);

	// Create the two textures that we will be drawing to
	for (int i = 0; i < 2; i++)
	{
		m_Device->CreateTexture2D(&texDesc, NULL, &PingPong::tex[i]);
		m_Device->CreateRenderTargetView(PingPong::tex[i], NULL, &PingPong::rtv[i]);
		m_Device->CreateShaderResourceView(PingPong::tex[i], NULL, &PingPong::srv[i]);
		((ID3D11Texture2D*)PingPong::tex[i])->Release();
	}

	// Create a buffer of size ToyShaderBuffer in order to plug values into my Shader
	CD3D11_BUFFER_DESC desc(sizeof(ToyShaderBuffer), D3D11_BIND_CONSTANT_BUFFER);
	if (FAILED(m_Device->CreateBuffer(&desc, NULL, &m_ToyBuffer)))
		return false;

	// Load pre-compiled shaders from disk into bytes and then pass the bytes into DirectX to create vertex and pixel shader objects
	if (!LoadCompiledShadersFromDisk(vsFileName, psFileName))
		return false;

	// Texture sampler
	CD3D11_SAMPLER_DESC sampDesc(D3D11_DEFAULT);
	sampDesc.AddressU = sampDesc.AddressV = sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
	if (FAILED(m_Device->CreateSamplerState(&sampDesc, &m_SamplerSmooth)))
		return false;

	// Set Viewport and drawing mode to Triangles
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
	m_Context->ClearRenderTargetView(PingPong::rtv[PingPong::frontBufferIdx], clearColour);

	m_Context->OMSetRenderTargets(1, &PingPong::rtv[PingPong::frontBufferIdx], NULL);

	m_Context->UpdateSubresource(m_ToyBuffer, 0, NULL, &gToyBuffer, 0, 0);

	m_Context->PSSetConstantBuffers(0, 1, &m_ToyBuffer);

	m_Context->PSSetShaderResources(0, 1, &PingPong::srv[PingPong::backBufferIdx]);

	m_Context->PSSetSamplers(0, 1, &m_SamplerSmooth);

	m_Context->Draw(3, 0);

	m_Context->CopyResource(m_BackBuffer, PingPong::tex[PingPong::frontBufferIdx]);

	m_SwapChain->Present(0, 0);

	std::swap(PingPong::frontBufferIdx, PingPong::backBufferIdx);
}

bool RendererD3D11::LoadCompiledShadersFromDisk(const char* vsFileName, const char* psFileName)
{
	size_t size = 0;
	char* buffer;
	if (!ReadFileToBytes(vsFileName, &buffer, &size))
		return false;

	// Create the vertex shader object from our bytes of len: size
	HRESULT res = m_Device->CreateVertexShader(buffer, size, NULL, &m_ToyVS);
	// Free the memory we alloc'd in the function so we can use the variable again in the next one
	// Utilising the free macro instead of delete allows me to bypass the requirement to guard against null
	// I recall reading somewhere that it is best practace when freeing char[]
	free(buffer);
	size = 0;

	if (FAILED(res))
		return false;

	if (!ReadFileToBytes(psFileName, &buffer, &size))
		return false;

	// Create the pixel shader object from our bytes of len: size
	res = m_Device->CreatePixelShader(buffer, size, NULL, &m_ToyPS);
	free(buffer);
	size = 0;

	if (FAILED(res))
		return false;

	// Tell our D3DContext to use our Vertex and pixel shaders
	//		Normally this can be done in render loop when having many shaders to swap shaders in and out, 
	//		however since this renderer is really simple and only renders one shader - setting this here
	//		is acceptable.
	m_Context->VSSetShader(m_ToyVS, NULL, 0);
	m_Context->PSSetShader(m_ToyPS, NULL, 0);

	return true;
}

bool RendererD3D11::ReadFileToBytes(const char* fileName, char** bufferOut, size_t* bufLenOut)
{
	// Standard method for reading a file from disk into a byte array using standard streams. 
	// My only real concern with this function is the fact it allocs memory without any indication to the client.
	std::ifstream stream;
	stream.open(fileName, std::ifstream::in | std::ifstream::binary);
	if (!stream.good())
		return false;

	// Precalculate length and alloc buffer
	stream.seekg(0, std::ios::end);
	*bufLenOut = size_t(stream.tellg());
	*bufferOut = new char[*bufLenOut];
	// Read bytes into buffer
	stream.seekg(0, std::ios::beg);
	stream.read(bufferOut[0], *bufLenOut);
	stream.close();

	if (*bufferOut == NULL)
		return false;

	return true;
}
