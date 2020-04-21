// Simple as potato

#pragma once

#include <d3d11.h>
#include <dxgi1_2.h>
#include <vector>
#include <string>

class D3DBackend
{
	HWND savedHwnd;

	ID3D11Device* device = nullptr;
	ID3D11DeviceContext* deviceContext = nullptr;
	IDXGIDevice1* dxgiDevice = nullptr;
	IDXGIAdapter* adapter = nullptr;
	IDXGIFactory2* dxgiFactory = nullptr;
	IDXGISwapChain1* swapChain = nullptr;
	ID3D11Texture2D* backBuffer = nullptr;
	ID3D11RenderTargetView* renderTarget = nullptr;

	ID3D11VertexShader* vertexShader = nullptr;
	ID3D11PixelShader* pixelShader = nullptr;

	HRESULT CreateSwapChain(HWND hwnd);

	std::vector<uint8_t> vsData;
	std::vector<uint8_t> psData;

	ID3D11InputLayout* inputLayout = nullptr;

public:
	D3DBackend(HWND hwnd);
	~D3DBackend();

	void InitRender();
	void BeginRender();
	void DoneRender();

	void ClearScreen();
	void DrawStuff();

	void LoadVertexShader(std::string filename);
	void LoadPixelShader(std::string filename);
};
