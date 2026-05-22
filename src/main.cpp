#include "log/log.hpp"
#include "render/impl/dx11.hpp"
#include "util/types.hpp"

static LRESULT CALLBACK wnd_proc(const HWND hwnd, const UINT msg, const WPARAM wparam, const LPARAM lparam)
{
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

cstd::int32_t main()
{
	LOG_INFO("rendezvous");

	constexpr cstd::int32_t width = 1280;
	constexpr cstd::int32_t height = 720;

	WNDCLASSEXW wnd_class = { };
	wnd_class.cbSize = sizeof(wnd_class);
	wnd_class.lpfnWndProc = wnd_proc;
	wnd_class.hInstance = GetModuleHandleA(nullptr);
	wnd_class.lpszClassName = L"rv_window";

	RegisterClassExW(&wnd_class);

	const HWND hwnd = CreateWindowExW(0, L"rv_window", L"rendezvous", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, nullptr, nullptr, wnd_class.hInstance, nullptr);

	ShowWindow(hwnd, SW_SHOW);

	DXGI_SWAP_CHAIN_DESC swap_chain_desc = { };

	swap_chain_desc.BufferCount = 2;
	swap_chain_desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swap_chain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swap_chain_desc.OutputWindow = hwnd;
	swap_chain_desc.SampleDesc.Count = 1;
	swap_chain_desc.Windowed = true;
	swap_chain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

	rv::dx11_object<IDXGISwapChain> swap_chain;
	rv::dx11_object<ID3D11Device> device;
	rv::dx11_object<ID3D11DeviceContext> context;
	rv::dx11_object<ID3D11RenderTargetView> rtv;

	const HRESULT status = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
	                                                     D3D11_SDK_VERSION, &swap_chain_desc, swap_chain.release_and_get(), device.release_and_get(),
	                                                     nullptr, context.release_and_get());

	if (status != S_OK)
	{
		LOG_ERR("unable to create device and swap chain");

		return 1;
	}

	rv::dx11_object<ID3D11Texture2D> back_buffer;

	swap_chain->GetBuffer(0, IID_PPV_ARGS(back_buffer.release_and_get()));
	device->CreateRenderTargetView(back_buffer.value(), nullptr, rtv.release_and_get());

	back_buffer.release();

	const auto renderer = cstd::make_unique<rv::dx11_renderer>(device.value(), context.value());

	if (!renderer->init())
	{
		LOG_ERR("unable to init renderer");

		return 1;
	}

	const auto font = renderer->add_font("C:\\Windows\\Fonts\\arial.ttf", 32.f);

	MSG msg = { };

	do
	{
		if (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessageW(&msg);

			continue;
		}

		ID3D11RenderTargetView* const tmp_rtv = rtv.value();

		constexpr array_t<float, 4> clear_color = { 0.1f, 0.1f, 0.1f, 1.f };

		context->OMSetRenderTargets(1, &tmp_rtv, nullptr);
		context->ClearRenderTargetView(tmp_rtv, clear_color.data());
		
		renderer->begin_frame({ static_cast<float>(width), static_cast<float>(height) });

		renderer->draw_rect_filled({ 100.f, 100.f }, { 300.f, 250.f }, { 1.f, 0.f, 0.f, 1.f }, 17.5f);
		renderer->draw_rect({ 400.f, 100.f }, { 600.f, 250.f }, { 0.f, 1.f, 0.f, 1.f }, 5.f, 5.f);
		renderer->draw_rect({ 100.f, 300.f }, { 300.f, 450.f }, { 1.f, 1.f, 1.f, 1.f });
		renderer->draw_line({ 500.f, 300.f }, { 555.f, 355.f }, { 0.f, 1.f, 0.f, 1.f }, 2.f);
		renderer->draw_circle({ 500.f, 500.f }, 50.f, { 0.f, 1.f, 0.f, 1.f }, 2.f);
		renderer->draw_circle_filled({ 675.f, 500.f }, 50.f, { 1.f, 0.f, 0.f, 1.f });

		if (font)
		{
			renderer->draw_text(*font, { 100.f, 500.f }, "Hello world!", { 0.f, 1.f, 1.f, 1.f }, 35.f);
		}

		renderer->end_frame();

		swap_chain->Present(1, 0);

	} while (msg.message != WM_QUIT);

	return 0;
}
