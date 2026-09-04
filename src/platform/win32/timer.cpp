#include "timer_impl.h"

using namespace ga::platform;

Timer::Timer()
	: m_pImpl(std::make_unique<Impl>())
{
	Reset();
}

Timer::~Timer() = default;

uint64_t Timer::Now()
{
    FILETIME   filetime;
    GetSystemTimePreciseAsFileTime(&filetime);

    FILETIME   localfiletime;
    FileTimeToLocalFileTime(&filetime, &localfiletime);

    SYSTEMTIME systemtime;
    FileTimeToSystemTime(&localfiletime, &systemtime);

    uint64_t       retval = systemtime.wYear;
    retval <<= 4;  retval += systemtime.wMonth;
    retval <<= 5;  retval += systemtime.wDay;
    retval <<= 5;  retval += systemtime.wHour;
    retval <<= 6;  retval += systemtime.wMinute;
    retval <<= 6;  retval += systemtime.wSecond;
    retval <<= 10; retval += systemtime.wMilliseconds;

    return retval;
}

std::string Timer::TimestampToString(uint64_t timestamp)
{
    uint64_t milliseconds = timestamp & ((1 << 10) - 1); timestamp >>= 10;
    uint64_t seconds = timestamp & ((1 << 6) - 1); timestamp >>= 6;
    uint64_t minutes = timestamp & ((1 << 6) - 1); timestamp >>= 6;
    uint64_t hours = timestamp & ((1 << 5) - 1); timestamp >>= 5;
    uint64_t day = timestamp & ((1 << 5) - 1); timestamp >>= 5;
    uint64_t month = timestamp & ((1 << 4) - 1); timestamp >>= 4;
    uint64_t year = timestamp;

    auto alignToLength = [](uint64_t v, uint64_t boundary)
    {
        std::string retval = "";

        while (boundary > 1)
        {
            if (v >= boundary)
                break;

            retval += "0";
            boundary /= 10;
        }

        retval += std::to_string(v);
        return retval;
    };

    return alignToLength(year, 1000) + "-" + alignToLength(month, 10) + "-" + alignToLength(day, 10) + " " +
        alignToLength(hours, 10) + ":" + alignToLength(minutes, 10) + ":" + alignToLength(seconds, 10) + "." + alignToLength(milliseconds, 100);
}

void Timer::Reset()
{
	QueryPerformanceFrequency(&m_pImpl->frequency);
	QueryPerformanceCounter(&m_pImpl->startTime);
	m_pImpl->lastTime = m_pImpl->startTime;
	m_pImpl->currentTime = m_pImpl->startTime;
}

void Timer::Tick()
{
	m_pImpl->lastTime = m_pImpl->currentTime;
	QueryPerformanceCounter(&m_pImpl->currentTime);

	const int64_t delta = m_pImpl->currentTime.QuadPart - m_pImpl->lastTime.QuadPart;
	const int64_t total = m_pImpl->currentTime.QuadPart - m_pImpl->startTime.QuadPart;

	m_pImpl->deltaTime   = static_cast<double>(delta) / static_cast<double>(m_pImpl->frequency.QuadPart);
	m_pImpl->elapsedTime = static_cast<double>(total) / static_cast<double>(m_pImpl->frequency.QuadPart);
}

double Timer::DeltaTime() const
{
	return m_pImpl->deltaTime;
}

double Timer::ElapsedTime() const
{
	return m_pImpl->elapsedTime;
}

void Timer::SetElapsedTime(double t)
{
	QueryPerformanceCounter(&m_pImpl->currentTime);

	const int64_t ticks = static_cast<int64_t>(t * m_pImpl->frequency.QuadPart);

	m_pImpl->startTime.QuadPart = m_pImpl->currentTime.QuadPart - ticks;
	m_pImpl->lastTime = m_pImpl->currentTime;

	m_pImpl->elapsedTime = t;
}
