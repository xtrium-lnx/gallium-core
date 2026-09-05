#ifndef GALLIUM__CORE__CTTI_H
#define GALLIUM__CORE__CTTI_H
#pragma once

#include <string>
#include <unordered_map>
#include <vector>

#include <gallium/string_hash.h>

namespace ga::core
{
	class CttiSerializer;
	class CttiDeserializer;

	//! CTTI using constexpr. Heavily inspired by https://simoncoenen.com/blog/programming/StaticReflection.html

	class CttiTypeInfo
	{
	public:
		CttiTypeInfo(const char* typeName, const CttiTypeInfo* baseTypeInfo);
		~CttiTypeInfo();

		bool IsTypeOf(const string_hash_t& type) const;
		bool IsTypeOf(const CttiTypeInfo* typeInfo) const;

		template<typename T>
		bool IsTypeOf() const
		{
			return IsTypeOf(T::GetTypeInfoStatic());
		}

		const string_hash_t&  GetType()       const { return m_type; }
		const std::string&  GetTypeName()     const { return m_typeName; }
		const CttiTypeInfo* GetBaseTypeInfo() const { return m_baseTypeInfo; }

	private:
		std::string         m_typeName;
		string_hash_t       m_type;
		const CttiTypeInfo* m_baseTypeInfo = nullptr;
	};

	class CttiObject
	{
	public:
		virtual string_hash_t       GetType()     const { return GetTypeInfoStatic()->GetType(); }
		virtual const std::string&  GetTypeName() const { return GetTypeInfoStatic()->GetTypeName(); }
		virtual const CttiTypeInfo* GetTypeInfo() const { return GetTypeInfoStatic(); }
		static  string_hash_t       GetTypeStatic()     { return GetTypeInfoStatic()->GetType(); }
		static  const std::string&  GetTypeNameStatic() { return GetTypeInfoStatic()->GetTypeName(); }
		static  const CttiTypeInfo* GetTypeInfoStatic() { static const CttiTypeInfo typeInfoStatic("CttiObject", nullptr); return &typeInfoStatic; }

		virtual CttiObject* Clone() const { return nullptr; }

		bool IsTypeOf(const CttiTypeInfo* pTypeInfo) const;
		bool IsTypeOf(string_hash_t type) const;

		template<typename T>
		inline bool IsTypeOf() const
		{
			static_assert(std::is_base_of_v<CttiObject, T>, "T must derive directly or indirectly from CttiObject");
			return IsTypeOf(T::GetTypeInfoStatic());
		}

		template<typename T>
		T* As()
		{
			if (!IsTypeOf<T>())
				return nullptr;

			return reinterpret_cast<T*>(this);
		}

		virtual void NOTYPEINFO() = 0;

		virtual void Serialize(CttiSerializer&) const   {}
		virtual void Deserialize(CttiDeserializer&)     {}
		virtual void ResolvePointers(CttiDeserializer&) {}
	};

	//! CTTI based (de-)serialization

	class CttiDeserializer
	{
		bool                                      m_deepCopy;
		const uint8_t* m_bytes;
		size_t                                    m_size;
		std::unordered_map<uint64_t, CttiObject*> m_existingIdsToPointer;
		std::unordered_map<void*, uint64_t>       m_pointersToResolve;
		size_t                                    m_readCursor;
	
	public:
		explicit CttiDeserializer(const void* data, size_t length, bool deepCopy = true);
		~CttiDeserializer();
	
		void* ResolvePointerBase(void* ptr);
	
		template<typename T>
		T* ResolvePointer(T* ptr)
		{ return reinterpret_cast<T*>(ResolvePointerBase(ptr)); }
	
		const uint8_t* GetDataPtr();
	
		void   Reset();
		void   Skip(size_t numBytes);
		size_t GetReadCursor() const;
		void   SetReadCursor(size_t pos);
	
		CttiDeserializer& operator>>(CttiObject& s);
		CttiDeserializer& operator>>(std::string& s);
	
		template<typename T>
		T Peek() const
		{
			return *reinterpret_cast<const T*>(m_bytes + m_readCursor);
		}
	
		template<typename T>
		void TranslatePtr(T*& ptr)
		{
			uint64_t ptrId;
			operator>>(ptrId);
	
			bool shouldDeserializePointerContents = false;
	
			if (ptrId == uint64_t(-1))
			{
				ptrId = GetReadCursor();
				shouldDeserializePointerContents = true;
			}
	
			ptr = *reinterpret_cast<T**>(&ptrId);
			m_pointersToResolve[&ptr] = ptrId;
	
			if (shouldDeserializePointerContents)
				m_existingIdsToPointer[ptrId] = new T(*this);
		}
	
