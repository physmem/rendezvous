#include "render.hpp"
#include "shaders.hpp"
#include "../util/types.hpp"

void rv::renderer::draw_vertices(const span_t<const vertex> vertices) noexcept
{
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

void rv::renderer::draw_rect_filled(const position min, const position max, const color col, const float rounding) noexcept
{
	if (rounding < 0.5f)
	{
		const auto [x0, y0] = to_ndc(min);
		const auto [x1, y1] = to_ndc(max);

		const auto make_vertex = [col](const float x, const float y) -> vertex
			{
				return vertex{ .pos = { x, y }, .col = col };
			};

		const array_t<vertex, 6> vertices =
		{
			make_vertex(x0, y0), make_vertex(x1, y0), make_vertex(x0, y1),
			make_vertex(x1, y0), make_vertex(x1, y1), make_vertex(x0, y1),
		};

		draw_vertices(vertices);

		return;
	}

	constexpr float pi = cstd::numbers::pi_f;

	const float r = rounding;

	add_arc_path({ min.x + r, min.y + r }, r, pi, pi * 1.5f);
	add_arc_path({ max.x - r, min.y + r }, r, pi * 1.5f, pi * 2.f);
	add_arc_path({ max.x - r, max.y - r }, r, 0.f, pi * 0.5f);
	add_arc_path({ min.x + r, max.y - r }, r, pi * 0.5f, pi);

	draw_filled_path(col);
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
				vertex{ .pos = a_outer_fringe, .col = transparent },
				vertex{ .pos = b_outer_fringe, .col = transparent },
				vertex{ .pos = a_outer_opaque, .col = col },
				vertex{ .pos = b_outer_fringe, .col = transparent },
				vertex{ .pos = b_outer_opaque, .col = col },
				vertex{ .pos = a_outer_opaque, .col = col },

				vertex{ .pos = a_outer_opaque, .col = col },
				vertex{ .pos = b_outer_opaque, .col = col },
				vertex{ .pos = a_inner_opaque, .col = col },
				vertex{ .pos = b_outer_opaque, .col = col },
				vertex{ .pos = b_inner_opaque, .col = col },
				vertex{ .pos = a_inner_opaque, .col = col },

				vertex{ .pos = a_inner_opaque, .col = col },
				vertex{ .pos = b_inner_opaque, .col = col },
				vertex{ .pos = a_inner_fringe, .col = transparent },
				vertex{ .pos = b_inner_opaque, .col = col },
				vertex{ .pos = b_inner_fringe, .col = transparent },
				vertex{ .pos = a_inner_fringe, .col = transparent },
			};

			draw_vertices(vertices);
		};

	const auto prev_of = [&](const cstd::size_t i) -> position
		{
			if (0 < i)
			{
				return path_points_[i - 1];
			}

			return closed ? path_points_[n - 1] : path_points_[i];
		};

	const auto next_of = [&](const cstd::size_t i) -> position
		{
			if (i + 1 < n)
			{
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
			vertex{ .pos = first_point, .col = col },
			vertex{ .pos = point, .col = col },
			vertex{ .pos = next_point, .col = col },
		};

		draw_vertices(vertices);
	}

	draw_lined_path(col, 0.f);

	path_points_.clear();
}

void rv::renderer::add_arc_path(const position pos, const float radius, const float a_min, const float a_max,
                                const cstd::size_t segment_count) noexcept
{
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
