// SPDX-FileCopyrightText: 2020-2021 UnionTech Software Technology Co.,Ltd.

#include "Lexical.h" // for string, assert, std::string;
#include <ystdex/string.hpp> // for ystdex::get_mid, ystdex::quote;
#include "Exception.h" // for UnilangException;

namespace Unilang
{

bool
HandleBackslashPrefix(string_view buf, UnescapeContext& uctx)
{
	assert(!buf.empty() && "Invalid buffer found.");
	if(!uctx.IsHandling() && buf.back() == '\\')
	{
		uctx.Start = buf.length() - 1;
		uctx.Length = 1;
		return true;
	}
	return {};
}

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

string
Escape(string_view sv)
{
	assert(bool(sv.data()));

	char last{};
	string res;

	res.reserve(sv.length());
	for(char c : sv)
	{
		char unescaped{};

		switch(c)
		{
		case '\a':
			unescaped = 'a';
			break;
		case '\b':
			unescaped = 'b';
			break;
		case '\f':
			unescaped = 'f';
			break;
		case '\n':
			unescaped = 'n';
			break;
		case '\r':
			unescaped = 'r';
			break;
		case '\t':
			unescaped = 't';
			break;
		case '\v':
			unescaped = 'v';
			break;
		case '\'':
		case '"':
			unescaped = c;
		}
		if(last == '\\')
		{
			if(c == '\\')
			{
				yunseq(last = char(), res += '\\');
				continue;
			}
			switch(c)
			{
			case 'a':
			case 'b':
			case 'f':
			case 'n':
			case 'r':
			case 't':
			case 'v':
			case '\'':
			case '"':
				res += '\\';
			}
		}
		if(unescaped == char())
			res += c;
		else
		{
			res += '\\';
			res += unescaped;
			unescaped = char();
		}
		last = c;
	}
	return res;
}

string
EscapeLiteral(string_view sv)
{
	const char c(CheckLiteral(sv));
	auto content(Escape(c == char() ? sv : ystdex::get_mid(string(sv))));

	if(!content.empty() && content.back() == '\\')
		content += '\\';
	return c == char() ? std::move(content) : ystdex::quote(content, c);
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


void
ThrowMismatchBoundaryToken(char ldelim, char rdelim)
{
	throw UnilangException(std::string("Mismatched right boundary token '")
		+ rdelim + "' found for left boundary token '" + ldelim + "'.");
}

} // namespace Unilang;

