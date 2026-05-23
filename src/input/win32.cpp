#include "win32.hpp"
#include <windowsx.h>

bool rv::win32_input::handle_message(const HWND hwnd, const UINT msg, const WPARAM wparam, const LPARAM lparam)
{
	switch (msg)
	{
	case WM_KEYDOWN:
	case WM_SYSKEYDOWN:
		if (wparam < 256)
		{
			if (!state_.keys_down[wparam])
				state_.keys_pressed[wparam] = true;
			state_.keys_down[wparam] = true;
		}
		return true;

	case WM_KEYUP:
	case WM_SYSKEYUP:
		if (wparam < 256)
		{
			if (state_.keys_down[wparam])
				state_.keys_released[wparam] = true;
			state_.keys_down[wparam] = false;
			state_.keys_pressed[wparam] = false;
		}
		return true;

	case WM_LBUTTONDOWN:
	case WM_LBUTTONDBLCLK:
		if (!state_.mouse_down[0]) state_.mouse_clicked[0] = true;
		state_.mouse_down[0] = true;
		return true;

	case WM_LBUTTONUP:
		if (state_.mouse_down[0]) state_.mouse_released[0] = true;
		state_.mouse_down[0] = false;
		state_.mouse_clicked[0] = false;
		return true;

	case WM_RBUTTONDOWN:
	case WM_RBUTTONDBLCLK:
		if (!state_.mouse_down[1]) state_.mouse_clicked[1] = true;
		state_.mouse_down[1] = true;
		return true;

	case WM_RBUTTONUP:
		if (state_.mouse_down[1]) state_.mouse_released[1] = true;
		state_.mouse_down[1] = false;
		state_.mouse_clicked[1] = false;
		return true;

	case WM_MBUTTONDOWN:
	case WM_MBUTTONDBLCLK:
		if (!state_.mouse_down[2]) state_.mouse_clicked[2] = true;
		state_.mouse_down[2] = true;
		return true;

	case WM_MBUTTONUP:
		if (state_.mouse_down[2]) state_.mouse_released[2] = true;
		state_.mouse_down[2] = false;
		state_.mouse_clicked[2] = false;
		return true;

	case WM_MOUSEWHEEL:
		state_.scroll_delta += static_cast<float>(GET_WHEEL_DELTA_WPARAM(wparam)) / static_cast<float>(WHEEL_DELTA);
		return true;

	case WM_MOUSEMOVE:
		state_.mouse_pos.x = static_cast<float>(GET_X_LPARAM(lparam));
		state_.mouse_pos.y = static_cast<float>(GET_Y_LPARAM(lparam));
		return true;
	}

	return false;
}
