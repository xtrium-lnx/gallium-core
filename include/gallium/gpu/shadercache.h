#ifndef GALLIUM__GPU__SHADERCACHE_H
#define GALLIUM__GPU__SHADERCACHE_H
#pragma once

#include <memory>
#include <string_view>

namespace ga::platform { class Vfs; }

namespace ga::gpu
{
	class Device;
	class ShaderModule;

	class ShaderCache
	{
		struct Impl;
		std::unique_ptr<Impl> m_pImpl;

	public:
		ShaderCache(platform::Vfs& vfs, Device& gpu);
		~ShaderCache();

		ShaderModule& Get(std::string_view path);
	};
}

#endif /* GALLIUM__GPU__SHADERCACHE_H */
