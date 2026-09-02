#ifndef GALLIUM__RENDER__POSTFXCHAIN_H
#define GALLIUM__RENDER__POSTFXCHAIN_H
#pragma once

#include <gallium/string_hash.h>

#include <deque>
#include <memory>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <vector>

#include <glm/glm.hpp>

namespace ga::gpu
{
	class Device;
	class CommandEncoder;
	class Image;
}

namespace ga::render
{
	class RenderGraph;

	class PostFXBase
	{
	public:
		virtual ~PostFXBase() = default;
		virtual void Execute(const ga::gpu::CommandEncoder& encoder, ga::gpu::Image& in, ga::gpu::Image& out, const std::vector<ga::gpu::Image*>& renderImages, const void* data) = 0;
	};

	template<typename T>
	class PostFX
		: public PostFXBase
	{
		void Execute(const ga::gpu::CommandEncoder& encoder, ga::gpu::Image& in, ga::gpu::Image& out, const std::vector<ga::gpu::Image*>& renderImages, const void* data) final
		{
			Execute(encoder, in, out, renderImages, *static_cast<const T*>(data));
		}

		virtual void Execute(const ga::gpu::CommandEncoder& encoder, ga::gpu::Image& in, ga::gpu::Image& out, const std::vector<ga::gpu::Image*>& renderImages, const T& params) = 0;
	};

	class PostFXChain
	{
		struct Entry
		{
			PostFXBase&            effect;
			std::vector<std::byte> data;
		};

		ga::gpu::Device&                m_gpu;

		std::unique_ptr<ga::gpu::Image> m_ping;
		std::unique_ptr<ga::gpu::Image> m_pong;

		std::unordered_map<string_hash_t, std::unique_ptr<PostFXBase>> m_effects;
		std::deque<Entry>                                              m_entries;

	public:
		PostFXChain(ga::gpu::Device& gpu);
		~PostFXChain();

		void Execute(ga::gpu::Device& gpu, const ga::gpu::CommandEncoder& encoder, ga::gpu::Image& input, ga::gpu::Image& output, const std::vector<ga::gpu::Image*>& renderImages);
		void Reset();

		template<typename T, typename PARAMS>
		void AddEffect(const PARAMS& params)
		{
			static_assert(std::is_trivially_copyable_v<PARAMS>);

			auto [it, inserted] = m_effects.try_emplace(T::Id());
			if (inserted)
				it->second = std::make_unique<T>(m_gpu);

			std::vector<std::byte> data(sizeof(params));
			std::copy_n(reinterpret_cast<const std::byte*>(&params), sizeof(params), data.data());
			m_entries.push_back({ .effect = *it->second, .data = data });
		}

		template<typename T, typename PARAMS>
		void AddEffectToFront(const PARAMS& params)
		{
			static_assert(std::is_trivially_copyable_v<PARAMS>);

			auto [it, inserted] = m_effects.try_emplace(T::Id());
			if (inserted)
				it->second = std::make_unique<T>(m_gpu);

			std::vector<std::byte> data(sizeof(params));
			std::copy_n(reinterpret_cast<const std::byte*>(&params), sizeof(params), data.data());
			m_entries.push_front({ .effect = *it->second, .data = data });
		}
	};
}

#endif /* GALLIUM__RENDER__POSTFXCHAIN_H */
