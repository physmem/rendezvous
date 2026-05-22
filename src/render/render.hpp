#pragma once
#include "position.hpp"

// define this to use freetype font rendering
// #define RV_USE_FREETYPE

namespace rv 
{
	class texture;
	class font;

	enum rounding_flags : cstd::uint32_t 
	{
		rounding_flags_none = 0,
		rounding_flags_top_left = 1 << 0,
		rounding_flags_top_right = 1 << 1,
		rounding_flags_bottom_right = 1 << 2,
		rounding_flags_bottom_left = 1 << 3,
		rounding_flags_all = rounding_flags_top_left | rounding_flags_top_right | rounding_flags_bottom_right | rounding_flags_bottom_left
	};

	struct vertex 
	{
		ndc_position pos;
		color col;
		texture_position uv;
		array_t<float, 8> custom_data;
	};

	enum class shader_type : cstd::uint8_t
	{
		default_shader,
		shadow_shader,
		rect_shader
	};

	struct vertex_batch 
	{
		cstd::uint32_t vertex_offset;
		cstd::uint32_t vertex_count;
		shared_ptr_t<texture> texture;
		shader_type shader;
	};

	class renderer 
	{
	public:
		bool init();

		virtual void begin_frame(vector_2d<float> display_size) noexcept = 0;
		virtual void end_frame() noexcept = 0;

		void draw_vertices(span_t<const vertex> vertices, shader_type shader = shader_type::default_shader) noexcept;

		void draw_rect(position min, position max, color col, float thickness = 1.f, float rounding = 0.f) noexcept;
		void draw_rect_filled(position min, position max, color col, float rounding = 0.f, rounding_flags flags = rounding_flags_all) noexcept;
		void draw_shadow_rect(position min, position max, color col, float rounding = 0.f, float shadow_blur = 15.f, float shadow_spread = 0.f, rounding_flags flags = rounding_flags_all) noexcept;

		void draw_line(position a, position b, color col, float thickness = 1.f) noexcept;

		void draw_circle(position pos, float radius, color col, float thickness = 1.f,
						 cstd::size_t segment_count = 32) noexcept;

		void draw_circle_filled(position pos, float radius, color col, cstd::size_t segment_count = 32) noexcept;

		void draw_text(const font& font, position pos, string_view_t text, color col, float size = 0.f) noexcept;
		position calc_text_size(const font& font, string_view_t text, float size = 0.f) const noexcept;

		optional_t<font> add_font(span_t<const cstd::uint8_t> bytes, float pixel_height = 16.f);
		optional_t<font> add_font(const string_t& path, float pixel_height = 16.f);

	protected:
		virtual bool init_backend() noexcept = 0;
		virtual void flush_pending_vertices() noexcept = 0;
		virtual shared_ptr_t<texture> create_texture(span_t<const cstd::uint8_t> buffer, cstd::uint32_t width, cstd::uint32_t height) = 0;

		void add_arc_path(position pos, float radius, float a_min, float a_max, cstd::size_t segment_count = 8) noexcept;
		void add_circle_path(position pos, float radius, cstd::size_t segment_count) noexcept;

		void add_path_point(position pos);
		void draw_lined_path(color col, float thickness = 1.f, bool closed = true);
		void draw_filled_path(color col);

		[[nodiscard]] ndc_position to_ndc(position pos) const noexcept;

		vector_2d<float> display_size_ = { };
		vector_t<position> path_points_ = { };
		vector_t<vertex> pending_vertices_ = { };
		vector_t<vertex_batch> pending_batches_ = { };
		shared_ptr_t<texture> current_texture_ = { };
		shared_ptr_t<texture> default_texture_ = { };

		cstd::size_t buffer_vertex_count_ = 0;
	};
}
