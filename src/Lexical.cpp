// © 2020-2021 Uniontech Software Technology Co.,Ltd.

#include "Lexical.h"

namespace Unilang
{

bool
LexicalAnalyzer::UpdateBack(char& b, char c)
{
	switch(c)
	{
		case '\'':
		case '"':
			if(ld == char())
			{
				ld = c;
				return true;
			}
			else if(ld == c)
			{
				ld = char();
				return true;
			}
			break;
		case '\f':
		case '\n':
		case '\t':
		case '\v':
			if(ld == char())
			{
				b = ' ';
				break;
			}
			YB_ATTR_fallthrough;
		default:
			break;
	}
	return {};
}


char
CheckLiteral(string_view sv) noexcept
{
	assert(sv.data());
	if(sv.length() > 1)
		if(const char c = sv.front() == sv.back() ? sv.front() : char())
		{
			if(c == '\'' || c == '"')
				return c;
		}
	return {};
}


LexemeCategory
CategorizeBasicLexeme(string_view id) noexcept
{
	assert(id.data());

	const auto c(CheckLiteral(id));

	if(c == '\'')
		return LexemeCategory::Code;
	if(c != char())
		return LexemeCategory::Data;
	return LexemeCategory::Symbol;
}

} // namespace Unilang;

