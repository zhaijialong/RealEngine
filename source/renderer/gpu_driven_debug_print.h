#pragma once

#include "resource/texture_2d.h"
#include "resource/raw_buffer.h"
#include "resource/structured_buffer.h"

class Renderer;

class GpuDrivenDebugPrint
{
public:
    GpuDrivenDebugPrint(Renderer* pRenderer);

    void Clear(IGfxCommandList* pCommandList);
    void PrepareForDraw(IGfxCommandList* pCommandList);
    void Draw(IGfxCommandList* pCommandList);

    IGfxDescriptor* GetTextCounterBufferUAV() const { return m_pTextCounterBuffer->GetUAV(); }
    IGfxDescriptor* GetTextBufferUAV() const { return m_pTextBuffer->GetUAV(); }
    IGfxDescriptor* GetFontCharBufferSRV() const { return m_pFontCharBuffer->GetSRV(); }

private:
    void CreateFontTexture();

private:
    Renderer* m_pRenderer = nullptr;
    IGfxPipelineState* m_pDrawPSO = nullptr;
    IGfxPipelineState* m_pBuildCommandPSO = nullptr;

    eastl::unique_ptr<Texture2D> m_pFontTexture;
    eastl::unique_ptr<StructuredBuffer> m_pFontCharBuffer; //0-128 to quad uv mapping

    eastl::unique_ptr<StructuredBuffer> m_pTextBuffer;
    eastl::unique_ptr<RawBuffer> m_pTextCounterBuffer;
    eastl::unique_ptr<RawBuffer> m_pDrawArugumentsBuffer;
};