// © 2020-2022 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Syntax_h_
#define INC_Unilang_Syntax_h_ 1

#include "TermNode.h" // for TermNode, stack;
#include "Exception.h" // for UnilangException;
#include <cassert> // for assert;
#include "Lexical.h" // for ThrowMismatchBoundaryToken;

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
		return
			Unilang::AsTermNode(Allocator, std::allocator_arg, Allocator, val);
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
		if(*first == "(" || *first == "[" || *first == "{")
		{
			tms.push(AsTermNode(a));
			tms.top().Value = *first;
		}
		else if(!(*first == ")" || *first == "]" || *first == "}"))
		{
			assert(!tms.empty());
			tms.top().Add(tokenize(*first));
		}
		else if(tms.size() != 1)
		{
			auto tm(std::move(tms.top()));

			tms.pop();

			const auto rtok(*first);
			const auto& ltok(tm.Value.GetObject<decltype(rtok)>());

			if(rtok == ")" && ltok != "(")
				ThrowMismatchBoundaryToken('(', ')');
			if(rtok == "]" && ltok != "[")
				ThrowMismatchBoundaryToken('[', ']');
			if(rtok == "}" && ltok != "{")
				ThrowMismatchBoundaryToken('{', '}');
			tm.Value.Clear();
			assert(!tms.empty());
			tms.top().Add(std::move(tm));
		}
		else
			return first;
	if(tms.size() == 1)
		return first;
	throw UnilangException("Redundant '(', '[' or '{' found.");
}

} // namespace Unilang;

#endif

