#ifndef GALLIUM__ASSETS__IMAGEDATA_H
#define GALLIUM__ASSETS__IMAGEDATA_H
#pragma once

#include <cstdint>
#include <vector>

#include <gallium/gpu/enums.h>

namespace ga::assets
{
	struct ImageData
	{
		std::vector<std::byte> pixels;
		uint32_t               width  = 0;
		uint32_t               height = 0;
		gpu::EFormat           format = gpu::EFormat::R8G8B8A8_UNorm;
	};
}

#endif /* GALLIUM__ASSETS__IMAGEDATA_H */
