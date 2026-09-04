#ifndef GALLIUM__CORE__TASK_H
#define GALLIUM__CORE__TASK_H
#pragma once

#include <gallium/badge.h>
#include <gallium/globalinstance.h>

#include <mutex>
#include <vector>

template<typename T, size_t N>
class TaskScheduler_ThreadSafeRingBuffer
{
	T          m_data[N] = {};
	size_t     m_head = 0;
	size_t     m_tail = 0;
	std::mutex m_mutex;

public:
	inline bool push_back(const T& item)
	{
		bool result = false;
		m_mutex.lock();

		size_t next = (m_head + 1) % N;

		if (next != m_tail)
		{
			m_data[m_head] = item;
			m_head = next;
			result = true;
		}

		m_mutex.unlock();
		return result;
	}

	inline bool pop_front(T& item)
	{
		bool result = false;
		m_mutex.lock();

		if (m_tail != m_head)
		{
			item = m_data[m_tail];
			++m_tail %= N;
			result = true;
		}

		m_mutex.unlock();
		return result;
	}
};

namespace ga::core
{
	template<typename RETURN_TYPE, typename... ARGS>
	class DelegateBase;
	using Task = DelegateBase<void>;

	class TaskScheduler;

	class WorkerThread
	{
		std::thread      m_thread;
		TaskScheduler*   m_scheduler;
		std::atomic_bool m_keepRunning;

		WorkerThread(TaskScheduler* scheduler);
		static void Run(WorkerThread* self, TaskScheduler* scheduler);

	public:
		WorkerThread(const Badge<TaskScheduler>&, TaskScheduler* scheduler)
			: WorkerThread(scheduler) {}

		~WorkerThread();
	};

	class TaskScheduler
		: public GlobalInstance<TaskScheduler>
	{
		uint32_t                                       m_numThreads;
		std::vector<WorkerThread*>                     m_workers;

		std::atomic_uint64_t                           m_finishedLabel;
		TaskScheduler_ThreadSafeRingBuffer<Task*, 256> m_pool;
		std::condition_variable                        m_wakeCondition;
		std::mutex                                     m_wakeMutex;
		uint64_t                                       m_currentLabel = 0;
		std::mutex                                     m_poolMutex;

		void m_Poll();

	public:
		void  Initialize() override;
		void  Terminate() override;

		void  EnqueueTask(Task* task);

		Task* QueryTask(const Badge<WorkerThread>&);
		void  NotifyTaskDone(const Badge<WorkerThread>&);
		void  Idle(const Badge<WorkerThread>&);
		void  WakeAll(const Badge<WorkerThread>&);

		bool  IsBusy();
		void  WaitForIdle();
	};
}

#endif /* GALLIUM__CORE__TASK_H */
