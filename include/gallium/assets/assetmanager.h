#ifndef GALLIUM__ASSETS__ASSETMANAGER_H
#define GALLIUM__ASSETS__ASSETMANAGER_H
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <span>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <gallium/globalinstance.h>
#include <gallium/assets/iassetloader.h>
#include <gallium/gpu/image.h>
#include <gallium/audio/sound.h>
#include <gallium/render/mesh.h>
#include <gallium/render/font.h>
#include <gallium/render/materiallibrary.h>

namespace ga::platform { class Platform; }
namespace ga::audio    { class AudioSystem; }

namespace ga::assets
{
    struct AssetManagerCreateInfo
    {
        platform::Platform& platform;
        gpu::Device&        gpu;
        audio::AudioSystem& audioSystem;
    };

    class AssetManager
        : public ga::GlobalInstance<AssetManager, AssetManagerCreateInfo>
    {
        template <typename T>
        using LoaderList = std::vector<std::unique_ptr<IAssetLoader<T>>>;

        template<typename... TYPES>
        using LoaderContainerT = std::tuple<LoaderList<TYPES>...>;

        using LoaderContainer = LoaderContainerT<
            ga::gpu::Image,
            ga::audio::SoundHandle,
            ga::render::Mesh,
            ga::render::Font
        >;

        std::unique_ptr<ga::render::MaterialLibrary> m_materialLibrary;

        std::vector<std::byte> m_ReadBytes(std::string_view path);
        std::filesystem::file_time_type m_ReadLastModified(std::string_view path);

        LoaderContainer     m_loaders;
        platform::Platform& m_platform;

        std::unordered_map<std::string, std::shared_ptr<void>>           m_cache;
        std::unordered_map<std::string, std::filesystem::file_time_type> m_lastModified;

        template<typename T>
        static T& m_FromCacheEntry(std::shared_ptr<void>& entry)
        {
            return *static_cast<T*>(entry.get());
        }

    public:
        explicit AssetManager(const AssetManagerCreateInfo& info);

        ~AssetManager()                              = default;
        AssetManager(const AssetManager&)            = delete;
        AssetManager& operator=(const AssetManager&) = delete;

        void Clear();
        std::vector<std::string> GetLoadedPaths() const;

        ga::render::MaterialLibrary& MaterialLibrary();

        template<typename T>
        void RegisterLoader(std::unique_ptr<IAssetLoader<T>> loader)
        {
            std::get<LoaderList<T>>(m_loaders).push_back(std::move(loader));
        }

        bool IsLoaded(std::string_view path) const
        {
            return m_cache.contains(std::string(path));
        }

        template<typename T>
        std::optional<std::reference_wrapper<T>> Get(std::string_view path)
        {
            if (!IsLoaded(path))
                return std::nullopt;

            return std::ref(m_FromCacheEntry<T>(m_cache[std::string(path)]));
        }

        template<typename T>
        T& Insert(std::string_view path, std::span<const std::byte> bytes, std::filesystem::file_time_type lastModified = std::filesystem::file_time_type::min())
        {
            for (auto& loader : std::get<LoaderList<T>>(m_loaders))
            {
                const auto ext = std::filesystem::path(path).extension().string();

                if (loader->CanLoad(ext))
                {
                    m_cache[std::string(path)]        = loader->Load(path, bytes, *this);
                    m_lastModified[std::string(path)] = lastModified;
                    break;
                }
            }

            if (!IsLoaded(path))
                throw std::runtime_error("AssetManager: no loader registered for '" + std::string(path) + "'");

            return Get<T>(path)->get();
        }

        template<typename T>
        T& Load(std::string_view path)
        {
            if (IsLoaded(path))
            {
                const auto lastModifiedFromCache = m_lastModified[std::string(path)];
                const auto lastModifiedFromFile  = m_ReadLastModified(path);

                auto now = std::chrono::file_clock::now();
                auto dt  = now - lastModifiedFromFile;

                if (lastModifiedFromCache == lastModifiedFromFile || dt < std::chrono::milliseconds(500))
                    return Get<T>(path)->get();
                else
                {
                    m_lastModified.erase(std::string(path));
                    m_cache.erase(std::string(path));
                }
            }

            const auto bytes = m_ReadBytes(path);

            return Insert<T>(path, bytes, m_ReadLastModified(path));
        }

        template<typename T>
        std::string PathOf(const T& t)
        {
            for (const auto& [name, asset] : m_cache)
                if (&t == static_cast<T*>(asset.get()))
                    return name;

            return "";
        }
    };
}

#endif /* GALLIUM__ASSETS__ASSETMANAGER_H */
