#include "font_ttf.h"

#include <gallium/gpu/image.h>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

#define MSDF_IMPLEMENTATION
#include <msdf.h>

#include <algorithm>
#include <stdexcept>
#include <vector>

using namespace ga::assets::loaders;

constexpr int kGlyphPadding = 2;
constexpr int kAtlasGutter  = 2;

struct TempGlyph
{
    uint32_t             cp;
    int                  w, h;
    std::vector<uint8_t> pixels;
    ga::render::Glyph    glyph;
};

static uint8_t s_encodeToUNorm(float d, float range)
{
    float v = d / range * 0.5f + 0.5f;
    return uint8_t(glm::clamp(v, 0.f, 1.f) * 255.0f);
}

static uint32_t s_utf8Decode(const char*& s)
{
    uint8_t c = (uint8_t)*s++;

    if (c < 0x80)
        return c;

    if ((c >> 5) == 0x6)
    {
        uint32_t cp = (c & 0x1F) << 6;
        cp |= ((uint8_t)*s++ & 0x3F);
        return cp;
    }

    if ((c >> 4) == 0xE)
    {
        uint32_t cp = (c & 0x0F) << 12;
        cp |= ((uint8_t)*s++ & 0x3F) << 6;
        cp |= ((uint8_t)*s++ & 0x3F);
        return cp;
    }

    if ((c >> 3) == 0x1E)
    {
        uint32_t cp = (c & 0x07) << 18;
        cp |= ((uint8_t)*s++ & 0x3F) << 12;
        cp |= ((uint8_t)*s++ & 0x3F) << 6;
        cp |= ((uint8_t)*s++ & 0x3F);
        return cp;
    }

    return '?';
}

bool s_tryPack(int W, int H, const std::vector<TempGlyph>& glyphs)
{
    int penX = 0, penY = 0, rowH = 0;

    for (auto& glyph : glyphs)
    {
        if (penX + glyph.w > W)
        {
            penX = 0;
            penY += rowH + kAtlasGutter;
            rowH = 0;
        }

        if (penY + glyph.h > H)
            return false;

        penX += glyph.w + kAtlasGutter;
        rowH = std::max(rowH, glyph.h);
    }

    return true;
}

TtfFontLoader::TtfFontLoader(ga::gpu::Device& gpu)
	: m_gpu(gpu)
{
}

bool TtfFontLoader::CanLoad(std::string_view ext) const
{
	return ext == ".ttf";
}

