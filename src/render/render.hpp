#pragma once
#include <d3d11.h>
#include "../util/types.hpp"

namespace rv
{
	struct color
	{
		float r, g, b, a;
	};

	template <class T>
	struct vector_2d
	{
		T x, y;
	};

	struct position : vector_2d<float>
	{
		
	};

	struct vertex
	{
		position pos;
		color col;
	};

	class renderer
	{
	public:
		virtual bool init() noexcept = 0;
		virtual void begin_frame(vector_2d<float> display_size) noexcept = 0;
		virtual void end_frame() noexcept = 0;

		virtual void draw_vertices(span_t<const vertex> vertices) noexcept = 0;

		void draw_rect(position min, position max, color col) noexcept;

	protected:
		[[nodiscard]] vector_2d<float> to_ndc(position pos) const;

		vector_2d<float> display_size_ = { };
	};

	template <class T>
	class dx11_object
	{
	public:
		dx11_object() noexcept = default;
		explicit dx11_object(T* value_ptr) noexcept
				:	value_(value_ptr) { }

		dx11_object(const dx11_object&) = delete;
		dx11_object& operator=(const dx11_object&) = delete;

		dx11_object(dx11_object&& other) noexcept
				:	value_(other.value_)
		{
			other.value_ = nullptr;
		}

		dx11_object& operator=(dx11_object&& other) noexcept
		{
			if (this != &other)
			{
				release();

				value_ = other.value_;
				other.value_ = nullptr;
			}

			return *this;
		}

		[[nodiscard]] T* value() const noexcept
		{
			return value_;
		}

		[[nodiscard]] T* operator->() const noexcept
		{
			return value_;
		}

		void release()
		{
			if (value_)
			{
				value_->Release();

				value_ = nullptr;
			}
		}

		[[nodiscard]] T** release_and_get() noexcept
		{
			release();

			return &value_;
		}

		~dx11_object()
		{
			release();
		}

	protected:
		T* value_ = nullptr;
	};

	class dx11_renderer : public renderer
	{
	public:
		dx11_renderer(ID3D11Device* device, ID3D11DeviceContext* context)
				:	device_(device),
					context_(context) { }

		bool init() noexcept override;
		void begin_frame(vector_2d<float> display_size) noexcept override;
		void end_frame() noexcept override;

		void draw_vertices(span_t<const vertex> vertices) noexcept override;

	protected:
		constexpr static cstd::size_t buffer_vertex_count = 8;

		ID3D11Device* device_;
		ID3D11DeviceContext* context_;

		dx11_object<ID3D11PixelShader> pixel_shader_;
		dx11_object<ID3D11VertexShader> vertex_shader_;
		dx11_object<ID3D11InputLayout> input_layout_;
		dx11_object<ID3D11BlendState> blend_state_;
		dx11_object<ID3D11Buffer> buffer_;
	};
}
