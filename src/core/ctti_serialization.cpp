#include <gallium/core/ctti.h>

using namespace ga::core;

CttiSerializer::CttiSerializer()
{
	Clear();

#ifndef NDEBUG
	// Reserve 1MB to avoid spurious reallocations during memory analysis
	m_bytes.reserve(1024 * 1024);
#endif // !NDEBUG
}

void CttiSerializer::Clear()
{
	m_bytes.clear();
	m_existingPointersToId.clear();
}

void CttiSerializer::GetData(uint8_t*& data, size_t& length)
{
	data = &m_bytes[0];
	length = m_bytes.size();
}

CttiDeserializer CttiSerializer::ToDeserializer() const
{
	return CttiDeserializer(&m_bytes[0], m_bytes.size());
}

CttiSerializer& CttiSerializer::operator<<(const CttiObject& s)
{
	s.Serialize(*this);
	return *this;
}

CttiSerializer& CttiSerializer::operator<<(const std::string& s)
{
	(*this) << s.length();

	for (auto& c : s)
		(*this) << c;

	return *this;
}

CttiSerializer& CttiSerializer::operator<<(const char* s)
{
	return operator<<(std::string(s));
}

// ----------------------------------------------------------------------------

CttiDeserializer::CttiDeserializer(const void* data, size_t size, bool deepCopy /* = true */)
	: m_readCursor(0)
	, m_deepCopy(deepCopy)
	, m_size(size)
{
	if (m_deepCopy)
	{
		m_bytes = new uint8_t[size];

		if (data)
			std::copy_n(reinterpret_cast<const uint8_t*>(data), size, const_cast<uint8_t*>(m_bytes));
	}
	else
		m_bytes = reinterpret_cast<const uint8_t*>(data);

	m_existingIdsToPointer[0] = nullptr;
}

CttiDeserializer::~CttiDeserializer()
{
	if (m_deepCopy)
		delete[] m_bytes;
}

void* CttiDeserializer::ResolvePointerBase(void* ptr)
{
	uint64_t ptrId = *reinterpret_cast<uint64_t*>(&ptr);
	if (m_existingIdsToPointer.count(ptrId))
		return m_existingIdsToPointer[ptrId];

	return nullptr;
}

const uint8_t* CttiDeserializer::GetDataPtr()
{
	return m_bytes;
}

CttiDeserializer& CttiDeserializer::operator>>(CttiObject& s)
{
	s.Deserialize(*this);
	return *this;
}

CttiDeserializer& CttiDeserializer::operator>>(std::string& s)
{
	size_t length;
	operator>>(length);

	s = std::string(reinterpret_cast<const char*>(m_bytes + m_readCursor), length);
	m_readCursor += length;
	return *this;
}

void CttiDeserializer::Reset()
{
	m_readCursor = 0;
}

size_t CttiDeserializer::GetReadCursor() const
{
	return m_readCursor;
}

void CttiDeserializer::SetReadCursor(size_t offset)
{
	m_readCursor = offset;
}

void CttiDeserializer::Skip(size_t numBytes)
{
	m_readCursor += numBytes;
}