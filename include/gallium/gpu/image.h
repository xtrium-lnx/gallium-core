#ifndef GALLIUM__GPU__IMAGE_H
#define GALLIUM__GPU__IMAGE_H
#pragma once

#include <gallium/gpu/enums.h>

#include <glm/glm.hpp>

#include <memory>
#include <optional>
#include <span>
#include <string>

namespace ga::gpu
{
	class  Device;
	struct DescriptorIndex;

	struct MipData
	{
		const void* pixels;
		glm::uvec3  size;
	};

	struct ImageDesc
	{
		EImageType                 type;
		EFormat                    format;
		size_t                     width        = 0;
		size_t                     height       = 1;
		size_t                     depth        = 1;
		EImageUsage                usage        = EImageUsage(0);
		size_t                     levels       = 1;
		size_t                     layers       = 1;
		EImageSamplingMode         samplingMode = EImageSamplingMode::Repeat;
		std::optional<std::string> debugName    = std::nullopt;

		const void*     initialData = nullptr;
	};

	struct ImageDescEx
	{
		EImageType                 type;
		EFormat                    format;
		size_t                     width        = 0;
		size_t                     height       = 1;
		size_t                     depth        = 1;
		EImageUsage                usage        = EImageUsage(0);
		size_t                     levels       = 1;
		size_t                     layers       = 1;
		EImageSamplingMode         samplingMode = EImageSamplingMode::Repeat;
		std::optional<std::string> debugName    = std::nullopt;

		std::span<const MipData>   initialData  = {};
	};

	struct ExistingImageDesc;

	class Image
	{
		struct Impl;
		std::unique_ptr<Impl> m_pImpl;

	public:
		Image(Device& device, const ImageDesc& desc);
		Image(Device& device, const ImageDescEx& desc);
		Image(const Device& device, const ExistingImageDesc& desc);
		~Image();

		void SetDebugName(const std::string& name) const;
		void SetSamplingMode(ga::gpu::EImageSamplingMode mode);

		const Impl& GetImpl() const;
		const DescriptorIndex& GetDescriptorIndex() const;
		void SetDescriptorIndex(const DescriptorIndex& descriptorIndex) const;

		glm::uvec3  Size() const;
		void        Upload(Device& device, const glm::uvec3& offset, const glm::uvec3& size, const void* pixels);
		void        UploadEx(Device& device, uint32_t mipLevel, const glm::uvec3& offset, const glm::uvec3& size, const void* pixels);
	};
}

#endif /* GALLIUM__GPU__IMAGE_H */
