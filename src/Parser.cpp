// SPDX-FileCopyrightText: 2020-2022 UnionTech Software Technology Co.,Ltd.

#include "Parser.h"
#include <cassert> // for assert;
#include "Lexical.h" // for IsDelimiter, IsGraphicalDelimiter;

namespace Unilang
{

namespace
{

template<typename _fAdd, typename _fAppend, class _tParseResult>
void
UpdateByteRaw(_fAdd add, _fAppend append, _tParseResult& res, bool got_delim,
	const LexicalAnalyzer& lexer, string& cbuf, bool& update_current)
{
	const auto len(cbuf.length());

	assert(!(res.empty() && update_current) && "Invalid state found.");
	if(len > 0 && !lexer.GetUnescapeContext().IsHandling())
	{
		if(len == 1)
		{
			const auto update_c([&](char c){
				if(update_current)
					append(res, c);
				else
					add(res, string({c}, res.get_allocator()));
			});
			const char c(cbuf.back());
			const bool unquoted(lexer.GetDelimiter() == char());

			if(got_delim)
			{
				update_c(c);
				update_current = !unquoted;
			}
			else if(unquoted && IsDelimiter(c))
			{
				if(IsGraphicalDelimiter(c))
					add(res, string({c}, res.get_allocator()));
				update_current = {};
			}
			else
			{
				update_c(c);
				update_current = true;
			}
		}
		else if(update_current)
			append(res, std::move(cbuf));
		else
			add(res, std::move(cbuf));
		cbuf.clear();
	}
}

template<class _tParseResult>
struct SequenceAdd final
{
	template<typename _tParam>
	void
	operator()(_tParseResult& res, _tParam&& arg) const
	{
		res.push_back(yforward(arg));
	}
};

template<class _tParseResult>
struct SequenceAppend final
{
	template<typename _tParam>
	void
	operator()(_tParseResult& res, _tParam&& arg) const
	{
		res.back() += yforward(arg);
	}
};

template<class _tParseResult>
struct SourcedSequenceAdd final
{
	const SourcedByteParser& Parser;

	template<typename _tParam>
	void
	operator()(_tParseResult& res, _tParam&& arg) const
	{
		res.emplace_back(Parser.GetSourceLocation(), yforward(arg));
	}
};

template<class _tParseResult>
struct SourcedSequenceAppend final
{
	template<typename _tParam>
	void
	operator()(_tParseResult& res, _tParam&& arg) const
	{
		res.back().second += yforward(arg);
	}
};

} // unnamed namespace;


void
ByteParser::Update(bool got_delim)
{
	UpdateByteRaw(SequenceAdd<ParseResult>(), SequenceAppend<ParseResult>(),
		lexemes, got_delim, GetLexerRef(), GetBufferRef(), update_current);
}


void
SourcedByteParser::Update(bool got_delim)
{
	UpdateByteRaw(SourcedSequenceAdd<ParseResult>{*this},
		SourcedSequenceAppend<ParseResult>(), lexemes, got_delim, GetLexerRef(),
		GetBufferRef(), update_current);
}

} // namespace Unilang;

