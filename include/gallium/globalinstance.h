#ifndef GALLIUM__GLOBALINSTANCE_H
#define GALLIUM__GLOBALINSTANCE_H
#pragma once

#include <memory>

namespace ga
{
	template<typename T, typename CTOR_DESC = void>
	class GlobalInstance
	{
		static inline std::unique_ptr<T> ms_instance = nullptr;
	public:
		static T& Global()
		{
			return *ms_instance;
		}

		static void Create(const CTOR_DESC& desc)
		{
			ms_instance = std::make_unique<T>(desc);
		}

		static void Destroy()
		{
			ms_instance.reset();
		}

		virtual void Initialize(const CTOR_DESC& desc) {};
		virtual void Terminate() {};
	};

	template<typename T>
	class GlobalInstance<T, void>
	{
	public:
		static T& Global()
		{
			static T s_instance;
			return s_instance;
		}

		virtual void Initialize() {};
		virtual void Terminate() {};
	};
}

#endif /* GALLIUM__GLOBALINSTANCE_H */
