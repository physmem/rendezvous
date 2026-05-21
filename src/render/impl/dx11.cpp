#include "dx11.hpp"
#include "../shaders.hpp"

rv::dx11_renderer::dx11_renderer(ID3D11Device* const device, ID3D11DeviceContext* const context) noexcept
		:	device_(device),
			context_(context)
{
	device_->AddRef();
	context_->AddRef();
}

rv::dx11_renderer::dx11_renderer(IDXGISwapChain* swap_chain) noexcept
{
	if (swap_chain->GetDevice(IID_PPV_ARGS(device_.release_and_get())) == S_OK)
	{
		device_->GetImmediateContext(context_.release_and_get());
	}
}

bool rv::dx11_renderer::init() noexcept
{
	if (!device_ || !context_)
	{
		return false;
	}

	if (FAILED(device_->CreateVertexShader(d3d11_vertex_shader.data(), d3d11_vertex_shader.size(), nullptr, vertex_shader_.release_and_get())) ||
		FAILED(device_->CreatePixelShader(d3d11_pixel_shader.data(), d3d11_pixel_shader.size(), nullptr, pixel_shader_.release_and_get())))
	{
		return false;
	}

	static constexpr array_t<D3D11_INPUT_ELEMENT_DESC, 2> layout =
	{
		D3D11_INPUT_ELEMENT_DESC{ "POSITION", 0, DXGI_FORMAT_R32G32_FLOAT,    0, offsetof(vertex, pos),  D3D11_INPUT_PER_VERTEX_DATA, 0 },
		D3D11_INPUT_ELEMENT_DESC{ "COLOR",    0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(vertex, col), D3D11_INPUT_PER_VERTEX_DATA, 0}
	};

	if (FAILED(device_->CreateInputLayout(layout.data(), static_cast<cstd::uint32_t>(layout.size()), d3d11_vertex_shader.data(), d3d11_vertex_shader.size(), input_layout_.release_and_get())))
	{
		return false;
	}

	D3D11_BLEND_DESC blend_desc = { };

	auto& render_target = blend_desc.RenderTarget[0];

	render_target.BlendEnable = true;
	render_target.SrcBlend = D3D11_BLEND_SRC_ALPHA;
	render_target.DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
	render_target.BlendOp = D3D11_BLEND_OP_ADD;
	render_target.SrcBlendAlpha = D3D11_BLEND_ONE;
	render_target.DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
	render_target.BlendOpAlpha = D3D11_BLEND_OP_ADD;
	render_target.RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

	return SUCCEEDED(device_->CreateBlendState(&blend_desc, blend_state_.release_and_get()));
}

bool rv::dx11_renderer::create_buffer(const cstd::size_t vertex_count)
{
	D3D11_BUFFER_DESC buffer_desc = { };

	buffer_desc.ByteWidth = sizeof(vertex) * static_cast<cstd::uint32_t>(vertex_count);
	buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	if (FAILED(device_->CreateBuffer(&buffer_desc, nullptr, buffer_.release_and_get())))
	{
		return false;
	}

	buffer_vertex_count_ = vertex_count;

	return true;
}

bool rv::dx11_renderer::try_widen_buffer()
{
	if (pending_vertices_.size() <= buffer_vertex_count_)
	{
		return true;
	}

	constexpr cstd::size_t additional_vertices = 256;

	return create_buffer(pending_vertices_.size() + additional_vertices);
}

void rv::dx11_renderer::begin_frame(const vector_2d<float> display_size) noexcept
{
	D3D11_VIEWPORT viewport = { };

	viewport.Width = display_size.x;
	viewport.Height = display_size.y;
	viewport.MinDepth = 0.f;
	viewport.MaxDepth = 1.f;

	context_->RSSetViewports(1, &viewport);
	context_->IASetInputLayout(input_layout_.value());

	context_->VSSetShader(vertex_shader_.value(), nullptr, 0);
	context_->PSSetShader(pixel_shader_.value(), nullptr, 0);
	context_->GSSetShader(nullptr, nullptr, 0);
	context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

	constexpr array_t<float, 4> blend_factor = { };

	context_->OMSetBlendState(blend_state_.value(), blend_factor.data(), 0xffffffff);

	display_size_ = display_size;
}

void rv::dx11_renderer::end_frame() noexcept
{
	flush_pending_vertices();
}

void rv::dx11_renderer::flush_pending_vertices() noexcept
{
	if (pending_vertices_.empty())
	{
		return;
	}

	D3D11_MAPPED_SUBRESOURCE resource = { };

	if (!try_widen_buffer() || FAILED(context_->Map(buffer_.value(), 0, D3D11_MAP_WRITE_DISCARD, 0, &resource)))
	{
		pending_vertices_.clear();

		return;
	}

	cstd::memcpy(resource.pData, pending_vertices_.data(), pending_vertices_.size() * sizeof(vertex));

	context_->Unmap(buffer_.value(), 0);

	constexpr cstd::uint32_t strides = sizeof(vertex);
	constexpr cstd::uint32_t offsets = 0;

	ID3D11Buffer* buffer = buffer_.value();

	context_->IASetVertexBuffers(0, 1, &buffer, &strides, &offsets);
	context_->Draw(static_cast<cstd::int32_t>(pending_vertices_.size()), 0);

	pending_vertices_.clear();
}
