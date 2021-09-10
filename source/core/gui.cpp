#include "gui.h"
#include "engine.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"
#include "ImGuizmo/ImGuizmo.h"

GUI::GUI()
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	//io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
	io.BackendRendererName = "imgui_impl_realengine";
	io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;  // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.

	ImGui::StyleColorsDark();
	//ImGui::StyleColorsClassic();

	ImGui_ImplWin32_Init(Engine::GetInstance()->GetWindowHandle());
}

GUI::~GUI()
{
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

bool GUI::Init()
{
	Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
	IGfxDevice* pDevice = pRenderer->GetDevice();

	std::string font_file = Engine::GetInstance()->GetAssetPath() + "fonts/DroidSans.ttf";

	ImGuiIO& io = ImGui::GetIO();
	io.Fonts->AddFontFromFileTTF(font_file.c_str(), 14.0f);

	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	m_pFontTexture.reset(pRenderer->CreateTexture2D(width, height, 1, GfxFormat::RGBA8UNORM, GfxTextureUsageShaderResource, "GUI::m_pFontTexture"));
	pRenderer->UploadTexture(m_pFontTexture->GetTexture(), pixels, width * height * 4);

	io.Fonts->TexID = (ImTextureID)m_pFontTexture->GetSRV();

	GfxGraphicsPipelineDesc psoDesc;
	psoDesc.vs = pRenderer->GetShader("imgui.hlsl", "vs_main", "vs_6_6", {});
	psoDesc.ps = pRenderer->GetShader("imgui.hlsl", "ps_main", "ps_6_6", {});
	psoDesc.blend_state[0].blend_enable = true;
	psoDesc.blend_state[0].color_src = GfxBlendFactor::SrcAlpha;
	psoDesc.blend_state[0].color_dst = GfxBlendFactor::InvSrcAlpha;
	psoDesc.blend_state[0].alpha_src = GfxBlendFactor::One;
	psoDesc.blend_state[0].alpha_dst = GfxBlendFactor::InvSrcAlpha;
	psoDesc.rt_format[0] = pRenderer->GetSwapchain()->GetBackBuffer()->GetDesc().format;
	m_pPSO = pRenderer->GetPipelineState(psoDesc, "GUI PSO");

	return true;
}

void GUI::Tick()
{
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	ImGuizmo::BeginFrame();

	ImGuiIO& io = ImGui::GetIO();
	ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
}

void GUI::Render(IGfxCommandList* pCommandList)
{
	GPU_EVENT(pCommandList, "GUI");
	ImGui::Render();

	Renderer* pRenderer = Engine::GetInstance()->GetRenderer();
	IGfxDevice* pDevice = pRenderer->GetDevice();
	ImDrawData* draw_data = ImGui::GetDrawData();

	// Avoid rendering when minimized
	if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
	{
		return;
	}

	uint32_t frame_index = pDevice->GetFrameID() % MAX_INFLIGHT_FRAMES;

	if (m_pVertexBuffer[frame_index] == nullptr || m_pVertexBuffer[frame_index]->GetBuffer()->GetDesc().size < draw_data->TotalVtxCount * sizeof(ImDrawVert))
	{
		m_pVertexBuffer[frame_index].reset(pRenderer->CreateStructuredBuffer(nullptr, sizeof(ImDrawVert), draw_data->TotalVtxCount + 5000, "GUI VB", GfxMemoryType::CpuToGpu));
	}

	if (m_pIndexBuffer[frame_index] == nullptr || m_pIndexBuffer[frame_index]->GetIndexCount() < (uint32_t)draw_data->TotalIdxCount)
	{
		m_pIndexBuffer[frame_index].reset(pRenderer->CreateIndexBuffer(nullptr, sizeof(ImDrawIdx), draw_data->TotalIdxCount + 10000, "GUI IB", GfxMemoryType::CpuToGpu));
	}

	ImDrawVert* vtx_dst = (ImDrawVert*)m_pVertexBuffer[frame_index]->GetBuffer()->GetCpuAddress();
	ImDrawIdx* idx_dst = (ImDrawIdx*)m_pIndexBuffer[frame_index]->GetBuffer()->GetCpuAddress();
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
		memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));
		vtx_dst += cmd_list->VtxBuffer.Size;
		idx_dst += cmd_list->IdxBuffer.Size;
	}

	SetupRenderStates(pCommandList, frame_index);

	// Render command lists
	// (Because we merged all buffers into a single one, we maintain our own offset into them)
	int global_vtx_offset = 0;
	int global_idx_offset = 0;
	ImVec2 clip_off = draw_data->DisplayPos;
	for (int n = 0; n < draw_data->CmdListsCount; n++)
	{
		const ImDrawList* cmd_list = draw_data->CmdLists[n];
		for (int cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; cmd_i++)
		{
			const ImDrawCmd* pcmd = &cmd_list->CmdBuffer[cmd_i];
			if (pcmd->UserCallback != NULL)
			{
				// User callback, registered via ImDrawList::AddCallback()
				// (ImDrawCallback_ResetRenderState is a special callback value used by the user to request the renderer to reset render state.)
				if (pcmd->UserCallback == ImDrawCallback_ResetRenderState)
				{
					SetupRenderStates(pCommandList, frame_index);
				}
				else
				{
					pcmd->UserCallback(cmd_list, pcmd);
				}
			}
			else
			{
				// Apply Scissor, Bind texture, Draw
				float x = pcmd->ClipRect.x - clip_off.x;
				float y = pcmd->ClipRect.y - clip_off.y;
				float width = pcmd->ClipRect.z - pcmd->ClipRect.x;
				float height = pcmd->ClipRect.w - pcmd->ClipRect.y;

				if (width > 0 && height > 0)
				{				
					pCommandList->SetScissorRect((uint32_t)x, (uint32_t)y, (uint32_t)width, (uint32_t)height);

					uint32_t resource_ids[4] = { 
						m_pVertexBuffer[frame_index]->GetSRV()->GetHeapIndex(), 
						pcmd->VtxOffset + global_vtx_offset, 
						((IGfxDescriptor*)pcmd->TextureId)->GetHeapIndex(),
						pRenderer->GetLinearSampler()->GetHeapIndex() };

					pCommandList->SetConstantBuffer(GfxPipelineType::Graphics, 0, resource_ids, sizeof(resource_ids));

					pCommandList->DrawIndexed(pcmd->ElemCount, 1, pcmd->IdxOffset + global_idx_offset);
				}
			}
		}
		global_idx_offset += cmd_list->IdxBuffer.Size;
		global_vtx_offset += cmd_list->VtxBuffer.Size;
	}
}

void GUI::SetupRenderStates(IGfxCommandList* pCommandList, uint32_t frame_index)
{
	ImDrawData* draw_data = ImGui::GetDrawData();

	pCommandList->SetViewport(0, 0, (uint32_t)draw_data->DisplaySize.x, (uint32_t)draw_data->DisplaySize.y);
	pCommandList->SetPipelineState(m_pPSO);
	pCommandList->SetIndexBuffer(m_pIndexBuffer[frame_index]->GetBuffer());

	float L = draw_data->DisplayPos.x;
	float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
	float T = draw_data->DisplayPos.y;
	float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
	float mvp[4][4] =
	{
		{ 2.0f/(R-L),   0.0f,           0.0f,       0.0f },
		{ 0.0f,         2.0f/(T-B),     0.0f,       0.0f },
		{ 0.0f,         0.0f,           0.5f,       0.0f },
		{ (R+L)/(L-R),  (T+B)/(B-T),    0.5f,       1.0f },
	};

	pCommandList->SetConstantBuffer(GfxPipelineType::Graphics, 1, mvp, sizeof(mvp));
}
