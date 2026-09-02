#ifndef GALLIUM__AUDIO__SOUND_H
#define GALLIUM__AUDIO__SOUND_H
#pragma once

#include <cstdint>

namespace ga::audio
{
    struct SoundHandle
    {
        uint32_t id = UINT32_MAX;
        bool IsValid() const { return id != UINT32_MAX; }
        bool operator==(const SoundHandle&) const = default;
    };
}

template<>
struct std::hash<ga::audio::SoundHandle>
{
    std::size_t operator()(const ga::audio::SoundHandle& s) const noexcept
    {
        return s.id;
    }
};

#endif /* GALLIUM__AUDIO__SOUND_H */