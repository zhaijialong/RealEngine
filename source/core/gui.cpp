#include "gui.h"
#include "engine.h"
#include "imgui/imgui.h"
#include "imgui/imgui_impl_win32.h"

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

	// Build texture atlas
	ImGuiIO& io = ImGui::GetIO();
	unsigned char* pixels;
	int width, height;
	io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);

	io.Fonts->TexID = 0; //TODO !

	GfxGraphicsPipelineDesc desc;
	desc.vs = pRenderer->GetShader("imgui.hlsl", "vs_main", "vs_6_6", {});
	desc.ps = pRenderer->GetShader("imgui.hlsl", "ps_main", "ps_6_6", {});


	return true;
}

void GUI::NewFrame()
{
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
}

void GUI::Render()
{
	ImGui::Render();
}
