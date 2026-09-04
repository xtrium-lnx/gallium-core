#ifndef GALLIUM__PLATFORM__TIMER_H
#define GALLIUM__PLATFORM__TIMER_H
#pragma once

#include <cstdint>
#include <memory>
#include <string>

namespace ga::platform
{
	class Timer
	{
		struct Impl;
		std::unique_ptr<Impl> m_pImpl;

	public:
		Timer();
		~Timer();

		static uint64_t Now();
		static std::string TimestampToString(uint64_t timestamp);

		void   Reset();
		void   Tick();
		double DeltaTime() const;
		double ElapsedTime() const;
		void   SetElapsedTime(double t);
	};
}

#endif /* GALLIUM__PLATFORM__TIMER_H */