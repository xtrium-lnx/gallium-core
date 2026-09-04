#include <gallium/core/log.h>

#define WIN32_LEAN_AND_MEAN
#define WIN32_EXTRA_LEAN
#include <Windows.h>

#include <cassert>
#include <format>

using namespace ga::core;

std::string LogLevelToString(ELogLevel level)
{
	switch (level)
	{
	case ELogLevel::Info:    return "info";
	case ELogLevel::Warning: return "warning";
	case ELogLevel::Error:   return "error";
	case ELogLevel::Fatal:   return "fatal";

	default:
		break;
	}

	return "verbose";
}

struct LogSink_Console::Internal
{
	MessageHandler* sinkDelegate = nullptr;
};

LogSink_Console::LogSink_Console()
	: m_pImpl(new Internal)
{
}

LogSink_Console::~LogSink_Console() noexcept
{
	Terminate();
}

void LogSink_Console::Initialize()
{
	m_pImpl->sinkDelegate = new ObjectMemberDelegate(this, &LogSink_Console::OnLogMessage);
	MessageBus::Global().AddRecipient("log_append"_h, m_pImpl->sinkDelegate);
}

void LogSink_Console::Terminate()
{
	if (m_pImpl->sinkDelegate)
		MessageBus::Global().RemoveRecipient("log_append"_h, m_pImpl->sinkDelegate);

	delete m_pImpl->sinkDelegate;
	m_pImpl->sinkDelegate = nullptr;
}

void LogSink_Console::SetMinLogLevel(ELogLevel level)
{
	m_minLogLevel = level;
}

void LogSink_Console::OnLogMessage(MessageDataBase* msg)
{
	LogMessageData* log = CTTI_CAST(msg, LogMessageData);
	assert(log);

	if (log->data.severity >= m_minLogLevel)
	{
		auto fullString = std::format("[{}] ({}) {}: {}\n",
			ga::platform::Timer::TimestampToString(log->data.timestamp),
			log->data.tag,
			LogLevelToString(log->data.severity),
			log->data.message
		);

		OutputDebugStringA(fullString.c_str());
	}
}