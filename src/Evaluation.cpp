// © 2020 Uniontech Software Technology Co.,Ltd.

#include "Evaluation.h" // for ReductionStatus, TermNode, Context,
//	TermToNamePtr, ystdex::sfmt, std::string, Unilang::TryAccessLeaf,
//	TermReference, ContextHandler, std::prev, ystdex::begins_with;
#include <cassert> // for assert;
#include <ystdex/cctype.h> // for ystdex::isdigit;
#include "Exception.h" // for InvalidSyntax, BadIdentifier,
//	ListReductionFailure, ParameterMismatch;
#include <ystdex/memory.hpp> // for ystdex::share_move;
#include "Lexical.h" // for CategorizeBasicLexeme, LexemeCategory;
#include <ystdex/typeinfo.h> // for ystdex::type_id;
#include <ystdex/functional.hpp> // for ystdex::update_thunk;

namespace Unilang
{

namespace
{

ReductionStatus
ReduceLeaf(TermNode& term, Context& ctx)
{
	if(const auto p = TermToNamePtr(term))
	{
		auto res(ReductionStatus::Retained);
		const auto id(*p);

		assert(id.data());

		if(!id.empty())
		{
			const char f(id.front());

			if((id.size() > 1 && (f == '#'|| f == '+' || f == '-')
				&& id.find_first_not_of("+-") != string_view::npos)
				|| ystdex::isdigit(f))
				throw InvalidSyntax(ystdex::sfmt<std::string>(id.front()
					!= '#' ? "Unsupported literal prefix found in literal '%s'."
					: "Invalid literal '%s' found.", id.data()));

			auto pr(ctx.Resolve(ctx.GetRecordPtr(), id));

			if(pr.first)
			{
				auto& bound(*pr.first);

				if(const auto p
					= Unilang::TryAccessLeaf<const TermReference>(bound))
				{
					term.Subterms = bound.Subterms;
					term.Value = TermReference(*p);
				}
				else
					term.Value = TermReference(bound, std::move(pr.second));
				res = ReductionStatus::Neutral;
			}
			else
				throw BadIdentifier(id);
		}
		return CheckReducible(res) ? ReduceOnce(term, ctx) : res;
	}
	return ReductionStatus::Retained;
}

template<typename... _tParams>
ReductionStatus
CombinerReturnThunk(const ContextHandler& h, TermNode& term, Context& ctx,
	_tParams&&... args)
{
	static_assert(sizeof...(args) < 2, "Unsupported owner arguments found.");

	// TODO: Implement TCO.
	ctx.SetNextTermRef(term);
	ctx.SetupFront(std::bind([&](const _tParams&...){
		return RegularizeTerm(term, ctx.LastStatus);
	}, std::move(args)...));
	ctx.SetupFront(Continuation(std::ref(h), ctx));
	return ReductionStatus::Partial;
}

ReductionStatus
ReduceCombinedBranch(TermNode& term, Context& ctx)
{
	assert(IsBranchedList(term));

	auto& fm(AccessFirstSubterm(term));
	const auto p_ref_fm(Unilang::TryAccessLeaf<const TermReference>(fm));

	if(p_ref_fm)
	{
		if(const auto p_handler
			= Unilang::TryAccessLeaf<const ContextHandler>(p_ref_fm->get()))
			return CombinerReturnThunk(*p_handler, term, ctx);
	}
	if(const auto p_handler = Unilang::TryAccessTerm<ContextHandler>(fm))
	{
		// TODO: Implement TCO.
		auto p(ystdex::share_move(ctx.get_allocator(), *p_handler));

		return CombinerReturnThunk(*p, term, ctx, std::move(p));
	}
	throw
		ListReductionFailure("Invalid object found in the combiner position.");
}

ReductionStatus
ReduceBranch(TermNode& term, Context& ctx)
{
	if(IsBranch(term))
	{
		assert(term.size() != 0);
		if(term.size() == 1)
		{
			// NOTE: The following is necessary to prevent unbounded overflow in
			//	handling recursive subterms.
			auto term_ref(ystdex::ref(term));

			do
			{
				term_ref = AccessFirstSubterm(term_ref);
			}
			while(term_ref.get().size() == 1);
			return ReduceOnceLifted(term, ctx, term_ref);
		}
		if(IsEmpty(AccessFirstSubterm(term)))
			RemoveHead(term);
		assert(IsBranchedList(term));
		ctx.SetNextTermRef(term);
		ctx.LastStatus = ReductionStatus::Neutral;

		auto& sub(AccessFirstSubterm(term));

		ctx.SetupFront([&](Context& c){
			c.SetNextTermRef(term);
			return ReduceCombinedBranch(term, ctx);
		});
		ctx.SetupFront([&]{
			return ReduceOnce(sub, ctx);
		});
		return ReductionStatus::Partial;
	}
	return ReductionStatus::Retained;
}


inline ReductionStatus
ReduceChildrenOrderedAsync(TNIter, TNIter, Context&);

ReductionStatus
ReduceChildrenOrderedAsyncUnchecked(TNIter first, TNIter last, Context& ctx)
{
	assert(first != last);

	auto& term(*first++);

	return ReduceSubsequent(term, ctx, [=](Context& c){
		return ReduceChildrenOrderedAsync(first, last, c);
	});
}

inline ReductionStatus
ReduceChildrenOrderedAsync(TNIter first, TNIter last, Context& ctx)
{
	return first != last ? ReduceChildrenOrderedAsyncUnchecked(first, last, ctx)
		: ReductionStatus::Neutral;
}

ReductionStatus
ReduceSequenceOrderedAsync(TermNode& term, Context& ctx, TNIter i)
{
	assert(i != term.end());
	if(std::next(i) == term.end())
		return ReduceOnceLifted(term, ctx, *i);
	ctx.SetupFront([&, i]{
		return ReduceSequenceOrderedAsync(term, ctx, term.erase(i));
	});
	return ReduceOnce(*i, ctx);
}


template<typename _func>
auto
CheckSymbol(string_view n, _func f) -> decltype(f())
{
	if(CategorizeBasicLexeme(n) == LexemeCategory::Symbol)
		return f();
	throw ParameterMismatch(ystdex::sfmt(
		"Invalid token '%s' found for symbol parameter.", n.data()));
}

template<typename _func>
auto
CheckParameterLeafToken(string_view n, _func f) -> decltype(f())
{
	if(n != "#ignore")
		CheckSymbol(n, f);
}


using Action = function<void()>;

struct BindParameterObject
{
	lref<const EnvironmentReference> Referenced;

