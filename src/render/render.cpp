#include "render.hpp"
#include "texture.hpp"

#ifdef RV_USE_FREETYPE
#include <ft2build.h>
#include FT_FREETYPE_H
#else
#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>
#endif

#include "../util/file.hpp"

bool rv::renderer::init() 
{
	if (!init_backend()) 
	{
		return false;
	}

	constexpr array_t<const cstd::uint8_t, 4> white = { 0xFF, 0xFF, 0xFF, 0xFF };

	default_texture_ = create_texture(white, 1, 1);
	current_texture_ = default_texture_;

	return static_cast<bool>(default_texture_);
}

void rv::renderer::draw_vertices(const span_t<const vertex> vertices, const shader_type shader) noexcept 
{
	if (vertices.empty()) 
	{
		return;
	}

	if (pending_batches_.empty() || current_texture_ != pending_batches_.back().texture || 
		pending_batches_.back().shader != shader) {
		pending_batches_.push_back(vertex_batch{ static_cast<cstd::uint32_t>(pending_vertices_.size()), 0, 
			current_texture_, shader });
	}

	auto& current_batch = pending_batches_.back();

	current_batch.vertex_count += static_cast<cstd::uint32_t>(vertices.size());

	pending_vertices_.insert(pending_vertices_.end(), vertices.begin(), vertices.end());
}

void rv::renderer::draw_rect(const position min, const position max, const color col, const float thickness, const float rounding) noexcept 
{
	if (rounding < 0.5f) 
	{
		draw_rect_filled(min, { max.x, min.y + thickness }, col);
		draw_rect_filled({ min.x, max.y - thickness }, max, col);
		draw_rect_filled(min, { min.x + thickness, max.y }, col);
		draw_rect_filled({ max.x - thickness, min.y }, max, col);

		return;
	}

	const float r = rounding;

	constexpr float pi = cstd::numbers::pi_f;

	add_arc_path({ min.x + r, min.y + r }, r, pi, pi * 1.5f);
	add_arc_path({ max.x - r, min.y + r }, r, pi * 1.5f, pi * 2.f);
	add_arc_path({ max.x - r, max.y - r }, r, 0.f, pi * 0.5f);
	add_arc_path({ min.x + r, max.y - r }, r, pi * 0.5f, pi);

	draw_lined_path(col, thickness, true);
}

void rv::renderer::draw_rect_filled(const position min, const position max, const color col, const float rounding, const rounding_flags flags) noexcept 
{
	const float width = max.x - min.x;
	const float height = max.y - min.y;

	const float cx = min.x + width * 0.5f;
	const float cy = min.y + height * 0.5f;

	const float qw = (width * 0.5f) + 1.f;
	const float qh = (height * 0.5f) + 1.f;

	const position p0 = { cx - qw, cy - qh };
	const position p1 = { cx + qw, cy + qh };

	const ndc_position n0 = to_ndc(p0);
	const ndc_position n1 = to_ndc(p1);

	const float rtl = (flags & rounding_flags_top_left) ? rounding : 0.f;
	const float rtr = (flags & rounding_flags_top_right) ? rounding : 0.f;
	const float rbr = (flags & rounding_flags_bottom_right) ? rounding : 0.f;
	const float rbl = (flags & rounding_flags_bottom_left) ? rounding : 0.f;

	const array_t<float, 8> data = { width, height, 0.f, 0.f, rtr, rbr, rbl, rtl };

	const auto make_vertex = [col, data](const float x, const float y, const float u, const float v) -> vertex 
	{
		return vertex{ .pos = { x, y }, .col = col, .uv = { u, v }, .custom_data = data };
	};

	const array_t<vertex, 6> vertices =
	{
		make_vertex(n0.x, n0.y, -qw, -qh),
		make_vertex(n1.x, n0.y,  qw, -qh),
		make_vertex(n0.x, n1.y, -qw,  qh),
		make_vertex(n1.x, n0.y,  qw, -qh),
		make_vertex(n1.x, n1.y,  qw,  qh),
		make_vertex(n0.x, n1.y, -qw,  qh),
	};

	draw_vertices(vertices, shader_type::rect_shader);
}

