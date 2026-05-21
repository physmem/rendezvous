#pragma once
#include "../util/math.hpp"

namespace rv
{
	struct color
	{
		float r, g, b, a;
	};

	struct position : vector_2d<float>
	{

	};

	struct ndc_position : vector_2d<float>
	{
		
	};

	struct vertex
	{
		ndc_position pos;
		color col;
	};

	class renderer
	{
	public:
		virtual bool init() noexcept = 0;
		virtual void begin_frame(vector_2d<float> display_size) noexcept = 0;
		virtual void end_frame() noexcept = 0;

		void draw_vertices(span_t<const vertex> vertices) noexcept;

		void draw_rect(position min, position max, color col, float thickness = 1.f, float rounding = 0.f) noexcept;
		void draw_rect_filled(position min, position max, color col, float rounding = 0.f) noexcept;

		void draw_line(position a, position b, color col, float thickness = 1.f) noexcept;

		void draw_circle(position pos, float radius, color col, float thickness = 1.f,
		                 cstd::size_t segment_count = 32) noexcept;

		void draw_circle_filled(position pos, float radius, color col, cstd::size_t segment_count = 32) noexcept;

	protected:
		virtual void flush_pending_vertices() noexcept = 0;

		void add_arc_path(position pos, float radius, float a_min, float a_max, cstd::size_t segment_count = 8) noexcept;
		void add_circle_path(position pos, float radius, cstd::size_t segment_count) noexcept;

		void add_path_point(position pos);
		void draw_lined_path(color col, float thickness = 1.f, bool closed = true);
		void draw_filled_path(color col);

		[[nodiscard]] ndc_position to_ndc(position pos) const noexcept;

		vector_2d<float> display_size_ = { };
		vector_t<position> path_points_ = { };
		vector_t<vertex> pending_vertices_ = { };

		cstd::size_t buffer_vertex_count_ = 0;
	};
}
