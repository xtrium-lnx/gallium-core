#ifndef GALLIUM__PLATFORM__WIN32__PLATFORM_IMPL_H
#define GALLIUM__PLATFORM__WIN32__PLATFORM_IMPL_H
#pragma once

#include <memory>

#include <gallium/platform/platform.h>
#include <gallium/platform/vfs.h>
#include <gallium/platform/timer.h>

#define GLFW_INCLUDE_VULKAN
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

namespace ga::platform
{
	struct Platform::Surface
	{
		GLFWwindow* window = nullptr;
	};

	struct Platform::Impl
	{
		PlatformInfo                     desc;
		Surface                          surface;
		std::unique_ptr<platform::Vfs>   vfs;
		std::unique_ptr<platform::Input> input;
		std::unique_ptr<platform::Timer> timer;

		std::vector<std::string> pendingDropPaths;
		std::vector<std::string> dropPaths;

		static void OnFileDropped(GLFWwindow* window, int path_count, const char* paths[]);
	};
}

#endif /* GALLIUM__PLATFORM__WIN32__PLATFORM_IMPL_H */
