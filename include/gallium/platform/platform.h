#ifndef GALLIUM__PLATFORM__PLATFORM_H
#define GALLIUM__PLATFORM__PLATFORM_H
#pragma once

#include <gallium/core/messagebus.h>

#include <memory>
#include <string>
#include <vector>

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>

namespace ga::platform
{
	class Vfs;
	class Input;
	class Timer;

	struct PlatformInfo
	{
		std::string appName;
		glm::uvec3  appVersion;
		glm::uvec2  requestedSurfaceSize;
		bool        requestFullscreen = false;
		bool        requestResizableWindow = false;
		bool        mountWorkingDirectoryAsDefault = true;
		bool        hideMouseCursor = false;
	};

	GA_MESSAGEDATA(SurfaceResizeData,
		glm::uvec2 surfaceSize;
	);

	class Platform
	{
		struct Impl;
		std::unique_ptr<Impl> m_pImpl;

	public:
		struct Surface;

		Platform(const PlatformInfo& desc);
		~Platform();

		Impl& GetImpl() const;

		void SetAppName(const std::string& name);

		Vfs& Vfs();
		Input& Input();
		Timer& Timer();

		glm::uvec2 SurfaceSize() const;
		const std::vector<std::string>& DropPaths() const;
		
		void PollEvents();
		void RequestExit();
		bool IsExitRequested() const;
	};
}

#endif /* GALLIUM__PLATFORM__PLATFORM_H */
