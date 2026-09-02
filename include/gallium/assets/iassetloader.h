#ifndef GALLIUM__ASSETS__IASSETLOADER_H
#define GALLIUM__ASSETS__IASSETLOADER_H
#pragma once

#include <cstdint>
#include <memory>
#include <span>
#include <string_view>

namespace ga::assets
{
	class AssetManager;

	template<typename T>
	class IAssetLoader
	{
	public:
		virtual ~IAssetLoader() = default;
		virtual bool               CanLoad(std::string_view extension) const = 0;
		virtual std::shared_ptr<T> Load(std::string_view path, std::span<const std::byte> bytes, AssetManager& assetManager) = 0;
		virtual void               Unload(std::shared_ptr<T>&) {}
	};
}

#endif /* GALLIUM__ASSETS__IASSETLOADER_H */