std::shared_ptr<ga::render::Font> TtfFontLoader::Load(std::string_view path, std::span<const std::byte> bytes, AssetManager& assetManager)
{
    static constexpr float kMsdfScale = 4.0f;

    static constexpr const char* kAllGlyphs =
        /* numbers */     "0123456789"
        /* symbols */     "+-_*/=[]{}\\'\";:/?.>,<!@#$%^&*()"
        /* uppercase */   "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        /* lowercase */   "abcdefghijklmnopqrstuvwxyz"
        /* accents */     "\xc3\xa9\xc3\xa8\xc3\xaa\xc3\xab"   // é è ê ë
                          "\xc3\xa0\xc3\xa2"                   // à â
                          "\xc3\xb9\xc3\xbb\xc3\xbc"           // ù û ü
                          "\xc3\xb4\xc3\xae\xc3\xaf"           // ô î ï
                          "\xc3\xa7"                           // ç
                          "\xc3\x89\xc3\x88\xc3\x8a\xc3\x8b"   // É È Ê Ë
                          "\xc3\x80\xc3\x82"                   // À Â
                          "\xc3\x99\xc3\x9b\xc3\x9c"           // Ù Û Ü
                          "\xc3\x94\xc3\x8e\xc3\x8f"           // Ô Î Ï
                          "\xc3\x87"                           // Ç
        /* ligatures */   "\xc5\x93\xc5\x8c\xc3\xa6\xc3\x86"   // œ Œ æ Æ
        /* quotes */      "\xc2\xab\xc2\xbb"                   // « »
        /* typographic */ "\xe2\x80\x99"                       // ' (right single quote)
                          "\xe2\x80\xa6"                       // … (ellipsis)
                          " "
                          "\0";

	auto result = std::make_shared<ga::render::Font>();

	stbtt_fontinfo font;
	stbtt_InitFont(&font, (const unsigned char*)bytes.data(), stbtt_GetFontOffsetForIndex((const unsigned char*)bytes.data(), 0));

	float scale = stbtt_ScaleForPixelHeight(&font, 48.0f);

	int ascent, descent, lineGap;
	stbtt_GetFontVMetrics(&font, &ascent, &descent, &lineGap);

	result->ascent    = float(ascent)  * scale;
	result->descent   = float(descent) * scale;
	result->lineGap   = float(lineGap) * scale;

    std::vector<uint32_t> codepoints;
    {
        const char* s = kAllGlyphs;
        while (*s)
        {
            uint32_t cp = s_utf8Decode(s);
            if (std::find(codepoints.begin(), codepoints.end(), cp) == codepoints.end())
                codepoints.push_back(cp);
        }
    }

    std::vector<TempGlyph> temp;
    for (uint32_t cp : codepoints)
    {
        int glyphIndex = stbtt_FindGlyphIndex(&font, cp);
        if (glyphIndex == 0)
            continue;

        msdf_Result msdfResult;
        int success = msdf_genGlyph(&msdfResult, &font, glyphIndex, kGlyphPadding, scale, kMsdfScale, nullptr);

        // bounds
        int x0,y0,x1,y1;
        stbtt_GetGlyphBox(&font, glyphIndex, &x0,&y0,&x1,&y1);

        float fx0 = x0 * scale;
        float fy0 = y0 * scale;
        float fx1 = x1 * scale;
        float fy1 = y1 * scale;

        int w = msdfResult.width;
        int h = msdfResult.height;

        std::vector<uint8_t> pixels(w * h * 4);
        for (int y = 0; y < h; ++y)
        {
            for (int x = 0; x < w; ++x)
            {
                size_t idx = 3 * (y * w + x);
                pixels[(y * w + x) * 4 + 0] = s_encodeToUNorm(msdfResult.rgb[idx + 0], kMsdfScale);
                pixels[(y * w + x) * 4 + 1] = s_encodeToUNorm(msdfResult.rgb[idx + 1], kMsdfScale);
                pixels[(y * w + x) * 4 + 2] = s_encodeToUNorm(msdfResult.rgb[idx + 2], kMsdfScale);
                pixels[(y * w + x) * 4 + 3] = 255;
            }
        }

        // metrics
        int advance, leftBearing;
        stbtt_GetGlyphHMetrics(&font, glyphIndex, &advance, &leftBearing);

        ga::render::Glyph g = {};
        g.advance = advance * scale;
        g.leftBearing = leftBearing * scale;

        g.xy0 = { fx0 - kGlyphPadding, fy0 - kGlyphPadding };
        g.xy1 = { fx1 + kGlyphPadding, fy1 + kGlyphPadding };

        temp.push_back({ cp, w, h, std::move(pixels), g });
    }

    std::sort(temp.begin(), temp.end(), [](const TempGlyph& a, const TempGlyph& b) { return std::max(a.w, a.h) > std::max(b.w, b.h); });

    int atlasW = 256;
    int atlasH = 256;

    for (size_t i = 0;; ++i)
    {
        if (atlasW > 4096 || atlasH > 4096)
            throw std::runtime_error("Atlas too big");

        if (s_tryPack(atlasW, atlasH, temp))
            break;

        if (i % 2)
            atlasH *= 2;
        else
            atlasW *= 2;
    }

    std::vector<uint8_t> pixels;
    pixels.resize(atlasW * atlasH * 4, 0);

    int penX = 0, penY = 0, rowH = 0;

    for (auto& t : temp)
    {
        if (penX + t.w >= atlasW)
        {
            penX = 0;
            penY += rowH + kAtlasGutter;
            rowH = 0;
        }

        // copy pixels
        for (int y = 0; y < t.h; ++y)
            memcpy(&pixels[((penY+y)*atlasW + penX)*4], &t.pixels[y * t.w * 4], t.w * 4);

        // UVs
        float texelX = 0.5f / atlasW;
        float texelY = 0.5f / atlasH;
        t.glyph.uv0 = { (float)penX / atlasW + texelX, (float)penY / atlasH + texelY };
        t.glyph.uv1 = { (float)(penX + t.w) / atlasW - texelX, (float)(penY + t.h) / atlasH - texelY };

        float glyphTexelW = float(t.w - 1);
        float glyphVirtualW = t.glyph.xy1.x - t.glyph.xy0.x;
        t.glyph.msdfScale = (glyphVirtualW / glyphTexelW) * kMsdfScale;

        result->glyphs[t.cp] = t.glyph;

        penX += t.w + kAtlasGutter;
        rowH = std::max(rowH, t.h);
    }

    for (uint32_t a : codepoints)
    {
        for (uint32_t b : codepoints)
        {
            int gA = stbtt_FindGlyphIndex(&font, a);
            int gB = stbtt_FindGlyphIndex(&font, b);

            int k = stbtt_GetGlyphKernAdvance(&font, gA, gB);

            if (k != 0)
            {
                uint64_t key = ((uint64_t)a << 32) | b;
                result->kerning[key] = k * scale;
            }
        }
    }

    result->atlas = std::make_unique<ga::gpu::Image>(m_gpu, ga::gpu::ImageInfo {
        .type        = ga::gpu::EImageType::Image2D,
        .format      = ga::gpu::EFormat::R8G8B8A8_UNorm,
        .width       = size_t(atlasW),
        .height      = size_t(atlasH),
        .usage       = ga::gpu::EImageUsage::Sampled,
        .initialData = pixels.data()
    });
    result->atlas->SetDebugName("TtfFontLoader atlas");

	return result;
}
