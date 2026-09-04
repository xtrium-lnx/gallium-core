#ifndef GALLIUM__CORE__LOG_H
#define GALLIUM__CORE__LOG_H
#pragma once

#include <gallium/globalinstance.h>
#include <gallium/platform/timer.h>
#include <gallium/core/messagebus.h>

namespace ga::core
{
	enum class ELogLevel
	{
		Verbose = 0,
		Info,
		Warning,
		Error,
		Fatal = 99
	};

	GA_MESSAGEDATA(LogMessageData,
		uint64_t    timestamp;
		ELogLevel   severity;
		std::string tag;
		std::string message;
	);

	class LogSink_Console
		: public GlobalInstance<LogSink_Console>
	{
		ELogLevel m_minLogLevel = ELogLevel::Info;

		struct Internal;
		std::unique_ptr<Internal> m_pImpl;

	public:
		explicit LogSink_Console();
		~LogSink_Console() noexcept;

		void Initialize();
		void Terminate();

		void SetMinLogLevel(ELogLevel level);
		void OnLogMessage(MessageDataBase* msg);
	};
}

#ifndef NDEBUG
# define GA_LOG_VERBOSE(TAG, MSG)   ga::core::MessageBus::Global().PostImmediate("log_append"_h, ga::core::LogMessageData({ ga::platform::Timer::Now(), ga::core::ELogLevel::Verbose, TAG, MSG }))
# define GA_LOG_INFO(TAG, MSG)      ga::core::MessageBus::Global().PostImmediate("log_append"_h, ga::core::LogMessageData({ ga::platform::Timer::Now(), ga::core::ELogLevel::Info,    TAG, MSG }))
# define GA_LOG_WARNING(TAG, MSG)   ga::core::MessageBus::Global().PostImmediate("log_append"_h, ga::core::LogMessageData({ ga::platform::Timer::Now(), ga::core::ELogLevel::Warning, TAG, MSG }))
# define GA_LOG_ERROR(TAG, MSG)     ga::core::MessageBus::Global().PostImmediate("log_append"_h, ga::core::LogMessageData({ ga::platform::Timer::Now(), ga::core::ELogLevel::Error,   TAG, MSG }))
# define GA_LOG_FATAL(TAG, MSG)   ([&]{ ga::core::MessageBus::Global().PostImmediate("log_append"_h, ga::core::LogMessageData({ ga::core::Clock::Now(), ga::core::ELogLevel::Fatal,   TAG, MSG })); __debugbreak(); })()
#else
# define GA_LOG_VERBOSE(TAG, MSG) ([&]{})()
# define GA_LOG_INFO(TAG, MSG)    ([&]{})()
# define GA_LOG_WARNING(TAG, MSG) ([&]{})()
# define GA_LOG_ERROR(TAG, MSG)   ([&]{})()
# define GA_LOG_FATAL(TAG, MSG)   ([&]{})()
#endif

#endif /* GALLIUM__CORE__LOG_H */