void rv::renderer::draw_shadow_rect(const position min, const position max, const color col, const float rounding, const float shadow_blur, const float shadow_spread, const rounding_flags flags) noexcept 
{
	const float width = max.x - min.x;
	const float height = max.y - min.y;

	const float effective_width = cstd::fmaxf(0.f, width + 2.f * shadow_spread);
	const float effective_height = cstd::fmaxf(0.f, height + 2.f * shadow_spread);
	const float effective_rounding = cstd::fmaxf(0.f, rounding + shadow_spread);

	const float cx = min.x + width * 0.5f;
	const float cy = min.y + height * 0.5f;

	const float qw = (effective_width * 0.5f) + shadow_blur;
	const float qh = (effective_height * 0.5f) + shadow_blur;

	const position p0 = { cx - qw, cy - qh };
	const position p1 = { cx + qw, cy + qh };

	const ndc_position n0 = to_ndc(p0);
	const ndc_position n1 = to_ndc(p1);

	const float rtl = (flags & rounding_flags_top_left) ? effective_rounding : 0.f;
	const float rtr = (flags & rounding_flags_top_right) ? effective_rounding : 0.f;
	const float rbr = (flags & rounding_flags_bottom_right) ? effective_rounding : 0.f;
	const float rbl = (flags & rounding_flags_bottom_left) ? effective_rounding : 0.f;

	const array_t<float, 8> data = { effective_width, effective_height, 0.f, shadow_blur, rtr, rbr, rbl, rtl };

	const auto make_vertex = [col, data](const float x, const float y, const float u, const float v) -> vertex 
	{
		return vertex{ .pos = { x, y }, .col = col, .uv = { u, v }, .custom_data = data };
	};

	const array_t<vertex, 6> vertices =
	{
		make_vertex(n0.x, n0.y, -qw, -qh),
		make_vertex(n1.x, n0.y,  qw, -qh),
		make_vertex(n0.x, n1.y, -qw,  qh),
		make_vertex(n1.x, n0.y,  qw, -qh),
		make_vertex(n1.x, n1.y,  qw,  qh),
		make_vertex(n0.x, n1.y, -qw,  qh),
	};

	draw_vertices(vertices, shader_type::shadow_shader);
}

void rv::renderer::draw_line(const position a, const position b, const color col, const float thickness) noexcept 
{
	add_path_point(a);
	add_path_point(b);

	draw_lined_path(col, thickness, false);
}

void rv::renderer::draw_circle(const position pos, const float radius, const color col, const float thickness,
							   const cstd::size_t segment_count) noexcept 
{
	add_circle_path(pos, radius, segment_count);

	draw_lined_path(col, thickness, true);
}

void rv::renderer::draw_circle_filled(const position pos, const float radius, const color col,
									  const cstd::size_t segment_count) noexcept 
{
	add_circle_path(pos, radius, segment_count);

	draw_filled_path(col);
}

void rv::renderer::draw_text(const font& font, const position pos, const string_view_t text, const color col,
							 const float size) noexcept 
{
	const shared_ptr_t<texture> font_texture = font.texture();

	if (text.empty() || !font_texture) 
	{
		return;
	}

	current_texture_ = font_texture;

	const float scale = size != 0.f ? size / font.baked_size() : 1.f;
	const float baseline = pos.y + font.ascent() * scale;
	float pen = pos.x;

	for (const char c : text) 
	{
		const glyph& g = font.glyph(c);

		if (g.size.x > 0.f && g.size.y > 0.f) 
		{
			const float x0 = pen + g.bearing.x * scale;
			const float y0 = baseline - g.bearing.y * scale;
			const ndc_position a = to_ndc({ x0, y0 });
			const ndc_position b = to_ndc({ x0 + g.size.x * scale, y0 + g.size.y * scale });

			const auto make_vertex = [col](const float x, const float y, const float u, const float w) -> vertex 
			{
				return vertex{ .pos = { x, y }, .col = col, .uv = { u, w } };
			};

			const array_t<vertex, 6> vertices =
			{
				make_vertex(a.x, a.y, g.uv0.x, g.uv0.y),
				make_vertex(b.x, a.y, g.uv1.x, g.uv0.y),
				make_vertex(a.x, b.y, g.uv0.x, g.uv1.y),
				make_vertex(b.x, a.y, g.uv1.x, g.uv0.y),
				make_vertex(b.x, b.y, g.uv1.x, g.uv1.y),
				make_vertex(a.x, b.y, g.uv0.x, g.uv1.y),
			};
			draw_vertices(vertices);
		}

		pen += g.advance * scale;
	}

	current_texture_ = default_texture_;
}

