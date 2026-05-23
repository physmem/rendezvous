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
#include "../util/triangulate.hpp"

namespace
{
	cstd::uint32_t decode_utf8(const char*& s, const char* end) noexcept
	{
		cstd::uint32_t codepoint = 0;
		const cstd::uint8_t c0 = static_cast<cstd::uint8_t>(*s);
		
		if (c0 < 0x80)
		{
			codepoint = c0;
			s++;
		}
		else if ((c0 & 0xE0) == 0xC0 && s + 1 < end)
		{
			codepoint = ((c0 & 0x1F) << 6) | (static_cast<cstd::uint8_t>(s[1]) & 0x3F);
			s += 2;
		}
		else if ((c0 & 0xF0) == 0xE0 && s + 2 < end)
		{
			codepoint = ((c0 & 0x0F) << 12) | ((static_cast<cstd::uint8_t>(s[1]) & 0x3F) << 6) | (static_cast<cstd::uint8_t>(s[2]) & 0x3F);
			s += 3;
		}
		else if ((c0 & 0xF8) == 0xF0 && s + 3 < end)
		{
			codepoint = ((c0 & 0x07) << 18) | ((static_cast<cstd::uint8_t>(s[1]) & 0x3F) << 12) | ((static_cast<cstd::uint8_t>(s[2]) & 0x3F) << 6) | (static_cast<cstd::uint8_t>(s[3]) & 0x3F);
			s += 4;
		}
		else
		{
			codepoint = '?';
			s++;
		}

		return codepoint;
	}
}

bool rv::renderer::init() 
{
	if (!init_backend()) 
	{
		return false;
	}

	constexpr array_t<const cstd::uint8_t, 4> white = { 0xFF, 0xFF, 0xFF, 0xFF };

	default_texture_ = create_texture(white, 1, 1);
	current_texture_ = default_texture_;

	state_.last_time = cstd::get_time();

	return static_cast<bool>(default_texture_);
}

void rv::renderer::begin_frame(const vector_2d<float> display_size) noexcept
{
	const time_point_t current_time = cstd::get_time();

	state_.delta_time = cstd::get_time_diff(current_time, state_.last_time);
	state_.time += state_.delta_time;

	state_.last_time = current_time;
	state_.display_size = display_size;

	begin_frame_backend(display_size);
}

void rv::renderer::push_clip_rect(const position min, const position max) noexcept
{
	clip_rects_.push_back({ min, max });
}

void rv::renderer::pop_clip_rect() noexcept
{
	if (!clip_rects_.empty())
	{
		clip_rects_.pop_back();
	}
}

void rv::renderer::draw_vertices(const span_t<const vertex> vertices, const shader_type shader) noexcept 
{
	if (vertices.empty()) 
	{
		return;
	}

	const optional_t<rect> current_clip = clip_rects_.empty() ? optional_t<rect>() : clip_rects_.back();

	if (pending_batches_.empty() || current_texture_ != pending_batches_.back().texture || 
		pending_batches_.back().shader != shader || pending_batches_.back().clip_rect != current_clip) {
		pending_batches_.push_back(vertex_batch{ static_cast<cstd::uint32_t>(pending_vertices_.size()), 0, 
			static_cast<cstd::uint32_t>(pending_indices_.size()), 0, current_texture_, shader, current_clip });
	}

	auto& current_batch = pending_batches_.back();
	const cstd::uint32_t index_shift = current_batch.vertex_count;
	const cstd::uint32_t count = static_cast<cstd::uint32_t>(vertices.size());

	current_batch.vertex_count += count;
	current_batch.index_count += count;

	pending_vertices_.insert(pending_vertices_.end(), vertices.begin(), vertices.end());
	
	for (cstd::uint32_t i = 0; i < count; ++i) {
		pending_indices_.push_back(i + index_shift);
	}
}

