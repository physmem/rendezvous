#include "render.hpp"
#include "shaders.hpp"
#include "../util/types.hpp"

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

	if (FAILED(device_->CreateInputLayout(layout.data(), layout.size(), d3d11_vertex_shader.data(), d3d11_vertex_shader.size(), input_layout_.release_and_get())))
	{
		return false;
	}

	D3D11_BUFFER_DESC buffer_desc = { };

	buffer_desc.ByteWidth = sizeof(vertex) * buffer_vertex_count;
	buffer_desc.Usage = D3D11_USAGE_DYNAMIC;
	buffer_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	buffer_desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

	if (FAILED(device_->CreateBuffer(&buffer_desc, nullptr, buffer_.release_and_get())))
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