	BindParameterObject(const EnvironmentReference& r_env)
		: Referenced(r_env)
	{}

	template<typename _fCopy>
	void
	operator()(TermNode& o, _fCopy cp)
		const
	{
		if(const auto p = Unilang::TryAccessLeaf<TermReference>(o))
			cp(p->get());
		else
			cp(o);
	}
};


template<typename _fBindTrailing, typename _fBindValue>
class GParameterMatcher
{
public:
	_fBindTrailing BindTrailing;
	_fBindValue BindValue;

private:
	mutable Action act{};

public:
	template<class _type, class _type2>
	GParameterMatcher(_type&& arg, _type2&& arg2)
		: BindTrailing(yforward(arg)),
		BindValue(yforward(arg2))
	{}

	void
	operator()(const TermNode& t, TermNode& o,
		const EnvironmentReference& r_env) const
	{
		Match(t, o, r_env);
		while(act)
		{
			const auto a(std::move(act));

			a();
		}
	}

private:
	void
	Match(const TermNode& t, TermNode& o, const EnvironmentReference& r_env)
		const
	{
		if(IsList(t))
		{
			if(IsBranch(t))
			{
				const auto n_p(t.size());
				auto last(t.end());

				if(n_p > 0)
				{
					const auto& back(*std::prev(last));

					if(IsLeaf(back))
					{
						if(const auto p
							= Unilang::TryAccessLeaf<TokenValue>(back))
						{
							if(!p->empty() && p->front() == '.')
								--last;
						}
						else if(!IsList(back))
							throw ParameterMismatch(ystdex::sfmt(
								"Invalid term '%s' found for symbol parameter.",
								TermToString(back).c_str()));
					}
				}
				ResolveTerm([&, n_p](TermNode& nd,
					ResolvedTermReferencePtr p_ref){
					const bool ellipsis(last != t.end());
					const auto n_o(nd.size());

					if(n_p == n_o || (ellipsis && n_o >= n_p - 1))
						MatchSubterms(t.begin(), last, nd.begin(), nd.end(),
							p_ref ? p_ref->GetEnvironmentReference() : r_env,
							ellipsis);
					else if(!ellipsis)
						throw ArityMismatch(n_p, n_o);
					else
						ThrowInsufficientTermsError();
				}, o);
			}
			else
				ResolveTerm([&](const TermNode& nd, bool has_ref){
					if(nd)
						throw ParameterMismatch(ystdex::sfmt("Invalid nonempty"
							" operand value '%s' found for empty list"
							" parameter.", TermToStringWithReferenceMark(nd,
							has_ref).c_str()));
				}, o);
		}
		else
		{
			const auto& tp(t.Value.type());
		
			if(tp == ystdex::type_id<TermReference>())
				ystdex::update_thunk(act, [&]{
					Match(t.Value.GetObject<TermReference>().get(), o, r_env);
				});
			else if(tp == ystdex::type_id<TokenValue>())
				BindValue(t.Value.GetObject<TokenValue>(), o, r_env);
			else
				throw ParameterMismatch(ystdex::sfmt("Invalid parameter value"
					" '%s' found.", TermToString(t).c_str()));
		}
	}

