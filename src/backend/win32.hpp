#pragma once
#include <windows.h>
#include "../util/types.hpp"
#include "../util/math.hpp"

namespace rv::backend::win32
{
	bool handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	void update();

	bool is_key_down(cstd::int32_t key);
	bool is_key_pressed(cstd::int32_t key);
	bool is_key_released(cstd::int32_t key);

	bool is_mouse_down(cstd::int32_t button);
	bool is_mouse_clicked(cstd::int32_t button);
	bool is_mouse_released(cstd::int32_t button);

	vector_2d<float> get_mouse_pos();
	float get_scroll_delta();
}
