#include "gui.h"

#include "../imgui/imgui_impl_dx9.h"
#include "../imgui/imgui_impl_win32.h"
#include <xmemory>
#include <io.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter
);

long __stdcall WindowProcess(
	HWND window,
	UINT message,
	WPARAM wideParameter,
	LPARAM longParameter)
{
	if (ImGui_ImplWin32_WndProcHandler(window, message, wideParameter, longParameter))
		return true;

	switch (message)
	{
	case WM_SIZE: {
		if (gui::device && wideParameter != SIZE_MINIMIZED)
		{
			gui::presentParameters.BackBufferWidth = LOWORD(longParameter);
			gui::presentParameters.BackBufferHeight = HIWORD(longParameter);
			gui::ResetDevice();
		}
	}return 0;

	case WM_SYSCOMMAND: {
		if ((wideParameter & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
			return 0;
	}break;

	case WM_DESTROY: {
		PostQuitMessage(0);
	}return 0;

	case WM_LBUTTONDOWN: {
		gui::position = MAKEPOINTS(longParameter); // set click points
	}return 0;

	case WM_MOUSEMOVE: {
		if (wideParameter == MK_LBUTTON)
		{
			const auto points = MAKEPOINTS(longParameter);
			auto rect = ::RECT{ };

			GetWindowRect(gui::window, &rect);

			rect.left += points.x - gui::position.x;
			rect.top += points.y - gui::position.y;

			if (gui::position.x >= 0 &&
				gui::position.x <= gui::WINDOW_WIDTH &&
				gui::position.y >= 0 && gui::position.y <= 19)
				SetWindowPos(
					gui::window,
					HWND_TOPMOST,
					rect.left,
					rect.top,
					0, 0,
					SWP_SHOWWINDOW | SWP_NOSIZE | SWP_NOZORDER
				);
		}

	}return 0;

	}

	return DefWindowProc(window, message, wideParameter, longParameter);
}

void gui::CreateHWindow(const char* windowName) noexcept
{
	windowClass.cbSize = sizeof(WNDCLASSEX);
	windowClass.style = CS_CLASSDC;
	windowClass.lpfnWndProc = WindowProcess;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = GetModuleHandleA(0);
	windowClass.hIcon = 0;
	windowClass.hCursor = 0;
	windowClass.hbrBackground = 0;
	windowClass.lpszMenuName = 0;
	windowClass.lpszClassName = "class001";
	windowClass.hIconSm = 0;

	RegisterClassEx(&windowClass);

	window = CreateWindowEx(
		0,
		"class001",
		windowName,
		WS_POPUP,
		100,
		100,
		WINDOW_WIDTH,
		WINDOW_HEIGHT,
		0,
		0,
		windowClass.hInstance,
		0
	);

	ShowWindow(window, SW_SHOWDEFAULT);
	UpdateWindow(window);
}

void gui::DestroyHWindow() noexcept
{
	DestroyWindow(window);
	UnregisterClass(windowClass.lpszClassName, windowClass.hInstance);
}

bool gui::CreateDevice() noexcept
{
	d3d = Direct3DCreate9(D3D_SDK_VERSION);

	if (!d3d)
		return false;

	ZeroMemory(&presentParameters, sizeof(presentParameters));

	presentParameters.Windowed = TRUE;
	presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
	presentParameters.BackBufferFormat = D3DFMT_UNKNOWN;
	presentParameters.EnableAutoDepthStencil = TRUE;
	presentParameters.AutoDepthStencilFormat = D3DFMT_D16;
	presentParameters.PresentationInterval = D3DPRESENT_INTERVAL_ONE;

	if (d3d->CreateDevice(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		window,
		D3DCREATE_HARDWARE_VERTEXPROCESSING,
		&presentParameters,
		&device) < 0)
		return false;

	return true;
}

void gui::ResetDevice() noexcept
{
	ImGui_ImplDX9_InvalidateDeviceObjects();

	const auto result = device->Reset(&presentParameters);

	if (result == D3DERR_INVALIDCALL)
		IM_ASSERT(0);

	ImGui_ImplDX9_CreateDeviceObjects();
}

void gui::DestroyDevice() noexcept
{
	if (device)
	{
		device->Release();
		device = nullptr;
	}

	if (d3d)
	{
		d3d->Release();
		d3d = nullptr;
	}
}

static ImFont* font = nullptr;

void gui::CreateImGui() noexcept
{
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ::ImGui::GetIO();
	font = io.Fonts->AddFontFromFileTTF("C:/Windows/Fonts/PF DinDisplay Pro.ttf", 14);

	io.IniFilename = NULL;

	ImGui::StyleColorsDark();

	ImGui_ImplWin32_Init(window);
	ImGui_ImplDX9_Init(device);
}

void gui::DestroyImGui() noexcept
{
	ImGui_ImplDX9_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();
}

void gui::BeginRender() noexcept
{
	MSG message;
	while (PeekMessage(&message, 0, 0, 0, PM_REMOVE))
	{
		TranslateMessage(&message);
		DispatchMessage(&message);

		if (message.message == WM_QUIT)
		{
			isRunning = !isRunning;
			return;
		}
	}

	// Start the Dear ImGui frame
	ImGui_ImplDX9_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();
	ImGui::PushFont(font);
}

void gui::EndRender() noexcept
{
	ImGui::EndFrame();

	device->SetRenderState(D3DRS_ZENABLE, FALSE);
	device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
	device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);

	device->Clear(0, 0, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, D3DCOLOR_RGBA(0, 0, 0, 255), 1.0f, 0);

	if (device->BeginScene() >= 0)
	{
		ImGui::Render();
		ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
		device->EndScene();
	}

	const auto result = device->Present(0, 0, 0, 0);

	// Handle loss of D3D9 device
	if (result == D3DERR_DEVICELOST && device->TestCooperativeLevel() == D3DERR_DEVICENOTRESET)
		ResetDevice();
}

inline ImVec4 hexColor(unsigned long long int color, float alpha = 1.0f) {
	return { (color >> 16 & 0xFF) / 255.f, (color >> 8 & 0xFF) / 255.f, (color & 0xFF) / 255.f, alpha };
}

bool enableAimbot = false;
bool enableEsp = false;
bool enableHealthEsp = false;
bool enableIgnoreNPCs = false;
bool enableIgnoreTeammates = false;
bool enableTriggerBot = false;
bool enableSilentAim = false;

bool selectEjectHotkey = false;

bool enableNoRecoilCheckbox = false;
bool enableNoSpreadCheckbox = false;

bool enableItemESPCheckbox = false;
bool enableVehicleESPCheckbox = false;
bool enableBoxESPCheckbox = false;
bool enableTurretESPCheckbox = false;

bool enableFreecamCheckbox = false;
bool enableAutoReconnectCheckbox = false;

float noSpread = 0.0f;
float noRecoil = 0.0f;
float aimbotFOV = 0.0f;
float setFOV = 0.0f;

bool enableCursorCheckbox = false;
bool enableDebugMode = false;
bool enableBypassAC = false;
bool enableLoadConfig = false;
bool enableReloadConfig = false;
bool enableSaveConfig = false;

void gui::Render() noexcept
{
	ImGui::SetNextWindowPos({ 0, 0 });
	ImGui::SetNextWindowSize({ WINDOW_WIDTH, WINDOW_HEIGHT });
	ImGui::Begin(
		"TEMPLATE - RUST CHEAT",
		&isRunning,
		ImGuiWindowFlags_NoResize |
		ImGuiWindowFlags_NoSavedSettings |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_NoMove
	);

	ImGuiTabBarFlags tab_bar_flags = ImGuiTabBarFlags_None;
	ImGuiStyle& style = ImGui::GetStyle();
	style.Colors[ImGuiCol_CheckMark] = hexColor(0x68C346);
	style.Colors[ImGuiCol_ButtonHovered] = hexColor(0x68C346);
	style.Colors[ImGuiCol_Button] = hexColor(0x3461FF);
	style.Colors[ImGuiCol_Border] = hexColor(0x1A1A1A);
	style.Colors[ImGuiCol_ScrollbarGrab] = hexColor(0xFFFFFF);

	if (ImGui::BeginTabBar("navBar", tab_bar_flags))
	{

		if (ImGui::BeginTabItem("MAIN-MENU"))
		{
			ImGui::Text("Rust Cheat - Configuration");
			ImGui::Button("INJECT");

			ImGui::Spacing();
			ImGui::Spacing();

			ImGui::Text("AIMBOT-SETTINGS");
			ImGui::Checkbox("Enable Ignore-Teammates", &enableIgnoreTeammates);
			ImGui::Checkbox("Enable Ignore-NPCs", &enableIgnoreNPCs);
			ImGui::Checkbox("Enable Trigger-Bot", &enableTriggerBot);
			ImGui::Checkbox("Enable Silent-Aim", &enableSilentAim);

			ImGui::Spacing();
			ImGui::Spacing();

			ImGui::Text("Field of View");
			ImGui::SliderFloat("##fov", &setFOV, 0.0f, 100.0f, "%.2f", ImGuiInputTextFlags_None);

			ImGui::Spacing();
			ImGui::Spacing();

			ImGui::Checkbox("Enable Aimbot", &enableAimbot);
			const char* bones[] = { "Head", "Neck", "Body", "Leg", "Foot", "Left Arm", "Right Arm" };
			static int bone_current = 0;
			ImGui::Combo("##bones", &bone_current, bones, IM_ARRAYSIZE(bones));

			ImGui::Spacing();

			ImGui::Text("ESP-Settings");
			ImGui::Checkbox("Enable ESP", &enableEsp);
			ImGui::Checkbox("Enable Health-ESP", &enableHealthEsp);
			const char* espStyle[] = { "Glow", "Radar", "Box", "Skelleton", "Distance in M" };
			static int style_current = 0;
			ImGui::Combo("##esp", &style_current, espStyle, IM_ARRAYSIZE(espStyle));

			ImGui::Spacing();
			ImGui::Spacing();

			ImGui::Text("ESP MISC-SETTINGS");
			ImGui::Checkbox("Enable ITEM-ESP", &enableItemESPCheckbox);
			ImGui::Checkbox("Enable Vehicle-ESP", &enableVehicleESPCheckbox);
			ImGui::Checkbox("Enable Turret-ESP", &enableTurretESPCheckbox);
			ImGui::Checkbox("Enable Box-ESP", &enableBoxESPCheckbox);

			ImGui::Spacing();
			ImGui::Spacing();

			ImGui::Text("NO-SPREAD SETTINGS");
			ImGui::Checkbox("No Spread", &enableNoSpreadCheckbox);
			ImGui::Checkbox("No Recoil", &enableNoRecoilCheckbox);
			ImGui::SliderFloat("##spread", &noSpread, 0.0f, 100.0f, "%.2f", ImGuiInputTextFlags_None);
			ImGui::SliderFloat("##recoil", &noRecoil, 0.0f, 100.0f, "%.2f", ImGuiInputTextFlags_None);

			ImGui::Spacing();
			ImGui::Spacing();

			ImGui::Text("MISC-SETTINGS");
			ImGui::Checkbox("Enable Freecam", &enableFreecamCheckbox);
			ImGui::Checkbox("Enable Auto-Reconnect", &enableAutoReconnectCheckbox);
			ImGui::EndTabItem();

			ImGui::Spacing();
			ImGui::Spacing();
		}
	}

	if (ImGui::BeginTabItem("SETTINGS"))
	{
		ImGui::Text("Rust Cheat - Settings");
		ImGui::Spacing();
		ImGui::Spacing();

		ImGui::Button("UNLOAD CHEAT");
		ImGui::Button("LOAD CONFIG");
		ImGui::Button("RELOAD CONFIG");
		ImGui::Button("SAVE CONFIG");
		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Spacing();


		ImGui::Text("Open Menu:");
		ImGui::Button("Hotkey: F10");
		ImGui::Spacing();
		ImGui::Text("Panic Button:");
		ImGui::Button("Hotkey: F7");
		ImGui::Spacing();
		ImGui::Spacing();
		ImGui::Spacing();

		ImGui::Text("EXTENDED CHEAT-OPTIONS");
		ImGui::Spacing();
		ImGui::Spacing();

		ImGui::Checkbox("Enable Cursor", &enableCursorCheckbox);
		ImGui::Checkbox("Enable Debug-Mode", &enableDebugMode);
		ImGui::Checkbox("Enable Bypass-AC", &enableBypassAC);


		ImGui::EndTabItem();
	}
	ImGui::EndTabBar();

	ImGui::End();
}