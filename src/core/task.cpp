#include <gallium/core/task.h>
#include <gallium/core/delegate.h>
#include <gallium/core/log.h>

#include <cassert>
#include <iostream>

#include <sstream>

#ifdef _WIN32
# define NOMINMAX
# define WIN32_LEAN_AND_MEAN
# define WIN32_EXTRA_LEAN
# include <Windows.h>
#endif /* _WIN32 */

using namespace ga::core;

// ----------------------------------------------------------------------------

static uint32_t s_threadAffinityId = 0;

WorkerThread::WorkerThread(TaskScheduler* scheduler)
	: m_thread(Run, this, scheduler)
	, m_scheduler(scheduler)
	, m_keepRunning(true)
{
#ifdef _WIN32
	HANDLE handle = (HANDLE)m_thread.native_handle();

	// Increase thread priority
	BOOL priority_result = SetThreadPriority(handle, THREAD_PRIORITY_HIGHEST);
	if (priority_result == 0)
	{
		DWORD err = GetLastError();
		GA_LOG_WARNING("ga.core.WorkerThread.WorkerThread", "Worker " + std::to_string(*reinterpret_cast<size_t*>(&handle)) + ": Unable to set worker priority (" + std::to_string(err) + ")");
	}

	// Name the thread
	HRESULT hr = SetThreadDescription(handle, (L"Helium WorkerThread " + std::to_wstring(s_threadAffinityId)).c_str());
	if (!SUCCEEDED(hr))
		GA_LOG_WARNING("ga.core.WorkerThread.WorkerThread", "Worker " + std::to_string(*reinterpret_cast<size_t*>(&handle)) + ": Unable to set worker name (" + std::to_string(hr) + ")");

	// Put each thread on to dedicated core
	DWORD_PTR affinity_result = SetThreadAffinityMask(handle, 1ull << s_threadAffinityId);
	if (affinity_result == 0)
	{
		DWORD err = GetLastError();
		GA_LOG_WARNING("ga.core.WorkerThread.WorkerThread", "Worker " + std::to_string(*reinterpret_cast<size_t*>(&handle)) + ": Unable to set worker core affinity (" + std::to_string(err) + ")");
	}
	else
		++s_threadAffinityId;
#endif /* _WIN32 */
}

WorkerThread::~WorkerThread()
{
	if (m_thread.joinable())
	{
		m_keepRunning.store(false);
		m_scheduler->WakeAll({});
		m_thread.join();
	}
}

void WorkerThread::Run(WorkerThread* self, TaskScheduler* scheduler)
{
	Task* task = nullptr;

	while (self->m_keepRunning.load())
	{
		task = scheduler->QueryTask({});

		if (task)
		{
			task->Execute();
			scheduler->NotifyTaskDone({});

			delete task;
		}
		else
			scheduler->Idle({});
	}
}

// ------------------------------------------------------------------------------------------------

void TaskScheduler::Initialize()
{
	m_finishedLabel.store(0);
	m_numThreads = std::max(1u, std::thread::hardware_concurrency());

	s_threadAffinityId = 0;
	for (uint32_t tId = 0; tId < m_numThreads; ++tId)
		m_workers.push_back(new WorkerThread({}, this));
}

void TaskScheduler::Terminate()
{
	for (auto& worker : m_workers)
		delete worker;

	m_workers.clear();
}

void TaskScheduler::m_Poll()
{
	m_wakeCondition.notify_one();
	std::this_thread::yield();
}

Task* TaskScheduler::QueryTask(const Badge<WorkerThread>&)
{
	std::unique_lock<std::mutex> lock(m_poolMutex);

	Task* result = nullptr;
	if (m_pool.pop_front(result))
		return result;

	return nullptr;
}

void TaskScheduler::NotifyTaskDone(const Badge<WorkerThread>&)
{
	m_finishedLabel.fetch_add(1);
}

void TaskScheduler::Idle(const Badge<WorkerThread>&)
{
	std::unique_lock lock(m_wakeMutex);
	m_wakeCondition.wait(lock);
}

void TaskScheduler::WakeAll(const Badge<WorkerThread>&)
{
	m_wakeCondition.notify_all();
}

void TaskScheduler::EnqueueTask(Task* task)
{
	std::unique_lock<std::mutex> lock(m_poolMutex);

	m_currentLabel += 1;

	while (!m_pool.push_back(task))
		m_Poll();

	m_wakeCondition.notify_one();
}

bool TaskScheduler::IsBusy()
{
	return m_finishedLabel.load() < m_currentLabel;
}

void TaskScheduler::WaitForIdle()
{
	while (IsBusy())
		m_Poll();
}