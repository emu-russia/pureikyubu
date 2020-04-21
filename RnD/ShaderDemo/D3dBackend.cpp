// http://www.directxtutorial.com/Lesson.aspx?lessonid=111-4-5

#include "framework.h"
#include "D3dBackend.h"
#include <cassert>

D3DBackend::D3DBackend(HWND hwnd)
{
	savedHwnd = hwnd;

	HRESULT res = D3D11CreateDevice(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		0,
		nullptr,
		0,
		D3D11_SDK_VERSION,
		&device,
		nullptr,
		&deviceContext);
	assert(res == S_OK);

	res = device->QueryInterface(&dxgiDevice);
	assert(res == S_OK);
	
	res = dxgiDevice->GetAdapter(&adapter);
	assert(res == S_OK);

	res = adapter->GetParent(__uuidof(IDXGIFactory2), (void**)&dxgiFactory);
	assert(res == S_OK);

	res = CreateSwapChain(hwnd);
	assert(res == S_OK);

	res = swapChain->Present(1, 0);
	assert(res == S_OK);

	res = swapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&backBuffer);
	assert(res == S_OK);

	res = device->CreateRenderTargetView(backBuffer, nullptr, &renderTarget);
	assert(res == S_OK);

	deviceContext->OMSetRenderTargets(1, &renderTarget, nullptr);

	InitRender();
}

HRESULT D3DBackend::CreateSwapChain(HWND hwnd)
{
	DXGI_SWAP_CHAIN_DESC1 scd = { 0 };
	scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scd.BufferCount = 2;
	scd.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
	scd.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
	scd.SampleDesc.Count = 1;

	return dxgiFactory->CreateSwapChainForHwnd(device, hwnd, &scd, nullptr, nullptr, &swapChain);
}

D3DBackend::~D3DBackend()
{
	deviceContext->Release();
	device->Release();
}

void D3DBackend::ClearScreen()
{
	float color[4] = { 0.0f, 0.2f, 0.4f, 1.0f };
	deviceContext->ClearRenderTargetView(renderTarget, color);
}

void D3DBackend::InitRender()
{
	D3D11_INPUT_ELEMENT_DESC ied[] =
	{
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0},
	};

	device->CreateInputLayout(ied, ARRAYSIZE(ied), vsData.data(), vsData.size(), &inputLayout);
	deviceContext->IASetInputLayout(inputLayout);

	D3D11_VIEWPORT viewport = { 0 };

	RECT rect;

	GetWindowRect(savedHwnd, &rect);

	viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = rect.right - rect.left;
	viewport.Height = rect.bottom - rect.top;

	deviceContext->RSSetViewports(1, &viewport);
}

void D3DBackend::BeginRender()
{
	deviceContext->OMSetRenderTargets(1, &renderTarget, nullptr);

}

void D3DBackend::DoneRender()
{
	HRESULT res;

	res = swapChain->Present(1, 0);
	assert(res == S_OK);
}

struct VERTEX
{
	float X, Y, Z;
};

void D3DBackend::DrawStuff()
{
	VERTEX OurVertices[] =
	{
		{ 0.0f, 0.5f, 0.0f },
		{ 0.45f, -0.5f, 0.0f },
		{ -0.45f, -0.5f, 0.0f },
	};
	ID3D11Buffer* vertexBuffer = nullptr;

	// create the vertex buffer
	D3D11_BUFFER_DESC bd = { 0 };
	bd.ByteWidth = sizeof(VERTEX) * _countof(OurVertices);
	bd.BindFlags = D3D11_BIND_VERTEX_BUFFER;

	D3D11_SUBRESOURCE_DATA srd = { OurVertices, 0, 0 };

	device->CreateBuffer(&bd, &srd, &vertexBuffer);

	// set the vertex buffer
	UINT stride = sizeof(VERTEX);
	UINT offset = 0;
	deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);

	// set the primitive topology
	deviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	// draw 3 vertices, starting from vertex 0
	deviceContext->Draw(3, 0);


	vertexBuffer->Release();
}

void LoadFile(std::vector<uint8_t> & Data, std::string File)
{
	FILE* f;
	fopen_s(&f, File.c_str(), "rb");
	assert(f);

	fseek(f, 0, SEEK_END);
	size_t size = ftell(f);
	fseek(f, 0, SEEK_SET);

	Data.resize(size);

	fread(Data.data(), 1, size, f);
	fclose(f);
}

void D3DBackend::LoadVertexShader(std::string filename)
{
	LoadFile(vsData, filename);

	device->CreateVertexShader(vsData.data(), vsData.size(), nullptr, &vertexShader);
	deviceContext->VSSetShader(vertexShader, nullptr, 0);
}

void D3DBackend::LoadPixelShader(std::string filename)
{
	LoadFile(psData, filename);

	device->CreatePixelShader(psData.data(), psData.size(), nullptr, &pixelShader);
	deviceContext->PSSetShader(pixelShader, nullptr, 0);
}
