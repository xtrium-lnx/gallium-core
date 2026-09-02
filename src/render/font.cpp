#include <gallium/render/font.h>

using namespace ga::render;

uint32_t Font::DecodeUtf8(const char*& s)
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
