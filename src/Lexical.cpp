// © 2020-2021 Uniontech Software Technology Co.,Ltd.

#include "Lexical.h"

namespace Unilang
{

bool
Unescape(string& buf, char c, UnescapeContext& uctx, char ld)
{
	if(uctx.Length == 1)
	{
		uctx.VerifyBufferLength(buf.length());

		const auto i(uctx.Start);

		assert(i == buf.length() - 1 && "Invalid buffer found.");
		switch(c)
		{
		case '\\':
			buf[i] = '\\';
			break;
		case 'a':
			buf[i] = '\a';
			break;
		case 'b':
			buf[i] = '\b';
			break;
		case 'f':
			buf[i] = '\f';
			break;
		case 'n':
			buf[i] = '\n';
			break;
		case 'r':
			buf[i] = '\r';
			break;
		case 't':
			buf[i] = '\t';
			break;
		case 'v':
			buf[i] = '\v';
			break;
		case '\n':
			buf.pop_back();
			break;
		case '\'':
		case '"':
			if(c == ld)
			{
				buf[i] = ld;
				break;
			}
			YB_ATTR_fallthrough;
		default:
			uctx.Clear();
			buf += c;
			return {};
		}
		uctx.Clear();
		return true;
	}
	buf += c;
	return {};
}


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

