#pragma once
#include "position.hpp"

namespace rv
{
	class renderer;

	class texture
	{
	public:
		explicit texture(renderer* const renderer) noexcept
				:	renderer_(renderer) { }

		[[nodiscard]] bool owned_by(const renderer* const renderer) const noexcept
		{
			return renderer_ == renderer;
		}

	protected:
		renderer* renderer_ = nullptr;
	};

	struct glyph
	{
		texture_position uv0;
		texture_position uv1;

		vector_2d<float> size;
		vector_2d<float> bearing;

		float advance = 0.f;
	};

	class font
	{
	public:
		explicit font(shared_ptr_t<texture> texture, const vector_t<glyph>& glyphs, const cstd::uint32_t min_char, const cstd::uint32_t max_char, const float baked_size,
		              const float ascent, const float line_gap)
				:	texture_(cstd::move(texture)),
					glyphs_(glyphs),
					min_char_(min_char),
					max_char_(max_char),
					baked_size_(baked_size),
					ascent_(ascent),
					line_gap_(line_gap) { }

		[[nodiscard]] const glyph& glyph(const cstd::uint32_t c) const
		{
			if (c < min_char_ || max_char_ < c || glyphs_.empty())
			{
				const cstd::uint32_t fallback = '?' >= min_char_ && '?' <= max_char_ ? '?' : min_char_;
				return glyphs_[static_cast<cstd::size_t>(fallback - min_char_)];
			}

			return glyphs_[static_cast<cstd::size_t>(c - min_char_)];
		}

		[[nodiscard]] shared_ptr_t<texture> texture() const
		{
			return texture_;
		}

		[[nodiscard]] float ascent() const noexcept
		{
			return ascent_;
		}

		[[nodiscard]] float baked_size() const noexcept
		{
			return baked_size_;
		}

		[[nodiscard]] float line_gap() const noexcept
		{
			return line_gap_;
		}

	protected:
		shared_ptr_t<class texture> texture_;

		vector_t<struct glyph> glyphs_;
		cstd::uint32_t min_char_;
		cstd::uint32_t max_char_;
		float baked_size_;
		float ascent_;
		float line_gap_;
	};
}