rv::position rv::renderer::calc_text_size(const font& font, const string_view_t text, const float size) const noexcept 
{
	if (text.empty() || !font.texture()) 
	{
		return { 0.f, 0.f };
	}

	const float scale = size != 0.f ? size / font.baked_size() : 1.f;
	float width = 0.f;

	for (const char c : text) 
	{
		const glyph& g = font.glyph(c);
		width += g.advance * scale;
	}

	return { width, font.baked_size() * scale };
}

optional_t<rv::font> rv::renderer::add_font(const span_t<const cstd::uint8_t> bytes, const float pixel_height) 
{
#ifdef RV_USE_FREETYPE
	FT_Library library = nullptr;

	if (FT_Init_FreeType(&library)) 
	{
		return { };
	}

	FT_Face face = nullptr;

	if (FT_New_Memory_Face(library, bytes.data(), static_cast<FT_Long>(bytes.size()), 0, &face)) 
	{
		FT_Done_FreeType(library);

		return { };
	}

	FT_Set_Pixel_Sizes(face, 0, static_cast<FT_UInt>(pixel_height));

	cstd::int32_t ascent = static_cast<cstd::int32_t>(face->size->metrics.ascender >> 6);
	cstd::int32_t line_gap = static_cast<cstd::int32_t>((face->size->metrics.height - face->size->metrics.ascender + face->size->metrics.descender) >> 6);

	constexpr cstd::uint32_t width = 512;
	constexpr cstd::uint32_t height = 512;

	vector_t<cstd::uint8_t> coverage(static_cast<cstd::size_t>(width) * height, 0);

	array_t<glyph, font::glyph_count> glyphs = { };

	cstd::uint32_t pen_x = 0;
	cstd::uint32_t pen_y = 0;
	cstd::uint32_t row_height = 0;

	for (cstd::size_t i = 0; i < font::glyph_count; i++) 
	{
		const char c = static_cast<char>(font::first_char + i);

		if (FT_Load_Char(face, c, FT_LOAD_RENDER)) 
		{
			continue;
		}

		FT_Bitmap* bitmap = &face->glyph->bitmap;

		if (pen_x + bitmap->width >= width) 
		{
			pen_x = 0;
			pen_y += row_height + 1;
			row_height = 0;
		}

		if (bitmap->rows > row_height) 
		{
			row_height = bitmap->rows;
		}

		if (pen_y + bitmap->rows >= height) 
		{
			break;
		}

		for (cstd::uint32_t row = 0; row < bitmap->rows; row++) 
		{
			for (cstd::uint32_t col = 0; col < bitmap->width; col++) 
			{
				coverage[(pen_y + row) * width + (pen_x + col)] = bitmap->buffer[row * bitmap->pitch + col];
			}
		}

		glyph& g = glyphs[i];

		g.uv0 = { static_cast<float>(pen_x) / width, static_cast<float>(pen_y) / height };
		g.uv1 = { static_cast<float>(pen_x + bitmap->width) / width, static_cast<float>(pen_y + bitmap->rows) / height };
		g.size = { static_cast<float>(bitmap->width), static_cast<float>(bitmap->rows) };
		g.bearing = { static_cast<float>(face->glyph->bitmap_left), static_cast<float>(face->glyph->bitmap_top) };
		g.advance = static_cast<float>(face->glyph->advance.x >> 6);

		pen_x += bitmap->width + 1;
	}

	vector_t<cstd::uint8_t> rgba(static_cast<cstd::size_t>(width) * height * 4, 0);

	for (cstd::size_t i = 0; i < coverage.size(); i++) 
	{
		const cstd::size_t c = i * 4;

		rgba[c] = 0xFF;
		rgba[c + 1] = 0xFF;
		rgba[c + 2] = 0xFF;
		rgba[c + 3] = coverage[i];
	}

	const auto texture = create_texture(rgba, width, height);

	FT_Done_Face(face);
	FT_Done_FreeType(library);

	if (!texture) 
	{
		return { };
	}

	return font{ texture, glyphs, pixel_height, static_cast<float>(ascent), static_cast<float>(line_gap) };
#else
	stbtt_fontinfo info = { };

	if (!stbtt_InitFont(&info, bytes.data(), stbtt_GetFontOffsetForIndex(bytes.data(), 0))) 
	{
		return { };
	}

	const float scale = stbtt_ScaleForPixelHeight(&info, pixel_height);

	cstd::int32_t ascent = 0;
	cstd::int32_t descent = 0;
	cstd::int32_t line_gap = 0;

	stbtt_GetFontVMetrics(&info, &ascent, &descent, &line_gap);

	constexpr cstd::uint32_t width = 512;
	constexpr cstd::uint32_t height = 512;

	vector_t<cstd::uint8_t> coverage(static_cast<cstd::size_t>(width) * height, 0);

	array_t<stbtt_bakedchar, font::glyph_count> baked = { };

	if (!stbtt_BakeFontBitmap(bytes.data(), 0, pixel_height, coverage.data(), width, height, font::first_char,
		font::glyph_count, baked.data()))
	{
		return { };
	}

	vector_t<cstd::uint8_t> rgba(static_cast<cstd::size_t>(width) * height * 4, 0);

	for (cstd::size_t i = 0; i < coverage.size(); i++) {
		const cstd::size_t c = i * 4;

		rgba[c] = 0xFF;
		rgba[c + 1] = 0xFF;
		rgba[c + 2] = 0xFF;
		rgba[c + 3] = coverage[i];
	}

	const auto texture = create_texture(rgba, width, height);

	if (!texture) 
	{
		return { };
	}

	array_t<glyph, font::glyph_count> glyphs;

	for (cstd::size_t i = 0; i < font::glyph_count; i++) 
	{
		const stbtt_bakedchar& b = baked[i];
		glyph& g = glyphs[i];

		g.uv0 = { static_cast<float>(b.x0) / width, static_cast<float>(b.y0) / height };
		g.uv1 = { static_cast<float>(b.x1) / width, static_cast<float>(b.y1) / height };
		g.size = { static_cast<float>(b.x1 - b.x0), static_cast<float>(b.y1 - b.y0) };
		g.bearing = { b.xoff, -b.yoff };
		g.advance = b.xadvance;
	}

	return font{ texture, glyphs, pixel_height, static_cast<float>(ascent) * scale, static_cast<float>(line_gap) * scale };
#endif
}

