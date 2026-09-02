#pragma once

#include <gallium/gpu/shadercache.h>
#include <gallium/gpu/shadermodule.h>

#include <string>
#include <unordered_map>
#include <vector>

namespace ga::gpu
{
	struct ShaderCache::Impl
	{
		struct Entry
		{
			std::vector<std::byte>                 spirv;
			std::unique_ptr<ga::gpu::ShaderModule> module;
		};

		std::unordered_map<std::string, Entry> entries;
		ga::platform::Vfs&                     vfs;
		ga::gpu::Device&                       gpu;

		Impl(ga::platform::Vfs& vfs, ga::gpu::Device& gpu);
		~Impl();
	};
}
