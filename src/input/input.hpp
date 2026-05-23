#pragma once
#include "../util/types.hpp"
#include "../render/position.hpp"

namespace rv
{
	struct position;

	enum class cursor_type : cstd::uint8_t
	{
		none = 0,
		arrow,
		text_input,
		resize_all,
		resize_ns,
		resize_ew,
		resize_nesw,
		resize_nwse,
		hand,
		not_allowed
	};

	struct input_state
	{
		constexpr static cstd::int32_t key_count = 256;
		constexpr static cstd::int32_t button_count = 5;

		array_t<bool, key_count> keys_down = {};
		array_t<bool, key_count> keys_pressed = {};
		array_t<bool, key_count> keys_released = {};
		array_t<bool, button_count> mouse_down = {};
		array_t<bool, button_count> mouse_clicked = {};
		array_t<bool, button_count> mouse_released = {};
		position mouse_pos = {};
		float scroll_delta = 0.f;
	};

	class input
	{
	public:
		using key_type = std::int32_t;
		using button_type = std::int32_t;

		void reset()
		{
			state_.keys_pressed.fill(false);
			state_.keys_released.fill(false);
			state_.mouse_clicked.fill(false);
			state_.mouse_released.fill(false);
			state_.scroll_delta = 0.f;
		}

		[[nodiscard]] cursor_type get_cursor() const noexcept
		{
			return current_cursor_;
		}

		void set_cursor(const cursor_type cursor) noexcept
		{
			current_cursor_ = cursor;
		}

		[[nodiscard]] bool is_key_down(const key_type key) const noexcept
		{
			return key_in_range(key) && state_.keys_down[key];
		}

		[[nodiscard]] bool is_key_pressed(const key_type key) const noexcept
		{
			return key_in_range(key) && state_.keys_pressed[key];
		}

		[[nodiscard]] bool is_key_released(const key_type key) const noexcept
		{
			return key_in_range(key) && state_.keys_released[key];
		}

		[[nodiscard]] bool is_mouse_down(const button_type button) const noexcept
		{
			return button_in_range(button) && state_.mouse_down[button];
		}

		[[nodiscard]] bool is_mouse_clicked(const button_type button) const noexcept
		{
			return button_in_range(button) && state_.mouse_clicked[button];
		}

		[[nodiscard]] bool is_mouse_released(const button_type button) const noexcept
		{
			return button_in_range(button) && state_.mouse_released[button];
		}

		[[nodiscard]] position mouse_pos() const noexcept
		{
			return state_.mouse_pos;
		}

		[[nodiscard]] float scroll_delta() const noexcept
		{
			return state_.scroll_delta;
		}

	protected:
		[[nodiscard]] static bool key_in_range(const key_type key) noexcept
		{
			return 0 <= key && key < input_state::key_count;
		}

		[[nodiscard]] static bool button_in_range(const button_type button) noexcept
		{
			return 0 <= button && button < input_state::button_count;
		}

		input_state state_;
		cursor_type current_cursor_ = cursor_type::arrow;
	};
}