		template<typename T>
		CttiDeserializer& operator>>(T& t)
		{
			if (std::is_base_of_v<CttiObject, T>)
				reinterpret_cast<CttiObject*>(&t)->Deserialize(*this);
			else
			{
				t = *reinterpret_cast<const T*>(m_bytes + m_readCursor);
				m_readCursor += sizeof(T);
			}
			return *this;
		}
	
		template<typename T>
		CttiDeserializer& operator>>(T*& ptr)
		{
			TranslatePtr(ptr);
			return *this;
		}
	
		template<typename T>
		CttiDeserializer& operator>>(std::vector<T>& v)
		{
			v.clear();
	
			size_t numItems;
			operator>>(numItems);
	
			for (size_t i = 0; i < numItems; ++i)
			{
				T t;
				operator>>(t);
				v.push_back(t);
			}
	
			return *this;
		}
	
		template<typename T1, typename T2>
		CttiDeserializer& operator>>(std::unordered_map<T1, T2>& m)
		{
			m.clear();
	
			size_t numItems;
			operator>>(numItems);
	
			for (size_t i = 0; i < numItems; ++i)
			{
				T1 key; operator>>(key);
				T2 val; operator>>(val);
				m[key] = val;
			}
	
			return *this;
		}
	};
	
	class CttiSerializer
	{
		std::vector<uint8_t>                            m_bytes;
		std::unordered_map<const CttiObject*, uint64_t> m_existingPointersToId;
	
	public:
		explicit CttiSerializer();
		void Clear();
	
		void GetData(uint8_t*& data, size_t& length);
		CttiDeserializer ToDeserializer() const;
	
		CttiSerializer& operator<<(const CttiObject& s);
		CttiSerializer& operator<<(const std::string& s);
		CttiSerializer& operator<<(const char* s);
	
		template<typename T>
		CttiSerializer& operator<<(const T& t)
		{
			if (std::is_base_of<ga::core::CttiObject, T>::value)
				reinterpret_cast<const CttiObject*>(&t)->Serialize(*this);
			else
			{
				uint8_t* tBytes = (uint8_t*)(&t);
				for (size_t i = 0; i < sizeof(T); ++i)
					m_bytes.push_back(tBytes[i]);
			}
	
			return *this;
		}
	
		template<typename T>
		CttiSerializer& operator<<(T* ptr)
		{
			if (!ptr)
				(*this) << nullptr;
			else
			{
				if (m_existingPointersToId.count((CttiObject*)(ptr)))
					(*this) << m_existingPointersToId[(CttiObject*)(ptr)];
				else
				{
					(*this) << uint64_t(-1);
					m_existingPointersToId[(CttiObject*)(ptr)] = uint64_t(m_bytes.size());
					(*this) << *(CttiObject*)(ptr);
				}
			}
	
			return *this;
		}
	
		template<typename T>
		CttiSerializer& operator<<(const std::vector<T>& v)
		{
			(*this) << v.size();
	
			for (const T& t : v)
				(*this) << t;
	
			return *this;
		}
	
		template<typename T1, typename T2>
		CttiSerializer& operator<<(const std::unordered_map<T1, T2>& m)
		{
			(*this) << m.size();
	
			for (const auto& [key, value] : m)
			{
				(*this) << key;
				(*this) << value;
			}
	
			return *this;
		}
	};
}

//This macro automatically adds/implements the basics to get a CttiObject ready to use.
#define GA_CTTI_OBJECT(TYPENAME, BASETYPENAME)                                                                                                                                                               \
    public:                                                                                                                                                                                                  \
        using ClassName = TYPENAME;                                                                                                                                                                          \
        using Super     = BASETYPENAME;                                                                                                                                                                      \
        virtual       string_hash_t                GetType()           const override { return GetTypeInfoStatic()->GetType();     }                                                                         \
        virtual const std::string&                 GetTypeName()       const override { return GetTypeInfoStatic()->GetTypeName(); }                                                                         \
        virtual const ga::core::CttiTypeInfo*      GetTypeInfo()       const override { return GetTypeInfoStatic();                }                                                                         \
        static        string_hash_t                GetTypeStatic()                    { return GetTypeInfoStatic()->GetType();     }                                                                         \
        static  const std::string&                 GetTypeNameStatic()                { return GetTypeInfoStatic()->GetTypeName(); }                                                                         \
        static  const ga::core::CttiTypeInfo*      GetTypeInfoStatic()                { static const ga::core::CttiTypeInfo typeInfoStatic(#TYPENAME, Super::GetTypeInfoStatic()); return &typeInfoStatic; } \
	protected:                                                                                                                                                                                               \
		virtual void NOTYPEINFO() override {}                                                                                                                                                                \
	private:

#ifndef NDEBUG
# define CTTI_CAST(X, T) (X)->As<T>()
#else
# define CTTI_CAST(X, T) reinterpret_cast<T*>(X)
#endif

#endif /* GALLIUM__CORE__CTTI_H */