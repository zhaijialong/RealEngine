#include "gfx.h"
#include "d3d12/d3d12_device.h"

IGfxDevice* CreateGfxDevice(const GfxDeviceDesc& desc)
{
	IGfxDevice* pDevice = nullptr;

	switch (desc.backend)
	{
	case GfxRenderBackend::D3D12:
		pDevice = new D3D12Device(desc);
		((D3D12Device*)pDevice)->Init();
		break;
	default:
		break;
	}
	
	return pDevice;
}
