#pragma once

#include "renderer/renderer.h"
#include "EASTL/functional.h"

class GUI
{
public:
    GUI();
    ~GUI();

    bool Init();
    void Tick();
    void Render(IGfxCommandList* pCommandList);

    void AddCommand(const eastl::function<void()>& command) { m_commands.push_back(command); }

private:
    void SetupRenderStates(IGfxCommandList* pCommandList, uint32_t frame_index);

private:
    IGfxPipelineState* m_pPSO = nullptr;

    eastl::unique_ptr<Texture2D> m_pFontTexture;

    eastl::unique_ptr<StructuredBuffer> m_pVertexBuffer[GFX_MAX_INFLIGHT_FRAMES];
    eastl::unique_ptr<IndexBuffer> m_pIndexBuffer[GFX_MAX_INFLIGHT_FRAMES];

    std::vector<eastl::function<void()>> m_commands;
};