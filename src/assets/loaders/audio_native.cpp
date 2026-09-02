#include "audio_native.h"

#include <stdexcept>
#include <string>

#include <gallium/audio/audiosystem.h>

using namespace ga::assets::loaders;

AudioNativeLoader::AudioNativeLoader(ga::audio::AudioSystem& audioSystem)
    : m_audioSystem(audioSystem)
{
}

bool AudioNativeLoader::CanLoad(std::string_view ext) const
{
    return ext == ".wav"
        || ext == ".ogg"
        || ext == ".mp3";
}

std::shared_ptr<ga::audio::SoundHandle> AudioNativeLoader::Load(std::string_view path, std::span<const std::byte> bytes, AssetManager& assetManager)
{
    // Cheat it. The audio system already has to know the file when streaming
    return std::make_shared<ga::audio::SoundHandle>(std::move(m_audioSystem.LoadSound(path)));
}

void AudioNativeLoader::Unload(std::shared_ptr<ga::audio::SoundHandle>& sound)
{
    m_audioSystem.UnloadSound(*sound);
    sound->id = UINT32_MAX;
}
