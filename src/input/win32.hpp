#pragma once
#include "input.hpp"
#include <windows.h>

namespace rv
{
	class win32_input : public input
	{
	public:
		bool handle_message(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam);
	};
}
