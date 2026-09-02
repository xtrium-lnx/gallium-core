#ifndef GALLIUM__RENDER__FONT_H
#define GALLIUM__RENDER__FONT_H
#pragma once

#include <gallium/gpu/image.h>

#include <cstdint>
#include <unordered_map>
#include <memory>

#include <glm/glm.hpp>

namespace ga::render
{
	struct Glyph
	{
		glm::vec2 uv0;
		glm::vec2 uv1;
		glm::vec2 xy0;
		glm::vec2 xy1;
		float     advance;
		float     leftBearing;
		float     msdfScale;
	};

	struct KerningPair
	{
		uint32_t a, b;
		float advance;
	};

	struct Font
	{
		float ascent;
		float descent;
		float lineGap;

		std::unordered_map<uint32_t, Glyph> glyphs;
		std::unordered_map<uint64_t, float> kerning;
		std::unique_ptr<ga::gpu::Image>     atlas;

		static uint32_t DecodeUtf8(const char*& s);
	};
}

#endif /* GALLIUM__RENDER__FONT_H */