optional_t<rv::font> rv::renderer::add_font(const string_t& path, const float pixel_height) 
{
	const vector_t<std::uint8_t> buffer = read_file(path);

	return add_font(buffer, pixel_height);
}

void rv::renderer::add_path_point(const position pos) 
{
	path_points_.push_back(pos);
}

void rv::renderer::draw_lined_path(const color col, const float thickness, const bool closed) 
{
	const cstd::size_t n = path_points_.size();

	if (n <= 1) 
	{
		path_points_.clear();

		return;
	}

	constexpr float fringe_width = 1.f;

	const float half = thickness * 0.5f;
	const color transparent{ col.r, col.g, col.b, 0.f };

	const auto make_join = [](const position previous, const position current, const position next) -> position 
	{
		const auto dir_in = (current - previous).normalise();
		const auto dir_out = (next - current).normalise();

		const auto in_eff = dir_in ? dir_in : dir_out;
		const auto out_eff = dir_out ? dir_out : dir_in;

		const auto tangent = (in_eff + out_eff).normalise();
		const auto normal = tangent.perpendicular();

		const float c = tangent.dot(out_eff);
		const float scale = 1.f / cstd::fmaxf(c, 0.1f);

		const auto [x, y] = normal * scale;

		return { x, y };
	};

	const auto draw_segment = [&](const position pos_a, const position join_a, const position pos_b, const position join_b) 
	{
		const auto core_a = join_a * half;
		const auto outer_a = join_a * (half + fringe_width);
		const auto core_b = join_b * half;
		const auto outer_b = join_b * (half + fringe_width);

		const ndc_position a_outer_fringe = to_ndc({ pos_a.x + outer_a.x, pos_a.y + outer_a.y });
		const ndc_position a_outer_opaque = to_ndc({ pos_a.x + core_a.x,  pos_a.y + core_a.y });
		const ndc_position a_inner_opaque = to_ndc({ pos_a.x - core_a.x,  pos_a.y - core_a.y });
		const ndc_position a_inner_fringe = to_ndc({ pos_a.x - outer_a.x, pos_a.y - outer_a.y });

		const ndc_position b_outer_fringe = to_ndc({ pos_b.x + outer_b.x, pos_b.y + outer_b.y });
		const ndc_position b_outer_opaque = to_ndc({ pos_b.x + core_b.x,  pos_b.y + core_b.y });
		const ndc_position b_inner_opaque = to_ndc({ pos_b.x - core_b.x,  pos_b.y - core_b.y });
		const ndc_position b_inner_fringe = to_ndc({ pos_b.x - outer_b.x, pos_b.y - outer_b.y });

		const array_t<vertex, 18> vertices =
		{
			vertex{.pos = a_outer_fringe, .col = transparent },
			vertex{.pos = b_outer_fringe, .col = transparent },
			vertex{.pos = a_outer_opaque, .col = col },
			vertex{.pos = b_outer_fringe, .col = transparent },
			vertex{.pos = b_outer_opaque, .col = col },
			vertex{.pos = a_outer_opaque, .col = col },

			vertex{.pos = a_outer_opaque, .col = col },
			vertex{.pos = b_outer_opaque, .col = col },
			vertex{.pos = a_inner_opaque, .col = col },
			vertex{.pos = b_outer_opaque, .col = col },
			vertex{.pos = b_inner_opaque, .col = col },
			vertex{.pos = a_inner_opaque, .col = col },

			vertex{.pos = a_inner_opaque, .col = col },
			vertex{.pos = b_inner_opaque, .col = col },
			vertex{.pos = a_inner_fringe, .col = transparent },
			vertex{.pos = b_inner_opaque, .col = col },
			vertex{.pos = b_inner_fringe, .col = transparent },
			vertex{.pos = a_inner_fringe, .col = transparent },
		};

		draw_vertices(vertices);
	};

	const auto prev_of = [&](const cstd::size_t i) -> position 
	{
		if (0 < i) {
			return path_points_[i - 1];
		}

		return closed ? path_points_[n - 1] : path_points_[i];
	};

	const auto next_of = [&](const cstd::size_t i) -> position 
	{
		if (i + 1 < n) {
			return path_points_[i + 1];
		}

		return closed ? path_points_[0] : path_points_[i];
	};

	const position first_pos = path_points_[0];
	const position first_join = make_join(prev_of(0), first_pos, next_of(0));

	position previous_pos = first_pos;
	position previous_join = first_join;

	for (cstd::size_t i = 1; i < n; i++) 
	{
		const position current_pos = path_points_[i];
		const position current_join = make_join(prev_of(i), current_pos, next_of(i));

		draw_segment(previous_pos, previous_join, current_pos, current_join);

		previous_pos = current_pos;
		previous_join = current_join;
	}

	if (closed) 
	{
		draw_segment(previous_pos, previous_join, first_pos, first_join);
	}

	path_points_.clear();
}

