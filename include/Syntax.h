// SPDX-FileCopyrightText: 2020-2022 UnionTech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Syntax_h_
#define INC_Unilang_Syntax_h_ 1

#include "Parser.h" // for ToLexeme, ThrowMismatchBoundaryToken;
#include "TermNode.h" // for TermNode, stack;
#include "Exception.h" // for UnilangException;
#include <cassert> // for assert;
#include <ystdex/type_traits.hpp> // for ystdex::remove_reference_t;

namespace Unilang
{

struct LexemeTokenizer final
{
	TermNode::allocator_type Allocator;

	LexemeTokenizer(TermNode::allocator_type a = {})
		: Allocator(a)
	{}

	template<class _type>
	YB_ATTR_nodiscard YB_PURE inline TermNode
	operator()(const _type& val) const
	{
		return Unilang::AsTermNode(Allocator, std::allocator_arg, Allocator,
			ToLexeme(val));
	}
};


template<typename _tIn>
YB_ATTR_nodiscard inline _tIn
ReduceSyntax(TermNode& term, _tIn first, _tIn last)
{
	return ReduceSyntax(term, first, last,
		LexemeTokenizer{term.get_allocator()});
}
template<typename _tIn, typename _fTokenize>
YB_ATTR_nodiscard _tIn
ReduceSyntax(TermNode& term, _tIn first, _tIn last, _fTokenize tokenize)
{
	const auto a(term.get_allocator());
	stack<TermNode> tms(a);
	struct Guard final
	{
		TermNode& term;
		stack<TermNode>& tms;

		~Guard()
		{
			assert(!tms.empty());
			term = std::move(tms.top());
		}
	} gd{term, tms};

	tms.push(std::move(term));
	for(; first != last; ++first)
	{
		const auto& val(*first);
		const auto& lexeme(ToLexeme(val));

		if(lexeme == "(" || lexeme == "[" || lexeme == "{")
		{
			tms.push(AsTermNode(a));
			tms.top().Value = lexeme;
		}
		else if(!(lexeme == ")" || lexeme == "]" || lexeme == "}"))
		{
			assert(!tms.empty());
			tms.top().Add(tokenize(val));
		}
		else if(tms.size() != 1)
		{
			auto tm(std::move(tms.top()));

			tms.pop();

			const auto& ltok(tm.Value.GetObject<
				ystdex::remove_reference_t<decltype(lexeme)>>());

			if(lexeme == ")" && ltok != "(")
				ThrowMismatchBoundaryToken('(', ')');
			if(lexeme == "]" && ltok != "[")
				ThrowMismatchBoundaryToken('[', ']');
			if(lexeme == "}" && ltok != "{")
				ThrowMismatchBoundaryToken('{', '}');
			tm.Value.Clear();
			assert(!tms.empty());
			tms.top().Add(std::move(tm));
		}
		else
			return first;
	}
	if(tms.size() == 1)
		return first;
	throw UnilangException("Redundant '(', '[' or '{' found.");
}

} // namespace Unilang;

#endif

