#include "log/log.hpp"
#include "render/impl/dx11.hpp"
#include "util/types.hpp"

rv::vector_2d<float> screen_size = { 1280.f, 720.f };

static LRESULT CALLBACK wnd_proc(const HWND hwnd, const UINT msg, const WPARAM wparam, const LPARAM lparam)
{
	if (msg == WM_SIZE && wparam != SIZE_MINIMIZED)
	{
		screen_size.x = static_cast<float>(LOWORD(lparam));
		screen_size.y = static_cast<float>(HIWORD(lparam));
	}

    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

cstd::int32_t main()
{
	LOG_INFO("rendezvous");

	WNDCLASSEXW wnd_class = { };
	wnd_class.cbSize = sizeof(wnd_class);
	wnd_class.lpfnWndProc = wnd_proc;
	wnd_class.hInstance = GetModuleHandleA(nullptr);
	wnd_class.lpszClassName = L"rv_window";

	RegisterClassExW(&wnd_class);

	const HWND hwnd = CreateWindowExW(0, L"rv_window", L"rendezvous", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
	                                  static_cast<cstd::int32_t>(screen_size.x),
	                                  static_cast<cstd::int32_t>(screen_size.y), nullptr, nullptr, wnd_class.hInstance,
	                                  nullptr);

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

	const auto create_rtv = [&]()
		{
			rv::dx11_object<ID3D11Texture2D> back_buffer;

			swap_chain->GetBuffer(0, IID_PPV_ARGS(back_buffer.release_and_get()));
			device->CreateRenderTargetView(back_buffer.value(), nullptr, rtv.release_and_get());

			back_buffer.release();
		};


	create_rtv();

	const auto renderer = cstd::make_unique<rv::dx11_renderer>(device.value(), context.value());

	if (!renderer->init())
	{
		LOG_ERR("unable to init renderer");

		return 1;
	}

	rv::vector_2d<float> last_screen_size = screen_size;

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

		if (last_screen_size != screen_size)
		{
			context->OMSetRenderTargets(0, nullptr, nullptr);
			rtv.release();

			const HRESULT hr = swap_chain->ResizeBuffers(0,
				static_cast<cstd::uint32_t>(screen_size.x),
				static_cast<cstd::uint32_t>(screen_size.y),
				DXGI_FORMAT_UNKNOWN, 0);

			if (FAILED(hr))
			{
				LOG_ERR("ResizeBuffers failed");
			}

			swap_chain->ResizeBuffers(0, static_cast<cstd::uint32_t>(screen_size.x),
			                          static_cast<cstd::uint32_t>(screen_size.y), DXGI_FORMAT_UNKNOWN, 0);

			create_rtv();

			last_screen_size = screen_size;
		}

		ID3D11RenderTargetView* const tmp_rtv = rtv.value();

		constexpr array_t<float, 4> clear_color = { 0.1f, 0.1f, 0.1f, 1.f };

		context->OMSetRenderTargets(1, &tmp_rtv, nullptr);
		context->ClearRenderTargetView(tmp_rtv, clear_color.data());
		
		renderer->begin_frame(screen_size);

		// red filled rectangle with a basic black dropshadow
		renderer->draw_shadow_rect({ 105.f, 105.f }, { 305.f, 255.f }, { 0.f, 0.f, 0.f, 0.5f }, 17.5f, 20.f, 0.f);
		renderer->draw_rect_filled({ 100.f, 100.f }, { 300.f, 250.f }, { 1.f, 0.f, 0.f, 1.f }, 17.5f);

		// green outlined rectangle with a subtle black glow
		renderer->draw_shadow_rect({ 400.f, 100.f }, { 600.f, 250.f }, { 0.f, 0.f, 0.f, 0.3f }, 8.f, 40.f, 0.f);
		renderer->draw_rect({ 400.f, 100.f }, { 600.f, 250.f }, { 0.f, 1.f, 0.f, 1.f }, 2.f, 8.f);

		// standalone blue shadow rect
		renderer->draw_shadow_rect({ 700.f, 100.f }, { 900.f, 250.f }, { 0.f, 0.5f, 1.f, 0.8f }, 17.5f, 35.f, 3.f);

		// red filled rectangle with a really thick shadow
		renderer->draw_shadow_rect({ 100.f, 350.f }, { 300.f, 500.f }, { 0.f, 0.f, 0.f, 0.6f }, 17.5f, 10.f, 20.f);
		renderer->draw_rect_filled({ 100.f, 350.f }, { 300.f, 500.f }, { 1.f, 0.f, 0.f, 1.f }, 17.5f);

		// green filled rectangle with only the top left and bottom right corners rounded
		constexpr rv::rounding_flags selective_flags = static_cast<rv::rounding_flags>(rv::rounding_flags_top_left | rv::rounding_flags_bottom_right);
		renderer->draw_shadow_rect({ 400.f, 350.f }, { 600.f, 500.f }, { 0.f, 0.f, 0.f, 0.6f }, 30.f, 25.f, 0.f, selective_flags);
		renderer->draw_rect_filled({ 400.f, 350.f }, { 600.f, 500.f }, { 0.f, 1.f, 0.f, 1.f }, 30.f, selective_flags);

		renderer->draw_circle_filled({ 750.f, 400.f }, 50.f, { 1.f, 0.f, 0.f, 1.f });

		if (font)
		{
			const auto state = renderer->state();

			const string_t text = std::format("width {}px height {}px", state.display_size.x, state.display_size.y);

			constexpr float size = 35.f;
			constexpr rv::position text_pos = { 100.f, 580.f};
			const rv::position text_size = renderer->calc_text_size(*font, text, size);

			renderer->draw_rect_filled(text_pos, {text_pos.x + text_size.x, text_pos.y + text_size.y}, {1.f, 0.25f, 0.f, 1.f});
			renderer->draw_text(*font, text_pos, text, { 0.f, 1.f, 1.f, 1.f }, size);
		}

		renderer->end_frame();

		swap_chain->Present(1, 0);

	} while (msg.message != WM_QUIT);

	return 0;
}
