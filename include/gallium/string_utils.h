#ifndef GALLIUM__STRINGUTILS_H
#define GALLIUM__STRINGUTILS_H
#pragma once

#include <string>
#include <vector>

namespace std
{
	string to_string(void* ptr);
}

namespace ga
{
	//Splits text at each delimiter and fills tokens.
	void   string_split(std::vector<std::string>& tokens, const std::string& text, char delimiter);

	//Joins a token array into a single string.
	std::string string_join(const std::vector<std::string>& tokens, char delimiter);

	//Returns a string that is s, with all occurences of search replaced by replace.
	std::string string_replace(std::string s, const std::string& search, const std::string& replace);

	//Returns true if fullString ends with ending, false otherwise.
	bool   string_endsWith(const std::string& fullString, const std::string& ending);

	//Returns true if fullString starts with ending, false otherwise.
	bool   string_startsWith(const std::string& fullString, const std::string& starting);

	//Returns the number of occurences of character c in string fullString.
	uint32_t string_count(const std::string& fullString, char c);
}

#endif /* GALLIUM__STRINGUTILS_H */

