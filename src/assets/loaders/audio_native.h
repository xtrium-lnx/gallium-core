#ifndef GALLIUM__ASSETS__LOADERS__AUDIO_NATIVE_H
#define GALLIUM__ASSETS__LOADERS__AUDIO_NATIVE_H
#pragma once

#include <gallium/assets/iassetloader.h>
#include <gallium/audio/sound.h>

namespace ga::audio { class AudioSystem; }

namespace ga::assets::loaders
{
    class AudioNativeLoader
        : public IAssetLoader<ga::audio::SoundHandle>
    {
        ga::audio::AudioSystem& m_audioSystem;
    public:
        explicit AudioNativeLoader(ga::audio::AudioSystem& audioSystem);

        bool                                    CanLoad(std::string_view ext) const override;
        std::shared_ptr<ga::audio::SoundHandle> Load(std::string_view path, std::span<const std::byte> bytes, AssetManager& assetManager) override;
        void                                    Unload(std::shared_ptr<ga::audio::SoundHandle>& sound)  override;
    };
}

#endif /* GALLIUM__ASSETS__LOADERS__AUDIO_NATIVE_H */
