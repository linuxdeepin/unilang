// © 2020-2021 Uniontech Software Technology Co.,Ltd.

#include "Parser.h"
#include "Lexical.h" // for IsDelimiter, IsGraphicalDelimiter;
#include <cassert> // for assert;

namespace Unilang
{

void
ByteParser::operator()(char c)
{
	auto& lexer(GetLexerRef());
	const auto add = [&](string&& arg){
		lexemes.push_back(yforward(arg));
	};
	auto& cbuf(GetBufferRef());

	cbuf += c;

	const bool got_delim(UpdateBack(c));
	const auto len(cbuf.length());

	assert(!(lexemes.empty() && update_current) && "Invalid state found.");
	if(len > 0)
	{
		if(len == 1)
		{
			const auto update_c([&](char b){
				if(update_current)
					lexemes.back() += b;
				else
					add(string({b}, lexemes.get_allocator()));
			});
			const char b(cbuf.back());
			const bool unquoted(lexer.GetDelimiter() == char());

 			if(got_delim)
			{
				update_c(b);
				update_current = !unquoted;
			}
			else if(unquoted && IsDelimiter(b))
			{
				if(IsGraphicalDelimiter(b))
					add(string({b}, lexemes.get_allocator()));
				update_current = {};
			}
			else
			{
				update_c(b);
				update_current = true;
			}
		}
		else if(update_current)
			lexemes.back() += std::move(cbuf);
		else
			add(std::move(cbuf));
		cbuf.clear();
	}
}

bool
ByteParser::UpdateBack(char c)
{
	auto& b(GetBackRef());
	auto& ld(GetLexerRef().Delimiter);

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

} // namespace Unilang;

