#include "pix_runtime.h"

typedef HRESULT(WINAPI* BeginEventOnCommandList)(ID3D12GraphicsCommandList* commandList, UINT64 color, _In_ PCSTR formatString);
typedef HRESULT(WINAPI* EndEventOnCommandList)(ID3D12GraphicsCommandList* commandList);
typedef HRESULT(WINAPI* SetMarkerOnCommandList)(ID3D12GraphicsCommandList* commandList, UINT64 color, _In_ PCSTR formatString);

static BeginEventOnCommandList pixBeginEventOnCommandList = nullptr;
static EndEventOnCommandList   pixEndEventOnCommandList = nullptr;
static SetMarkerOnCommandList  pixSetMarkerOnCommandList = nullptr;

void pix::Init()
{
	HMODULE module = LoadLibrary(L"WinPixEventRuntime.dll");

	if (module)
	{
		pixBeginEventOnCommandList = (BeginEventOnCommandList)GetProcAddress(module, "PIXBeginEventOnCommandList");
		pixEndEventOnCommandList = (EndEventOnCommandList)GetProcAddress(module, "PIXEndEventOnCommandList");
		pixSetMarkerOnCommandList = (SetMarkerOnCommandList)GetProcAddress(module, "PIXSetMarkerOnCommandList");
	}
}

void pix::BeginEvent(ID3D12GraphicsCommandList* pCommandList, const char* event)
{
	pixBeginEventOnCommandList(pCommandList, 0, event);
}

void pix::EndEvent(ID3D12GraphicsCommandList* pCommandList)
{
	pixEndEventOnCommandList(pCommandList);
}
