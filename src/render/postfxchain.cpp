#include <gallium/render/postfxchain.h>

#include <gallium/gpu/device.h>
#include <gallium/gpu/commandbuffer.h>
#include <gallium/gpu/descriptorregistry.h>
#include <gallium/gpu/image.h>

#include <stdexcept>

using namespace ga::render;

PostFXChain::PostFXChain(ga::gpu::Device& gpu)
	: m_gpu(gpu)
{
}

PostFXChain::~PostFXChain()
{
}

void PostFXChain::Execute(ga::gpu::Device& gpu, const ga::gpu::CommandEncoder& encoder, ga::gpu::Image& input, ga::gpu::Image& output, const std::vector<ga::gpu::Image*>& renderImages)
{
	encoder.PushLabel("PostFX Chain");

	if (!m_ping || m_ping->Size() != input.Size())
	{
		if (m_ping)
			gpu.Defer([a = std::move(m_ping), b = std::move(m_pong)] {});

		auto pingPongImageDesc = ga::gpu::ImageDesc {
			.type   = ga::gpu::EImageType::Image2D,
			.format = ga::gpu::EFormat::R16G16B16A16_SFloat,
			.width  = input.Size().x,
			.height = input.Size().y,
			.usage  = ga::gpu::EImageUsage::Sampled | ga::gpu::EImageUsage::Storage
		};

		m_ping = std::make_unique<ga::gpu::Image>(gpu, pingPongImageDesc);
		m_ping->SetDescriptorIndex(gpu.GetDescriptorRegistry().Register(*m_ping));

		m_pong = std::make_unique<ga::gpu::Image>(gpu, pingPongImageDesc);
		m_pong->SetDescriptorIndex(gpu.GetDescriptorRegistry().Register(*m_pong));
	}

	if (m_entries.empty())
	{
		encoder.Transition(input,  gpu::EImageLayout::TransferSrcOptimal);
		encoder.Transition(output, gpu::EImageLayout::TransferDstOptimal);

		encoder.Transfer([&](const ga::gpu::TransferEncoder& transfer) {
			transfer.BlitImage(input, output, glm::uvec3(0), glm::uvec3(0), glm::min(input.Size(), output.Size()));
		});
	}

	for (size_t i = 0; i < m_entries.size(); ++i)
	{
		auto& in  = i ? *m_ping : input;
		auto& out = (i == m_entries.size() - 1) ? output : *m_pong;

		m_entries[i].effect.Execute(encoder, in, out, renderImages, m_entries[i].data.data());
		std::swap(m_ping, m_pong);
	}

	encoder.PopLabel();
	Reset();
}

void PostFXChain::Reset()
{
	m_entries.clear();
}