void rv::renderer::draw_indexed_vertices(const span_t<const vertex> vertices, const span_t<const cstd::uint32_t> indices, const shader_type shader) noexcept 
{
	if (vertices.empty() || indices.empty()) 
	{
		return;
	}

	const optional_t<rect> current_clip = clip_rects_.empty() ? optional_t<rect>() : clip_rects_.back();

	if (pending_batches_.empty() || current_texture_ != pending_batches_.back().texture || 
		pending_batches_.back().shader != shader || pending_batches_.back().clip_rect != current_clip) 
	{
		pending_batches_.push_back(vertex_batch{ static_cast<cstd::uint32_t>(pending_vertices_.size()), 0, 
			static_cast<cstd::uint32_t>(pending_indices_.size()), 0, current_texture_, shader, current_clip });
	}

	auto& current_batch = pending_batches_.back();
	const cstd::uint32_t index_shift = current_batch.vertex_count;

	current_batch.vertex_count += static_cast<cstd::uint32_t>(vertices.size());
	current_batch.index_count += static_cast<cstd::uint32_t>(indices.size());

	pending_vertices_.insert(pending_vertices_.end(), vertices.begin(), vertices.end());
	
	for (const cstd::uint32_t index : indices) 
	{
		pending_indices_.push_back(index + index_shift);
	}
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

void rv::renderer::draw_shadow_rect(const position min, const position max, const color col, const float rounding, const float shadow_blur, const float shadow_spread, const rounding_flags flags, const bool cut_background) noexcept 
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

	const array_t<float, 8> data = { effective_width, effective_height, cut_background ? 1.f : 0.f, shadow_blur, rtr, rbr, rbl, rtl };

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

void rv::renderer::draw_shadow_line(const position a, const position b, const color col, const float thickness, const float shadow_blur) noexcept 
{
	add_path_point(a);
	add_path_point(b);

	draw_shadow_lined_path(col, thickness, shadow_blur, false);
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

void rv::renderer::draw_shadow_circle(const position pos, const float radius, const color col, const float shadow_blur, const bool cut_background) noexcept 
{
	const float qw = radius + shadow_blur;
	const float qh = radius + shadow_blur;

	const position p0 = { pos.x - qw, pos.y - qh };
	const position p1 = { pos.x + qw, pos.y + qh };

	const ndc_position n0 = to_ndc(p0);
	const ndc_position n1 = to_ndc(p1);

	const array_t<float, 8> data = { radius * 2.f, radius * 2.f, cut_background ? 1.f : 0.f, shadow_blur, radius, radius, radius, radius };

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

void rv::renderer::draw_image(const shared_ptr_t<texture> tex, const position min, const position max, const color tint) noexcept 
{
	draw_image_rounded(tex, min, max, 0.f, rounding_flags_all, tint);
}

void rv::renderer::draw_image_rounded(const shared_ptr_t<texture> tex, const position min, const position max, const float rounding, const rounding_flags flags, const color tint) noexcept 
{
	const float width = max.x - min.x;
	const float height = max.y - min.y;

	if (!tex || width <= 0.f || height <= 0.f)
	{
		return;
	}

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

	const float u0 = -1.f / width;
	const float u1 = 1.f + 1.f / width;
	const float v0 = -1.f / height;
	const float v1 = 1.f + 1.f / height;

	const auto make_vertex = [tint, data](const float x, const float y, const float u, const float v) -> vertex 
	{
		return vertex{ .pos = { x, y }, .col = tint, .uv = { u, v }, .custom_data = data };
	};

	const array_t<vertex, 6> vertices =
	{
		make_vertex(n0.x, n0.y, u0, v0),
		make_vertex(n1.x, n0.y, u1, v0),
		make_vertex(n0.x, n1.y, u0, v1),
		make_vertex(n1.x, n0.y, u1, v0),
		make_vertex(n1.x, n1.y, u1, v1),
		make_vertex(n0.x, n1.y, u0, v1),
	};

	current_texture_ = tex;
	draw_vertices(vertices, shader_type::image_shader);
	current_texture_ = default_texture_;
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

	const char* s = text.data();
	const char* end = s + text.size();

	while (s < end) 
	{
		const cstd::uint32_t codepoint = decode_utf8(s, end);
		const glyph& g = font.glyph(codepoint);

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

	const char* s = text.data();
	const char* end = s + text.size();

	while (s < end) 
	{
		const cstd::uint32_t codepoint = decode_utf8(s, end);
		const glyph& g = font.glyph(codepoint);
		width += g.advance * scale;
	}

	return { width, font.baked_size() * scale };
}

optional_t<rv::font> rv::renderer::add_font(const span_t<const cstd::uint8_t> bytes, const float pixel_height, const cstd::uint32_t min_char, const cstd::uint32_t max_char, const bool anti_aliased) 
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

	const float estimated_area = static_cast<float>(max_char - min_char + 1) * pixel_height * pixel_height * 1.5f;
	cstd::uint32_t width = 512;
	cstd::uint32_t height = 512;
	while (static_cast<float>(width * height) < estimated_area) 
	{
		width *= 2;
		height *= 2;
	}

	vector_t<cstd::uint8_t> coverage(static_cast<cstd::size_t>(width) * height, 0);

	const cstd::size_t glyph_count = static_cast<cstd::size_t>(max_char - min_char + 1);
	vector_t<glyph> glyphs(glyph_count);

	cstd::uint32_t pen_x = 0;
	cstd::uint32_t pen_y = 0;
	cstd::uint32_t row_height = 0;

	for (cstd::size_t i = 0; i < glyph_count; i++) 
	{
		const cstd::uint32_t c = min_char + static_cast<cstd::uint32_t>(i);

		const cstd::int32_t load_flags = anti_aliased ? FT_LOAD_RENDER : (FT_LOAD_RENDER | FT_LOAD_TARGET_MONO);
		if (FT_Load_Char(face, c, load_flags)) 
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
				if (bitmap->pixel_mode == FT_PIXEL_MODE_MONO)
				{
					const cstd::uint8_t byte = bitmap->buffer[row * bitmap->pitch + (col / 8)];
					const bool bit = (byte & (1 << (7 - (col % 8)))) != 0;
					coverage[(pen_y + row) * width + (pen_x + col)] = bit ? 0xFF : 0;
				}
				else
				{
					coverage[(pen_y + row) * width + (pen_x + col)] = bitmap->buffer[row * bitmap->pitch + col];
				}
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

	return font{ texture, glyphs, min_char, max_char, pixel_height, static_cast<float>(ascent), static_cast<float>(line_gap) };
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

	const float estimated_area = static_cast<float>(max_char - min_char + 1) * pixel_height * pixel_height * 1.5f;
	cstd::uint32_t width = 512;
	cstd::uint32_t height = 512;
	while (static_cast<float>(width * height) < estimated_area) 
	{
		width *= 2;
		height *= 2;
	}

	vector_t<cstd::uint8_t> coverage(static_cast<cstd::size_t>(width) * height, 0);

	const cstd::size_t glyph_count = static_cast<cstd::size_t>(max_char - min_char + 1);
	vector_t<stbtt_bakedchar> baked(glyph_count);

	const int baked_count = stbtt_BakeFontBitmap(bytes.data(), 0, pixel_height, coverage.data(), width, height, static_cast<int>(min_char), static_cast<int>(glyph_count), baked.data());
	if (baked_count <= 0)
	{
		return { };
	}

	if (!anti_aliased)
	{
		for (cstd::size_t i = 0; i < coverage.size(); i++)
		{
			coverage[i] = coverage[i] > 127 ? 0xFF : 0;
		}
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

	vector_t<glyph> glyphs(glyph_count);

	for (cstd::size_t i = 0; i < glyph_count; i++) 
	{
		const stbtt_bakedchar& b = baked[i];
		glyph& g = glyphs[i];

		g.uv0 = { static_cast<float>(b.x0) / width, static_cast<float>(b.y0) / height };
		g.uv1 = { static_cast<float>(b.x1) / width, static_cast<float>(b.y1) / height };
		g.size = { static_cast<float>(b.x1 - b.x0), static_cast<float>(b.y1 - b.y0) };
		g.bearing = { b.xoff, -b.yoff };
		g.advance = b.xadvance;
	}

	return font{ texture, glyphs, min_char, max_char, pixel_height, static_cast<float>(ascent) * scale, static_cast<float>(line_gap) * scale };
#endif
}

optional_t<rv::font> rv::renderer::add_font(const string_t& path, const float pixel_height, const cstd::uint32_t min_char, const cstd::uint32_t max_char, const bool anti_aliased) 
{
	const vector_t<std::uint8_t> buffer = read_file(path);

	return add_font(buffer, pixel_height, min_char, max_char, anti_aliased);
}

rv::state& rv::renderer::state() noexcept
{
	return state_;
}

const rv::state& rv::renderer::state() const noexcept
{
	return state_;
}

cstd::size_t rv::renderer::vertex_count() const noexcept
{
	return pending_vertices_.size();
}

void rv::renderer::modify_alpha(const cstd::size_t start_idx, const cstd::size_t end_idx, const float alpha) noexcept
{
	if (alpha >= 1.0f)
	{
		return;
	}

	for (cstd::size_t i = start_idx; i < end_idx && i < pending_vertices_.size(); i++)
	{
		pending_vertices_[i].col.a *= alpha;
	}
}

void rv::renderer::add_path_point(const position pos) 
{
	path_points_.push_back(pos);
}

void rv::renderer::draw_lined_path(const color col, const float thickness, const bool closed, const float fringe_width, const cap_style cap, const join_style join) 
{
	const cstd::size_t n = path_points_.size();

	if (n <= 1) 
	{
		path_points_.clear();

		return;
	}

	const float half = thickness * 0.5f;
	const color transparent{ col.r, col.g, col.b, 0.f };

	const auto make_join = [join](const position previous, const position current, const position next) -> position 
	{
		const auto dir_in = (current - previous).normalise();
		const auto dir_out = (next - current).normalise();

		const auto in_eff = dir_in ? dir_in : dir_out;
		const auto out_eff = dir_out ? dir_out : dir_in;

		const auto tangent = (in_eff + out_eff).normalise();
		const auto normal = tangent.perpendicular();

		const float c = tangent.dot(out_eff);
		const float scale = (join == join_style::bevel) ? 1.0f : (1.f / cstd::fmaxf(c, 0.25f));

		const auto [x, y] = normal * scale;

		return { x, y };
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

	vector_t<vertex> vertices;
	vertices.reserve(n * 4);

	for (cstd::size_t i = 0; i < n; i++) 
	{
		const position current_pos = path_points_[i];
		const position current_join = make_join(prev_of(i), current_pos, next_of(i));

		const auto core = current_join * half;
		const auto outer = current_join * (half + fringe_width);

		vertices.push_back(vertex{ .pos = to_ndc({ current_pos.x + outer.x, current_pos.y + outer.y }), .col = transparent });
		vertices.push_back(vertex{ .pos = to_ndc({ current_pos.x + core.x,  current_pos.y + core.y }),  .col = col });
		vertices.push_back(vertex{ .pos = to_ndc({ current_pos.x - core.x,  current_pos.y - core.y }),  .col = col });
		vertices.push_back(vertex{ .pos = to_ndc({ current_pos.x - outer.x, current_pos.y - outer.y }), .col = transparent });
	}

	vector_t<cstd::uint32_t> indices;
	const cstd::size_t segments = closed ? n : n - 1;
	indices.reserve(segments * 18);

	for (cstd::size_t i = 0; i < segments; i++) 
	{
		const cstd::uint32_t idx = static_cast<cstd::uint32_t>(i * 4);
		const cstd::uint32_t nxt = static_cast<cstd::uint32_t>(((i + 1) % n) * 4);

		indices.push_back(idx); indices.push_back(nxt); indices.push_back(nxt + 1);
		indices.push_back(idx); indices.push_back(nxt + 1); indices.push_back(idx + 1);

		indices.push_back(idx + 1); indices.push_back(nxt + 1); indices.push_back(nxt + 2);
		indices.push_back(idx + 1); indices.push_back(nxt + 2); indices.push_back(idx + 2);

		indices.push_back(idx + 2); indices.push_back(nxt + 2); indices.push_back(nxt + 3);
		indices.push_back(idx + 2); indices.push_back(nxt + 3); indices.push_back(idx + 3);
	}

	if (!closed)
	{
		if (cap == cap_style::round)
		{
			const cstd::uint32_t cap_segments = 8;
			
			auto build_cap = [&](const position p, const auto dir, const cstd::uint32_t v_outer_start, const cstd::uint32_t v_core_start, const cstd::uint32_t v_outer_end, const cstd::uint32_t v_core_end)
			{
				const float phi = std::atan2f(dir.y, dir.x);
				const float theta_start = phi + cstd::numbers::pi_f / 2.f;
				
				const cstd::uint32_t center_idx = static_cast<cstd::uint32_t>(vertices.size());
				vertices.push_back(vertex{ .pos = to_ndc(p), .col = col });
				
				cstd::uint32_t prev_outer_idx = v_outer_start;
				cstd::uint32_t prev_core_idx = v_core_start;
				
				for (cstd::uint32_t j = 1; j <= cap_segments; j++)
				{
					cstd::uint32_t cur_outer_idx, cur_core_idx;
					
					if (j == cap_segments)
					{
						cur_outer_idx = v_outer_end;
						cur_core_idx = v_core_end;
					}
					else
					{
						const float a = theta_start + (static_cast<float>(j) / static_cast<float>(cap_segments)) * cstd::numbers::pi_f;
						const float cx = cstd::cosf(a);
						const float cy = cstd::sinf(a);
						
						cur_outer_idx = static_cast<cstd::uint32_t>(vertices.size());
						vertices.push_back(vertex{ .pos = to_ndc({ p.x + cx * (half + fringe_width), p.y + cy * (half + fringe_width) }), .col = transparent });
						
						cur_core_idx = static_cast<cstd::uint32_t>(vertices.size());
						vertices.push_back(vertex{ .pos = to_ndc({ p.x + cx * half, p.y + cy * half }), .col = col });
					}
					
					indices.push_back(prev_core_idx); indices.push_back(cur_core_idx); indices.push_back(center_idx);
					indices.push_back(center_idx); indices.push_back(cur_core_idx); indices.push_back(prev_core_idx);
					
					indices.push_back(prev_outer_idx); indices.push_back(cur_outer_idx); indices.push_back(cur_core_idx);
					indices.push_back(cur_core_idx); indices.push_back(cur_outer_idx); indices.push_back(prev_outer_idx);
					
					indices.push_back(prev_outer_idx); indices.push_back(cur_core_idx); indices.push_back(prev_core_idx);
					indices.push_back(prev_core_idx); indices.push_back(cur_core_idx); indices.push_back(prev_outer_idx);
					
					prev_outer_idx = cur_outer_idx;
					prev_core_idx = cur_core_idx;
				}
			};

			// start cap
			const position p0 = path_points_[0];
			const position p1 = path_points_[1];
			build_cap(p0, (p1 - p0).normalise(), 3, 2, 0, 1);

			// end cap
			const position pe = path_points_[n - 1];
			const position p_prev = path_points_[n - 2];
			const cstd::uint32_t base = static_cast<cstd::uint32_t>((n - 1) * 4);
			build_cap(pe, (p_prev - pe).normalise(), base + 0, base + 1, base + 3, base + 2);
		}
		else
		{
			// flat caps
			const position p0 = path_points_[0];
			const position p1 = path_points_[1];
			
			auto dir_start = (p1 - p0).normalise();
			auto norm_start = dir_start.perpendicular();

			const auto core_s = norm_start * half;
			const auto outer_s = norm_start * (half + fringe_width);

			const position cap_p0 = { p0.x - dir_start.x * fringe_width, p0.y - dir_start.y * fringe_width };

			const cstd::uint32_t v_start = static_cast<cstd::uint32_t>(vertices.size());
			vertices.push_back(vertex{ .pos = to_ndc({ cap_p0.x + outer_s.x, cap_p0.y + outer_s.y }), .col = transparent });
			vertices.push_back(vertex{ .pos = to_ndc({ cap_p0.x + core_s.x,  cap_p0.y + core_s.y }),  .col = transparent });
			vertices.push_back(vertex{ .pos = to_ndc({ cap_p0.x - core_s.x,  cap_p0.y - core_s.y }),  .col = transparent });
			vertices.push_back(vertex{ .pos = to_ndc({ cap_p0.x - outer_s.x, cap_p0.y - outer_s.y }), .col = transparent });

			const cstd::uint32_t idx = v_start;
			const cstd::uint32_t nxt = 0;

			// push both windings to ensure it renders regardless of the culling mode
			indices.push_back(idx); indices.push_back(nxt); indices.push_back(nxt + 1);
			indices.push_back(idx); indices.push_back(nxt + 1); indices.push_back(nxt);
			indices.push_back(idx); indices.push_back(nxt + 1); indices.push_back(idx + 1);
			indices.push_back(idx); indices.push_back(idx + 1); indices.push_back(nxt + 1);

			indices.push_back(idx + 1); indices.push_back(nxt + 1); indices.push_back(nxt + 2);
			indices.push_back(idx + 1); indices.push_back(nxt + 2); indices.push_back(nxt + 1);
			indices.push_back(idx + 1); indices.push_back(nxt + 2); indices.push_back(idx + 2);
			indices.push_back(idx + 1); indices.push_back(idx + 2); indices.push_back(nxt + 2);

			indices.push_back(idx + 2); indices.push_back(nxt + 2); indices.push_back(nxt + 3);
			indices.push_back(idx + 2); indices.push_back(nxt + 3); indices.push_back(nxt + 2);
			indices.push_back(idx + 2); indices.push_back(nxt + 3); indices.push_back(idx + 3);
			indices.push_back(idx + 2); indices.push_back(idx + 3); indices.push_back(nxt + 3);

			// end cap
			const position pe = path_points_[n - 1];
			const position p_prev = path_points_[n - 2];
			auto dir_end = (pe - p_prev).normalise();
			auto norm_end = dir_end.perpendicular();

			const auto core_e = norm_end * half;
			const auto outer_e = norm_end * (half + fringe_width);

			const position cap_pe = { pe.x + dir_end.x * fringe_width, pe.y + dir_end.y * fringe_width };

			const cstd::uint32_t v_end = static_cast<cstd::uint32_t>(vertices.size());
			vertices.push_back(vertex{ .pos = to_ndc({ cap_pe.x + outer_e.x, cap_pe.y + outer_e.y }), .col = transparent });
			vertices.push_back(vertex{ .pos = to_ndc({ cap_pe.x + core_e.x,  cap_pe.y + core_e.y }),  .col = transparent });
			vertices.push_back(vertex{ .pos = to_ndc({ cap_pe.x - core_e.x,  cap_pe.y - core_e.y }),  .col = transparent });
			vertices.push_back(vertex{ .pos = to_ndc({ cap_pe.x - outer_e.x, cap_pe.y - outer_e.y }), .col = transparent });

			const cstd::uint32_t idx_end = static_cast<cstd::uint32_t>((n - 1) * 4);
			const cstd::uint32_t nxt_end = v_end;

			indices.push_back(idx_end); indices.push_back(nxt_end); indices.push_back(nxt_end + 1);
			indices.push_back(idx_end); indices.push_back(nxt_end + 1); indices.push_back(nxt_end);
			indices.push_back(idx_end); indices.push_back(nxt_end + 1); indices.push_back(idx_end + 1);
			indices.push_back(idx_end); indices.push_back(idx_end + 1); indices.push_back(nxt_end + 1);

			indices.push_back(idx_end + 1); indices.push_back(nxt_end + 1); indices.push_back(nxt_end + 2);
			indices.push_back(idx_end + 1); indices.push_back(nxt_end + 2); indices.push_back(nxt_end + 1);
			indices.push_back(idx_end + 1); indices.push_back(nxt_end + 2); indices.push_back(idx_end + 2);
			indices.push_back(idx_end + 1); indices.push_back(idx_end + 2); indices.push_back(nxt_end + 2);

			indices.push_back(idx_end + 2); indices.push_back(nxt_end + 2); indices.push_back(nxt_end + 3);
			indices.push_back(idx_end + 2); indices.push_back(nxt_end + 3); indices.push_back(nxt_end + 2);
			indices.push_back(idx_end + 2); indices.push_back(nxt_end + 3); indices.push_back(idx_end + 3);
			indices.push_back(idx_end + 2); indices.push_back(idx_end + 3); indices.push_back(nxt_end + 3);
		}
	}

	draw_indexed_vertices(vertices, indices);

	path_points_.clear();
}

void rv::renderer::draw_filled_path(const color col, const float fringe_width) 
{
	if (path_points_.size() <= 2) 
	{
		path_points_.clear();

		return;
	}

	const vector_t<cstd::uint32_t> indices = triangulate::execute(path_points_);

	if (!indices.empty())
	{
		vector_t<vertex> vertices;
		vertices.reserve(path_points_.size());

		for (const position& p : path_points_)
		{
			vertices.push_back(vertex{ .pos = to_ndc(p), .col = col });
		}

		draw_indexed_vertices(vertices, indices);
	}

	draw_lined_path(col, 0.f, true, fringe_width);

	path_points_.clear();
}

void rv::renderer::draw_shadow_lined_path(const color col, const float thickness, const float shadow_blur, const bool closed) 
{
	draw_lined_path(col, thickness, closed, shadow_blur, cap_style::round, join_style::bevel);
}

void rv::renderer::draw_shadow_filled_path(const color col, const float shadow_blur) 
{
	const cstd::size_t n = path_points_.size();
	if (n <= 2) 
	{
		path_points_.clear();
		return;
	}

	const vector_t<cstd::uint32_t> core_indices = triangulate::execute(path_points_);
	if (!core_indices.empty())
	{
		vector_t<vertex> core_vertices;
		core_vertices.reserve(n);
		for (const position& p : path_points_)
		{
			core_vertices.push_back(vertex{ .pos = to_ndc(p), .col = col });
		}
		draw_indexed_vertices(core_vertices, core_indices);
	}

	if (shadow_blur > 0.f)
	{
		float area = 0.f;
		for (cstd::size_t i = 0; i < n; i++) 
		{
			const auto p0 = path_points_[i];
			const auto p1 = path_points_[(i + 1) % n];
			area += (p1.x - p0.x) * (p1.y + p0.y);
		}
			const bool is_cw = area < 0.f;

		vector_t<vertex> vertices;
		vector_t<cstd::uint32_t> indices;
		const color transparent{ col.r, col.g, col.b, 0.f };

		for (cstd::size_t i = 0; i < n; i++)
		{
			const position p0 = path_points_[i];
			const position p1 = path_points_[(i + 1) % n];
			
			auto dir = (p1 - p0).normalise();
			auto norm = dir.perpendicular();
			if (!is_cw) { norm.x = -norm.x; norm.y = -norm.y; }

			const cstd::uint32_t base = static_cast<cstd::uint32_t>(vertices.size());
			vertices.push_back(vertex{ .pos = to_ndc(p0), .col = col });
			vertices.push_back(vertex{ .pos = to_ndc({ p0.x + norm.x * shadow_blur, p0.y + norm.y * shadow_blur }), .col = transparent });
			vertices.push_back(vertex{ .pos = to_ndc(p1), .col = col });
			vertices.push_back(vertex{ .pos = to_ndc({ p1.x + norm.x * shadow_blur, p1.y + norm.y * shadow_blur }), .col = transparent });

			if (is_cw)
			{
				indices.push_back(base + 0); indices.push_back(base + 1); indices.push_back(base + 2);
				indices.push_back(base + 1); indices.push_back(base + 3); indices.push_back(base + 2);
			}
			else
			{
				indices.push_back(base + 0); indices.push_back(base + 2); indices.push_back(base + 1);
				indices.push_back(base + 1); indices.push_back(base + 2); indices.push_back(base + 3);
			}

			const position p_prev = path_points_[(i + n - 1) % n];
			auto dir_prev = (p0 - p_prev).normalise();
			auto norm_prev = dir_prev.perpendicular();
			if (!is_cw) { norm_prev.x = -norm_prev.x; norm_prev.y = -norm_prev.y; }

			const float cross = dir_prev.x * dir.y - dir_prev.y * dir.x;
			const bool is_convex = is_cw ? (cross > 0.f) : (cross < 0.f);

			if (is_convex)
			{
				float a0 = cstd::atan2f(norm_prev.y, norm_prev.x);
				float a1 = cstd::atan2f(norm.y, norm.x);
				
				if (is_cw && a1 < a0) a1 += cstd::numbers::pi_f * 2.f;
				if (!is_cw && a1 > a0) a1 -= cstd::numbers::pi_f * 2.f;

				const cstd::uint32_t cap_segments = 5;
				const cstd::uint32_t center_idx = static_cast<cstd::uint32_t>(vertices.size());
				vertices.push_back(vertex{ .pos = to_ndc(p0), .col = col });

				cstd::uint32_t prev_arc_idx = static_cast<cstd::uint32_t>(vertices.size());
				vertices.push_back(vertex{ .pos = to_ndc({ p0.x + norm_prev.x * shadow_blur, p0.y + norm_prev.y * shadow_blur }), .col = transparent });

				for (cstd::uint32_t j = 1; j <= cap_segments; j++)
				{
					const float a = a0 + (a1 - a0) * (static_cast<float>(j) / static_cast<float>(cap_segments));
					const float cx = cstd::cosf(a);
					const float cy = cstd::sinf(a);

					const cstd::uint32_t cur_arc_idx = static_cast<cstd::uint32_t>(vertices.size());
					vertices.push_back(vertex{ .pos = to_ndc({ p0.x + cx * shadow_blur, p0.y + cy * shadow_blur }), .col = transparent });

					if (is_cw)
					{
						indices.push_back(center_idx); indices.push_back(prev_arc_idx); indices.push_back(cur_arc_idx);
					}
					else
					{
						indices.push_back(center_idx); indices.push_back(cur_arc_idx); indices.push_back(prev_arc_idx);
					}

					prev_arc_idx = cur_arc_idx;
				}
			}
		}
		draw_indexed_vertices(vertices, indices);
	}
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
		2.f * (pos.x - 0.5f) / state_.display_size.x - 1.f,
		1.f - 2.f * (pos.y - 0.5f) / state_.display_size.y
	};
}
