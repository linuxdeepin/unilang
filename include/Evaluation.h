// © 2020-2021 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Evaluation_h_
#define INC_Unilang_Evaluation_h_ 1

#include "Context.h" // for ReductionStatus, TermNode, Context,
//	YSLib::AreEqualHeld, YSLib::GHEvent, ContextHandler, std::allocator_arg_t;
#include <ystdex/string.hpp> // for ystdex::sfmt;
#include <ystdex/meta.hpp> // for ystdex::exclude_self_t;
#include <iterator> // for std::make_move_iterator, std::next;
#include <ystdex/algorithm.hpp> // for ystdex::split;
#include <algorithm> // for std::find_if;
#include <ystdex/operators.hpp> // for ystdex::equality_comparable;
#include <ystdex/function.hpp> // for ystdex::make_function_type_t,
//	ystdex::make_parameter_list_t;
#include <ystdex/type_op.hpp> // for ystdex::exclude_self_params_t;
#include <ystdex/scope_guard.hpp> // for ystdex::guard;
#include "TermAccess.h" // for TokenValue;

namespace Unilang
{

ReductionStatus
ReduceCombinedBranch(TermNode&, Context&);

// NOTE: This is the main entry of the evaluation algorithm.
ReductionStatus
ReduceOnce(TermNode&, Context&);


// NOTE: The collection of values of unit types.
enum class ValueToken
{
	Unspecified
};


struct SeparatorTransformer
{
	template<typename _func, class _tTerm, class _fPred>
	YB_ATTR_nodiscard TermNode
	operator()(_func trans, _tTerm&& term, const ValueObject& pfx,
		_fPred filter) const
	{
		using it_t = decltype(std::make_move_iterator(term.begin()));

		return AddRange([&](TermNode& res, it_t b, it_t e){
			const auto add([&](TermNode& node, it_t i){
				node.Add(trans(Unilang::Deref(i)));
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
	YB_ATTR_nodiscard TermNode
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
	YB_ATTR_nodiscard static TermNode
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


template<typename _func>
class WrappedContextHandler
	: private ystdex::equality_comparable<WrappedContextHandler<_func>>
{
public:
	_func Handler;

	template<typename... _tParams, typename
		= ystdex::exclude_self_params_t<WrappedContextHandler, _tParams...>>
	WrappedContextHandler(_tParams&&... args)
		: Handler(yforward(args)...)
	{}
	WrappedContextHandler(const WrappedContextHandler&) = default;
	WrappedContextHandler(WrappedContextHandler&&) = default;

	WrappedContextHandler&
	operator=(const WrappedContextHandler&) = default;
	WrappedContextHandler&
	operator=(WrappedContextHandler&&) = default;

	template<typename... _tParams>
	ReductionStatus
	operator()(_tParams&&... args) const
	{
		Handler(yforward(args)...);
		return ReductionStatus::Clean;
	}

	YB_ATTR_nodiscard friend bool
	operator==(const WrappedContextHandler& x, const WrappedContextHandler& y)
	{
		return YSLib::AreEqualHeld(x.Handler, y.Handler);
	}

	friend void
	swap(WrappedContextHandler& x, WrappedContextHandler& y) noexcept
	{
		return swap(x.Handler, y.Handler);
	}
};

template<class _tDst, typename _func>
YB_ATTR_nodiscard YB_PURE inline _tDst
WrapContextHandler(_func&& h, ystdex::false_)
{
	return WrappedContextHandler<YSLib::GHEvent<ystdex::make_function_type_t<
		void, ystdex::make_parameter_list_t<typename _tDst::BaseType>>>>(
		yforward(h));
}
template<class, typename _func>
YB_ATTR_nodiscard YB_PURE inline _func
WrapContextHandler(_func&& h, ystdex::true_)
{
	return yforward(h);
}
template<class _tDst, typename _func>
YB_ATTR_nodiscard YB_PURE inline _tDst
WrapContextHandler(_func&& h)
{
	using BaseType = typename _tDst::BaseType;

	return Unilang::WrapContextHandler<_tDst>(yforward(h), ystdex::or_<
		std::is_constructible<BaseType, _func>,
		std::is_constructible<BaseType, ystdex::expanded_caller<
		typename _tDst::FuncType, ystdex::decay_t<_func>>>>());
}
template<class _tDst, typename _func, class _tAlloc>
YB_ATTR_nodiscard YB_PURE inline _tDst
WrapContextHandler(_func&& h, const _tAlloc& a, ystdex::false_)
{
	return WrappedContextHandler<YSLib::GHEvent<ystdex::make_function_type_t<
		void, ystdex::make_parameter_list_t<typename _tDst::BaseType>>>>(
		std::allocator_arg, a, yforward(h));
}
template<class, typename _func, class _tAlloc>
YB_ATTR_nodiscard YB_PURE inline _func
WrapContextHandler(_func&& h, const _tAlloc&, ystdex::true_)
{
	return yforward(h);
}
template<class _tDst, typename _func, class _tAlloc>
YB_ATTR_nodiscard YB_PURE inline _tDst
WrapContextHandler(_func&& h, const _tAlloc& a)
{
	using BaseType = typename _tDst::BaseType;

	return Unilang::WrapContextHandler<_tDst>(yforward(h), a, ystdex::or_<
		std::is_constructible<BaseType, _func>,
		std::is_constructible<BaseType, ystdex::expanded_caller<
		typename _tDst::FuncType, ystdex::decay_t<_func>>>>());
}


class FormContextHandler
{
public:
	ContextHandler Handler;
	size_t Wrapping;

	template<typename _func,
		typename = ystdex::exclude_self_t<FormContextHandler, _func>>
	FormContextHandler(_func&& f, size_t n = 0)
		: Handler(Unilang::WrapContextHandler<ContextHandler>(yforward(f))),
		Wrapping(n)
	{}
	template<typename _func, class _tAlloc>
	FormContextHandler(std::allocator_arg_t, const _tAlloc& a, _func&& f,
		size_t n = 0)
		: Handler(std::allocator_arg, a, Unilang::WrapContextHandler<
		ContextHandler>(yforward(f), a)), Wrapping(n)
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


YB_ATTR_nodiscard YB_PURE inline bool
IsIgnore(const TokenValue& s) noexcept
{
	return s == "#ignore";
}

void
CheckParameterTree(const TermNode&);

void
BindParameter(const shared_ptr<Environment>&, const TermNode&, TermNode&);

void
BindParameterWellFormed(const shared_ptr<Environment>&, const TermNode&,
	TermNode&);

} // namespace Unilang;

#endif

