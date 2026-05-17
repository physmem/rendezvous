#include "render.hpp"
#include "shaders.hpp"
#include "../util/types.hpp"

void rv::renderer::draw_vertices(const span_t<const vertex> vertices) noexcept
{
	pending_vertices_.insert(pending_vertices_.end(), vertices.begin(), vertices.end());
}

void rv::renderer::draw_rect(const position min, const position max, const color col) noexcept
{
	const auto [x0, y0] = to_ndc(min);
	const auto [x1, y1] = to_ndc(max);

	const auto make_vertex = [col](const float x, const float y) -> vertex
		{
			return vertex{ .pos = { x, y }, .col = col };
		};

	const array_t<vertex, 8> vertices =
	{
		make_vertex(x0, y0), make_vertex(x1, y0),
		make_vertex(x1, y0), make_vertex(x1, y1),
		make_vertex(x1, y1), make_vertex(x0, y1),
		make_vertex(x0, y1), make_vertex(x0, y0),
	};

	draw_vertices(vertices);
}

rv::vector_2d<float> rv::renderer::to_ndc(const position pos) const
{
	return
	{
		.x = 2.f * (pos.x - 0.5f) / display_size_.x - 1.f,
		.y = 1.f - 2.f * (pos.y - 0.5f) / display_size_.y
	};
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
	context_->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

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