	void
	MatchSubterms(TNCIter i, TNCIter last, TNIter j, TNIter o_last,
		const EnvironmentReference& r_env, bool ellipsis) const
	{
		if(i != last)
		{
			ystdex::update_thunk(act, [=, &r_env]{
				return MatchSubterms(std::next(i), last, std::next(j), o_last,
					r_env, ellipsis);
			});
			assert(j != o_last);
			Match(*i, *j, r_env);
		}
		else if(ellipsis)
		{
			const auto& lastv(last->Value);

			assert(lastv.type() == ystdex::type_id<TokenValue>());
			BindTrailing(j, o_last, lastv.GetObject<TokenValue>(), r_env);
		}
	}
};

template<typename _fBindTrailing, typename _fBindValue>
[[nodiscard]] inline GParameterMatcher<_fBindTrailing, _fBindValue>
MakeParameterMatcher(_fBindTrailing bind_trailing_seq, _fBindValue bind_value)
{
	return GParameterMatcher<_fBindTrailing, _fBindValue>(
		std::move(bind_trailing_seq), std::move(bind_value));
}

} // unnamed namespace;

void
ThrowInsufficientTermsError()
{
	throw ParameterMismatch("Insufficient terms found for list parameter.");
}


ReductionStatus
ReduceOnce(TermNode& term, Context& ctx)
{
	return term.Value ? ReduceLeaf(term, ctx)
		: ReduceBranch(term, ctx);
}

ReductionStatus
ReduceOrdered(TermNode& term, Context& ctx)
{
	if(IsBranch(term))
		return ReduceSequenceOrderedAsync(term, ctx, term.begin());
	term.Value = ValueToken::Unspecified;
	return ReductionStatus::Retained;
}


ReductionStatus
FormContextHandler::CallN(size_t n, TermNode& term, Context& ctx) const
{
	if(n == 0 || term.size() <= 1)
		return Handler(ctx.GetNextTermRef(), ctx);
	ctx.SetupFront([&, n](Context& c){
		c.SetNextTermRef( term);
		return CallN(n - 1, term, c);
	});
	ctx.SetNextTermRef(term);
	assert(!term.empty());
	ReduceChildrenOrderedAsyncUnchecked(std::next(term.begin()), term.end(),
		ctx);
	return ReductionStatus::Partial;
}


void
BindParameter(const shared_ptr<Environment>& p_env, const TermNode& t,
	TermNode& o)
{
	auto& env(*p_env);

	MakeParameterMatcher([&](TNIter first, TNIter last, string_view id,
		const EnvironmentReference& r_env){
		assert(ystdex::begins_with(id, "."));
		id.remove_prefix(1);
		if(!id.empty())
		{
			if(!id.empty())
			{
				TermNode::Container con(t.get_allocator());

				for(; first != last; ++first)
					BindParameterObject{r_env}(*first,
						[&](const TermNode& tm){
						con.emplace_back(tm.Subterms, tm.Value);
					});
				env.Bind(id, TermNode(std::move(con)));
			}
		}
	}, [&](const TokenValue& n, TermNode& b, const EnvironmentReference& r_env){
		CheckParameterLeafToken(n, [&]{
			if(!n.empty())
			{
				string_view id(n);

				if(!id.empty())
					BindParameterObject{r_env}(b,
						[&](const TermNode& tm){
						env.Bind(id, tm);
					});
			}
		});
	})(t, o, p_env);
}

} // namespace Unilang;

