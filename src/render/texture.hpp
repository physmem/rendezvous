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
		constexpr static char first_char = 32;
		constexpr static char last_char = 126;
		constexpr static cstd::size_t glyph_count = last_char - first_char + 1;

		explicit font(shared_ptr_t<texture> texture, const array_t<glyph, glyph_count>& glyphs, const float baked_size,
		              const float ascent, const float line_gap)
				:	texture_(cstd::move(texture)),
					glyphs_(glyphs),
					baked_size_(baked_size),
					ascent_(ascent),
					line_gap_(line_gap) { }

		[[nodiscard]] const glyph& glyph(const char c) const
		{
			if (c < first_char || last_char < c)
			{
				return glyphs_[static_cast<cstd::size_t>('?' - first_char)];
			}

			return glyphs_[static_cast<cstd::size_t>(c - first_char)];
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

		array_t<struct glyph, glyph_count> glyphs_;
		float baked_size_;
		float ascent_;
		float line_gap_;
	};
}
