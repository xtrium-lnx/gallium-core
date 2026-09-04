#ifndef GALLIUM__CORE__DELEGATE_H
#define GALLIUM__CORE__DELEGATE_H
#pragma once

#include <functional>

namespace ga::core
{
	template<typename RETURN_TYPE, typename... ARGS>
	class DelegateBase
	{
	public:
		virtual RETURN_TYPE Execute(ARGS... args) = 0;
		RETURN_TYPE operator()(ARGS... args) { return Execute(args...); }
	};

	template<typename RETURN_TYPE, typename... ARGS>
	class RawDelegate
		: public DelegateBase<RETURN_TYPE, ARGS...>
	{
		RETURN_TYPE(*m_function)(ARGS...);

	public:
		RawDelegate(RETURN_TYPE(*f)(ARGS...))
			: m_function(f)
		{ }

		virtual RETURN_TYPE Execute(ARGS... args) override
		{
			return m_function(args...);
		}
	};

	template<typename RETURN_TYPE, typename... ARGS>
	class LambdaDelegate
		: public DelegateBase<RETURN_TYPE, ARGS...>
	{
		std::function<RETURN_TYPE(ARGS...)> m_lambda;

	public:
		LambdaDelegate(const std::function<RETURN_TYPE(ARGS...)>& lambda)
			: m_lambda(lambda)
		{ }

		virtual RETURN_TYPE Execute(ARGS... args) override
		{
			return m_lambda(args...);
		}
	};

	template<typename OBJECT_TYPE, typename RETURN_TYPE, typename... ARGS>
	class ObjectMemberDelegate
		: public DelegateBase<RETURN_TYPE, ARGS...>
	{
		OBJECT_TYPE* m_object;
		RETURN_TYPE(OBJECT_TYPE::* m_member)(ARGS...);

	public:
		ObjectMemberDelegate(OBJECT_TYPE* o, RETURN_TYPE(OBJECT_TYPE::* m)(ARGS...))
			: m_object(o)
			, m_member(m)
		{ }

		virtual RETURN_TYPE Execute(ARGS... args) override
		{
			return std::invoke(m_member, m_object, args...);
		}
	};

	template<typename... ARGS>
	class Event
	{
		std::vector<DelegateBase<void, ARGS...>*> m_delegates;

	public:
		bool AddDelegate(DelegateBase<void, ARGS...>* delegate)
		{
			for (auto& d : m_delegates)
				if (d == delegate)
					return false;

			m_delegates.push_back(delegate);
			return true;
		}

		bool RemoveDelegate(DelegateBase<void, ARGS...>* delegate)
		{
			for (auto& it = m_delegates.begin(); it != m_delegates.end(); it++)
			{
				if ((*it) == delegate)
				{
					m_delegates.erase(it);
					return true;
				}
			}

			return false;
		}

		bool operator+=(DelegateBase<void, ARGS...>* delegate)
		{
			return AddDelegate(delegate);
		}

		bool operator-=(DelegateBase<void, ARGS...>* delegate)
		{
			return RemoveDelegate(delegate);
		}

		void Execute(ARGS... args)
		{
			for (auto& d : m_delegates)
				d->Execute(args...);
		}

		void operator()(ARGS... args)
		{
			Execute(args...);
		}
	};
}

#endif /* GALLIUM__CORE__DELEGATE_H */

