# RealEngine

my toy engine, mainly for rendering expariment and learning

## planned features

* render graph(~~barriers~~, ~~resource aliasing~~, async compute)
* ~~GTAO~~
* SSR
* SSGI
* clustered shading
* volume cloud
* volume fog
* ~~LPM~~
* ~~TAA~~
* ~~CAS~~
* ~~meshlet~~
* RTX
* variable rate shading
* sampler feedback
* sparse texture
* sparse shadowmap
* water/ocean
* large-scale landscape

## AMD related issues

tested on RX6600(win11, Adrenalin 21.11.3)

* PIX crashes when capturing
* indirect DispatchMesh results in GPU hang
    ```cpp
    void GpuDrivenDebugPrint::Draw(IGfxCommandList* pCommandList)
    {
        ...

        if (m_pRenderer->GetDevice()->GetVendor() == GfxVendor::AMD)
        {
            pCommandList->DispatchMesh(100, 1, 1);
        }
        else
        {
            pCommandList->DispatchMeshIndirect(m_pDrawArugumentsBuffer->GetBuffer(), 0);
        }
    }
    ```
* using `SV_IsFrontFace` in an AS-MS-PS combine results in `DXGI_ERROR_DRIVER_INTERNAL_ERROR` when creating the PSO
    ```
    D3D12: Removing Device.
    D3D12 WARNING: ID3D12Device::RemoveDevice: Device removal has been triggered for the following reason (DXGI_ERROR_DRIVER_INTERNAL_ERROR: There is strong evidence that the driver has performed an undefined operation; but it may be because the application performed an illegal or undefined operation to begin with.). [ EXECUTION WARNING #233: DEVICE_REMOVAL_PROCESS_POSSIBLY_AT_FAULT]
    ```