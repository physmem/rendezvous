#pragma once
#include "../render.hpp"
#include <d3d11.h>

namespace rv
{
	class renderer;

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

		explicit operator bool() const noexcept
		{
			return value_ != nullptr;
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
		explicit dx11_renderer(ID3D11Device* device, ID3D11DeviceContext* context) noexcept;
		explicit dx11_renderer(IDXGISwapChain* swap_chain) noexcept;

		bool init() noexcept override;
		void begin_frame(vector_2d<float> display_size) noexcept override;
		void end_frame() noexcept override;

	protected:
		bool create_buffer(cstd::size_t vertex_count);
		bool try_widen_buffer();

		void flush_pending_vertices() noexcept override;

		dx11_object<ID3D11Device> device_;
		dx11_object<ID3D11DeviceContext> context_;

		dx11_object<ID3D11PixelShader> pixel_shader_;
		dx11_object<ID3D11VertexShader> vertex_shader_;
		dx11_object<ID3D11InputLayout> input_layout_;
		dx11_object<ID3D11BlendState> blend_state_;
		dx11_object<ID3D11Buffer> buffer_;
	};
}
