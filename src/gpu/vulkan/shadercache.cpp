#include "shadercache_impl.h"

#include <gallium/platform/vfs.h>

using namespace ga::gpu;

ShaderCache::Impl::Impl(ga::platform::Vfs& vfs, ga::gpu::Device& gpu)
	: vfs(vfs)
	, gpu(gpu)
{
}

ShaderCache::Impl::~Impl()
{
}

ShaderCache::ShaderCache(platform::Vfs& vfs, Device& gpu)
	: m_pImpl(new Impl(vfs, gpu))
{
}

ShaderCache::~ShaderCache()
{
}

ShaderModule& ShaderCache::Get(std::string_view path)
{
    auto it = m_pImpl->entries.find(std::string(path));
    if (it != m_pImpl->entries.end())
        return *it->second.module;

    auto& entry = m_pImpl->entries[std::string(path)];
    entry.spirv = m_pImpl->vfs.Open(path)
        .and_then([](auto f) { return f.ReadBytes(); })
        .value();

    entry.module = std::make_unique<ShaderModule>(m_pImpl->gpu, entry.spirv.data(), entry.spirv.size());
    return *entry.module;
}