void rv::renderer::draw_filled_path(const color col) 
{
	if (path_points_.size() <= 1) 
	{
		path_points_.clear();

		return;
	}

	const ndc_position first_point = to_ndc(path_points_[0]);

	for (cstd::size_t i = 1; i < path_points_.size() - 1; i++) 
	{
		const ndc_position point = to_ndc(path_points_[i]);
		const ndc_position next_point = to_ndc(path_points_[i + 1]);

		const array_t<vertex, 3> vertices =
		{
			vertex{.pos = first_point, .col = col },
			vertex{.pos = point, .col = col },
			vertex{.pos = next_point, .col = col },
		};

		draw_vertices(vertices);
	}

	draw_lined_path(col, 0.f);

	path_points_.clear();
}

void rv::renderer::add_arc_path(const position pos, const float radius, const float a_min, const float a_max,
								const cstd::size_t segment_count) noexcept {
	for (cstd::size_t i = 0; i < segment_count; i++) 
	{
		const float a = a_min + (static_cast<float>(i) / static_cast<float>(segment_count)) * (a_max - a_min);

		add_path_point({ pos.x + cstd::cosf(a) * radius, pos.y + cstd::sinf(a) * radius });
	}
}

void rv::renderer::add_circle_path(const position pos, const float radius, const cstd::size_t segment_count) noexcept 
{
	add_arc_path(pos, radius, 0.f, cstd::numbers::pi_f * 2.f, static_cast<cstd::int32_t>(segment_count));
}

rv::ndc_position rv::renderer::to_ndc(const position pos) const noexcept 
{
	return
	{
		2.f * (pos.x - 0.5f) / display_size_.x - 1.f,
		1.f - 2.f * (pos.y - 0.5f) / display_size_.y
	};
}
