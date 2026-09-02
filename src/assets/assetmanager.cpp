#include <gallium/assets/assetmanager.h>

#include <gallium/platform/platform.h>
#include <gallium/platform/vfs.h>

#include "loaders/image_stb.h"
#include "loaders/image_exr.h"
#include "loaders/audio_native.h"
#include "loaders/mesh_obj.h"
#include "loaders/mesh_glb.h"
#include "loaders/font_ttf.h"

using namespace ga::assets;

AssetManager::AssetManager(const AssetManagerCreateInfo& info)
    : m_platform(info.platform)
{
    RegisterLoader<ga::gpu::Image>(std::make_unique<loaders::StbImageLoader>(info.gpu));
    RegisterLoader<ga::gpu::Image>(std::make_unique<loaders::ExrImageLoader>(info.gpu));
    RegisterLoader<ga::audio::SoundHandle>(std::make_unique<loaders::AudioNativeLoader>(info.audioSystem));
    RegisterLoader<ga::render::Mesh>(std::make_unique<loaders::ObjMeshLoader>(info.gpu));
    RegisterLoader<ga::render::Mesh>(std::make_unique<loaders::GlbMeshLoader>(info.gpu));
    RegisterLoader<ga::render::Font>(std::make_unique<loaders::TtfFontLoader>(info.gpu));

    m_materialLibrary = std::make_unique<ga::render::MaterialLibrary>();
}

void AssetManager::Clear()
{
    m_cache.clear();
    m_materialLibrary->Clear();
}

std::vector<std::string> AssetManager::GetLoadedPaths() const
{
    std::vector<std::string> result;

    for (const auto& [path, _] : m_cache)
        result.push_back(path);

    return result;
}

ga::render::MaterialLibrary& AssetManager::MaterialLibrary()
{
    return *m_materialLibrary;
}

std::vector<std::byte> AssetManager::m_ReadBytes(std::string_view path)
{
    return m_platform.Vfs()
        .Open(path)
        .and_then([](auto f) { return f.ReadBytes(); })
        .value();
}

std::filesystem::file_time_type AssetManager::m_ReadLastModified(std::string_view path)
{
    return m_platform.Vfs()
        .LastModified(path);
}