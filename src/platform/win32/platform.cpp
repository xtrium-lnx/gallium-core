#include "platform_impl.h"
#include "input_impl.h"
#include "timer_impl.h"

#ifndef NDEBUG
# pragma comment(lib, "glfw3-s-d.lib")
#else /* NDEBUG */
# pragma comment(lib, "glfw3-s.lib")
#endif /* NDEBUG */

using namespace ga::platform;

// ----------------------------------------------------------------------------

void Platform::Impl::OnFileDropped(GLFWwindow* window, int path_count, const char* paths[])
{
	auto& impl = reinterpret_cast<Platform*>(glfwGetWindowUserPointer(window))->GetImpl();

	for (int i = 0; i < path_count; ++i)
		impl.pendingDropPaths.emplace_back(paths[i]);
}

// ----------------------------------------------------------------------------

Platform::Platform(const PlatformDesc& desc)
	: m_pImpl(new Impl)
{
	m_pImpl->desc = desc;

	glfwInit();

	glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

	if (!desc.requestResizableWindow)
		glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	m_pImpl->surface.window = glfwCreateWindow(
		desc.requestedSurfaceSize.x, desc.requestedSurfaceSize.y,
		desc.appName.c_str(),
		desc.requestFullscreen ? glfwGetPrimaryMonitor() : nullptr,
		nullptr
	);

	glfwSetWindowUserPointer(m_pImpl->surface.window, this);
	glfwSetDropCallback(m_pImpl->surface.window, Impl::OnFileDropped);
	if (desc.hideMouseCursor)
		glfwSetInputMode(m_pImpl->surface.window, GLFW_CURSOR, GLFW_CURSOR_HIDDEN);

	m_pImpl->vfs   = std::make_unique<ga::platform::Vfs>(desc.mountWorkingDirectoryAsDefault);
	m_pImpl->input = std::make_unique<ga::platform::Input>(*this);
	m_pImpl->timer = std::make_unique<ga::platform::Timer>();
}

Platform::~Platform()
{
	m_pImpl->input.reset();
	m_pImpl->vfs.reset();

	if (m_pImpl->desc.hideMouseCursor)
		glfwSetInputMode(m_pImpl->surface.window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);

	glfwDestroyWindow(m_pImpl->surface.window);
	glfwTerminate();
}

Platform::Impl& Platform::GetImpl() const
{
	return *m_pImpl;
}

void Platform::SetAppName(const std::string& name)
{
	glfwSetWindowTitle(m_pImpl->surface.window, name.c_str());
}

Vfs& Platform::Vfs()
{
	return *m_pImpl->vfs;
}

Input& Platform::Input()
{
	return *m_pImpl->input;
}

Timer& Platform::Timer()
{
	return *m_pImpl->timer;
}

glm::uvec2 Platform::SurfaceSize() const
{
	int w, h;
	glfwGetFramebufferSize(m_pImpl->surface.window, &w, &h);
	return { uint32_t(w), uint32_t(h) };
}

const std::vector<std::string>& Platform::DropPaths() const
{
	return m_pImpl->dropPaths;
}

void Platform::PollEvents()
{
	glfwPollEvents();
	m_pImpl->input->GetImpl().PollEvents();
	m_pImpl->timer->Tick();

	m_pImpl->dropPaths = m_pImpl->pendingDropPaths;
	m_pImpl->pendingDropPaths.clear();
}

void Platform::RequestExit()
{
	glfwSetWindowShouldClose(m_pImpl->surface.window, GLFW_TRUE);
}

bool Platform::IsExitRequested() const
{
	return glfwWindowShouldClose(m_pImpl->surface.window);
}
