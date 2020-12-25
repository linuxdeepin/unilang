// © 2020 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Evaluation_h_
#define INC_Unilang_Evaluation_h_ 1

#include "Context.h" // for ReductionStatus, TermNode, Context, ContextHandler,
//	 ystdex::sfmt;
#include <ystdex/meta.hpp> // for ystdex::exclude_self_t;
#include <iterator> // for std::make_move_iterator, std::next;
#include <ystdex/algorithm.hpp> // for ystdex::split;
#include <algorithm> // for std::find_if;
#include <ystdex/type_traits.hpp> // for ystdex::enable_if_t, std::is_same;
#include <ystdex/scope_guard.hpp> // for ystdex::guard;
#include "Lexical.h" // for CategorizeLexeme, CategorizeBasicLexeme;

namespace Unilang
{

ReductionStatus
ReduceCombinedBranch(TermNode&, Context&);

// NOTE: This is the main entry of the evaluation algorithm.
ReductionStatus
ReduceOnce(TermNode&, Context&);


template<typename _fNext>
inline ReductionStatus
ReduceSubsequent(TermNode& term, Context& ctx, _fNext&& next)
{
	ctx.SetupFront(yforward(next));
	return ReduceOnce(term, ctx);
}


// NOTE: The collection of values of unit types.
enum class ValueToken
{
	Unspecified
};


class Continuation
{
public:
	using allocator_type
		= decltype(std::declval<const Context&>().get_allocator());

	ContextHandler Handler;

	template<typename _func, typename
		= ystdex::exclude_self_t<Continuation, _func>>
	inline
	Continuation(_func&& handler, allocator_type a)
		: Handler(ystdex::make_obj_using_allocator<ContextHandler>(a,
		yforward(handler)))
	{}
	template<typename _func, typename
		= ystdex::exclude_self_t<Continuation, _func>>
	inline
	Continuation(_func&& handler, const Context& ctx)
		: Continuation(yforward(handler), ctx.get_allocator())
	{}
	Continuation(const Continuation& cont, allocator_type a)
		: Handler(ystdex::make_obj_using_allocator<ContextHandler>(a,
		cont.Handler))
	{}
	Continuation(Continuation&& cont, allocator_type a)
		: Handler(ystdex::make_obj_using_allocator<ContextHandler>(a,
		std::move(cont.Handler)))
	{}
	Continuation(const Continuation&) = default;
	Continuation(Continuation&&) = default;

	Continuation&
	operator=(const Continuation&) = default;
	Continuation&
	operator=(Continuation&&) = default;

	[[nodiscard, gnu::const]] friend bool
	operator==(const Continuation& x, const Continuation& y) noexcept
	{
		return ystdex::ref_eq<>()(x, y);
	}

	ReductionStatus
	operator()(Context& ctx) const
	{
		return Handler(ctx.GetNextTermRef(), ctx);
	}
};


struct SeparatorTransformer
{
	template<typename _func, class _tTerm, class _fPred>
	[[nodiscard]] TermNode
	operator()(_func trans, _tTerm&& term, const ValueObject& pfx,
		_fPred filter) const
	{
		using it_t = decltype(std::make_move_iterator(term.begin()));

		return AddRange([&](TermNode& res, it_t b, it_t e){
			const auto add([&](TermNode& node, it_t i){
				node.Add(trans(*i));
			});

			if(b != e)
			{
				if(std::next(b) == e)
					add(res, b);
				else
				{
					auto child(Unilang::AsTermNode());

					do
					{
						add(child, b++);
					}
					while(b != e);
					res.Add(std::move(child));
				}
			}
		}, yforward(term), pfx, filter);
	}

	template<typename _func, class _tTerm, class _fPred>
	[[nodiscard]] TermNode
	AddRange(_func add_range, _tTerm&& term, const ValueObject& pfx,
		_fPred filter) const
	{
		using it_t = decltype(std::make_move_iterator(term.begin()));
		const auto a(term.get_allocator());
		auto res(Unilang::AsTermNode(yforward(term).Value));

		if(IsBranch(term))
		{
			res.Add(Unilang::AsTermNode(pfx));
			ystdex::split(std::make_move_iterator(term.begin()),
				std::make_move_iterator(term.end()), filter,
				[&](it_t b, it_t e){
				add_range(res, b, e);
			});
		}
		return res;
	}

	template<class _tTerm, class _fPred>
	[[nodiscard]] static TermNode
	Process(_tTerm&& term, const ValueObject& pfx, _fPred filter)
	{
		return SeparatorTransformer()([&](_tTerm&& tm) noexcept{
			return yforward(tm);
		}, yforward(term), pfx, filter);
	}

	template<class _fPred>
	static void
	ReplaceChildren(TermNode& term, const ValueObject& pfx,
		_fPred filter)
	{
		if(std::find_if(term.begin(), term.end(), filter) != term.end())
			term = Process(std::move(term), pfx, filter);
	}
};


class FormContextHandler
{
public:
	ContextHandler Handler;
	size_t Wrapping;

	template<typename _func, typename = ystdex::enable_if_t<
		!std::is_same<FormContextHandler&, _func&>::value>>
	FormContextHandler(_func&& f, size_t n = 0)
		: Handler(yforward(f)), Wrapping(n)
	{}
	FormContextHandler(const FormContextHandler&) = default;
	FormContextHandler(FormContextHandler&&) = default;

	FormContextHandler&
	operator=(const FormContextHandler&) = default;
	FormContextHandler&
	operator=(FormContextHandler&&) = default;

	ReductionStatus
	operator()(TermNode& term, Context& ctx) const
	{
		return CallN(Wrapping, term, ctx);
	}

private:
	ReductionStatus
	CallN(size_t, TermNode&, Context&) const;
};


enum WrappingKind : decltype(FormContextHandler::Wrapping)
{
	Form = 0,
	Strict = 1
};


template<size_t _vWrapping = Strict, class _tTarget, typename... _tParams>
inline void
RegisterHandler(_tTarget& target, string_view name, _tParams&&... args)
{
	Unilang::EmplaceLeaf<ContextHandler>(target, name,
		FormContextHandler(yforward(args)..., _vWrapping));
}

template<class _tTarget, typename... _tParams>
inline void
RegisterForm(_tTarget& target, string_view name, _tParams&&... args)
{
	Unilang::RegisterHandler<Form>(target, name,
		yforward(args)...);
}

template<class _tTarget, typename... _tParams>
inline void
RegisterStrict(_tTarget& target, string_view name, _tParams&&... args)
{
	Unilang::RegisterHandler<>(target, name, yforward(args)...);
}


using EnvironmentGuard = ystdex::guard<EnvironmentSwitcher>;


inline ReductionStatus
ReduceOnceLifted(TermNode& term, Context& ctx, TermNode& tm)
{
	LiftOther(term, tm);
	return ReduceOnce(term, ctx);
}

ReductionStatus
ReduceOrdered(TermNode&, Context&);

inline ReductionStatus
ReduceReturnUnspecified(TermNode& term) noexcept
{
	term.Value = ValueToken::Unspecified;
	return ReductionStatus::Clean;
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

void
BindParameter(const shared_ptr<Environment>&, const TermNode&, TermNode&);

} // namespace Unilang;

#endif

