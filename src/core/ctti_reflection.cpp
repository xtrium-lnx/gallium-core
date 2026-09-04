#include <gallium/core/ctti.h>

using namespace ga::core;

CttiTypeInfo::CttiTypeInfo(const char* typeName, const CttiTypeInfo* baseTypeInfo)
	: m_typeName(typeName)
	, m_type(ga::hash_string(typeName))
	, m_baseTypeInfo(baseTypeInfo)
{ }

CttiTypeInfo::~CttiTypeInfo()
{ }

bool CttiTypeInfo::IsTypeOf(const CttiTypeInfo* typeInfo) const
{
	if (!typeInfo)
		return false;

	return IsTypeOf(typeInfo->GetType());
}

bool CttiTypeInfo::IsTypeOf(const string_hash_t& type) const
{
	const CttiTypeInfo* info = this;

	while (info != nullptr)
	{
		if (type == info->m_type)
			return true;

		info = info->m_baseTypeInfo;
	}

	return false;
}

bool CttiObject::IsTypeOf(const CttiTypeInfo* pTypeInfo) const
{
	return GetTypeInfo()->IsTypeOf(pTypeInfo);
}

bool CttiObject::IsTypeOf(string_hash_t type) const
{
	return GetTypeInfo()->IsTypeOf(type);
}
