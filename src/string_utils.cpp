#include <gallium/string_utils.h>

#include <format>

void ga::string_split(std::vector<std::string>& tokens, const std::string& text, char delimiter)
{
	size_t start = 0;
	size_t end = 0;
	while ((end = text.find(delimiter, start)) != std::string::npos)
	{
		tokens.push_back(text.substr(start, end - start));
		start = end + 1;
	}
	tokens.push_back(text.substr(start));
}

std::string ga::string_join(const std::vector<std::string>& tokens, char delimiter)
{
	std::string result;

	for (size_t i = 0; i < tokens.size(); ++i)
	{
		if (i)
			result += delimiter;

		result += tokens[i];
	}

	return result;
}

std::string ga::string_replace(std::string s, const std::string& from, const std::string& to)
{
	if (from.empty())
		return s;

	size_t pos = 0;
	while ((pos = s.find(from, pos)) != std::string::npos)
	{
		s.replace(pos, from.length(), to);
		pos += to.length();
	}
	return s;
}

bool ga::string_endsWith(std::string const& fullString, std::string const& ending)
{
	if (fullString.length() >= ending.length())
		return (0 == fullString.compare(fullString.length() - ending.length(), ending.length(), ending));

	return false;
}

bool ga::string_startsWith(std::string const& fullString, std::string const& starting)
{
	if (fullString.length() >= starting.length())
		return (0 == fullString.compare(0, starting.length(), starting));

	return false;
}

uint32_t ga::string_count(const std::string& fullString, char c)
{
	uint32_t total = 0;

	for (size_t i = 0; i < fullString.length(); ++i)
	{
		char leChar = fullString[i];
		if (leChar == c)
			++total;
	}

	return total;
}

std::string std::to_string(void* ptr)
{
	return std::format("0x{:016x}", *reinterpret_cast<size_t*>(&ptr));
}

