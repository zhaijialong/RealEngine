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
* SV_IsFrontFace results in GPU hang
    ```hlsl
    GBufferOutput ps_main(VertexOut input, bool isFrontFace : SV_IsFrontFace)
    ```