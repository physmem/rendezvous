#include "win32.hpp"
#include <windowsx.h>

namespace
{
	struct state_t
	{
		array_t<bool, 256> keys_down = {};
		array_t<bool, 256> keys_pressed = {};
		array_t<bool, 256> keys_released = {};
		array_t<bool, 5> mouse_down = {};
		array_t<bool, 5> mouse_clicked = {};
		array_t<bool, 5> mouse_released = {};
		rv::vector_2d<float> mouse_pos = {};
		float scroll_delta = 0.f;
	} state;
}

namespace rv::backend::win32
{
	bool handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		switch (msg)
		{
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
			if (wparam < 256)
			{
				if (!state.keys_down[wparam])
					state.keys_pressed[wparam] = true;
				state.keys_down[wparam] = true;
			}
			return true;

		case WM_KEYUP:
		case WM_SYSKEYUP:
			if (wparam < 256)
			{
				if (state.keys_down[wparam])
					state.keys_released[wparam] = true;
				state.keys_down[wparam] = false;
				state.keys_pressed[wparam] = false;
			}
			return true;

		case WM_LBUTTONDOWN:
		case WM_LBUTTONDBLCLK:
			if (!state.mouse_down[0]) state.mouse_clicked[0] = true;
			state.mouse_down[0] = true;
			return true;

		case WM_LBUTTONUP:
			if (state.mouse_down[0]) state.mouse_released[0] = true;
			state.mouse_down[0] = false;
			state.mouse_clicked[0] = false;
			return true;

		case WM_RBUTTONDOWN:
		case WM_RBUTTONDBLCLK:
			if (!state.mouse_down[1]) state.mouse_clicked[1] = true;
			state.mouse_down[1] = true;
			return true;

		case WM_RBUTTONUP:
			if (state.mouse_down[1]) state.mouse_released[1] = true;
			state.mouse_down[1] = false;
			state.mouse_clicked[1] = false;
			return true;

		case WM_MBUTTONDOWN:
		case WM_MBUTTONDBLCLK:
			if (!state.mouse_down[2]) state.mouse_clicked[2] = true;
			state.mouse_down[2] = true;
			return true;

		case WM_MBUTTONUP:
			if (state.mouse_down[2]) state.mouse_released[2] = true;
			state.mouse_down[2] = false;
			state.mouse_clicked[2] = false;
			return true;

		case WM_MOUSEWHEEL:
			state.scroll_delta += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / static_cast<float>(WHEEL_DELTA);
			return true;

		case WM_MOUSEMOVE:
			state.mouse_pos.x = static_cast<float>(GET_X_LPARAM(lparam));
			state.mouse_pos.y = static_cast<float>(GET_Y_LPARAM(lparam));
			return true;
		}

		return false;
	}

	void update()
	{
		state.keys_pressed.fill(false);
		state.keys_released.fill(false);
		state.mouse_clicked.fill(false);
		state.mouse_released.fill(false);
		state.scroll_delta = 0.f;
	}

	bool is_key_down(cstd::int32_t key)
	{
		if (key < 0 || key >= 256) return false;
		return state.keys_down[key];
	}

	bool is_key_pressed(cstd::int32_t key)
	{
		if (key < 0 || key >= 256) return false;
		return state.keys_pressed[key];
	}

	bool is_key_released(cstd::int32_t key)
	{
		if (key < 0 || key >= 256) return false;
		return state.keys_released[key];
	}

	bool is_mouse_down(cstd::int32_t button)
	{
		if (button < 0 || button >= 5) return false;
		return state.mouse_down[button];
	}

	bool is_mouse_clicked(cstd::int32_t button)
	{
		if (button < 0 || button >= 5) return false;
		return state.mouse_clicked[button];
	}

	bool is_mouse_released(cstd::int32_t button)
	{
		if (button < 0 || button >= 5) return false;
		return state.mouse_released[button];
	}

	vector_2d<float> get_mouse_pos()
	{
		return state.mouse_pos;
	}

	float get_scroll_delta()
	{
		return state.scroll_delta;
	}
}
