// SPDX-FileCopyrightText: 2020-2023 UnionTech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Evaluation_h_
#define INC_Unilang_Evaluation_h_ 1

#include "TermNode.h" // for TermNode, ValueObject, Unilang::AsTermNode,
//	Unilang::Deref, string_view, in_place_type, shared_ptr,
//	Unilang::allocate_shared, IsSticky, IsLeaf, IsTyped, ystdex::ref_eq,
//	AccessFirstSubterm, Unilang::AsTermNodeTagged, TermTags, type_id,
//	YSLib::Logger, GetLValueTagsOf, lref, TNIter;
#include "Context.h" // for ReductionStatus, Context, YSLib::AreEqualHeld,
//	YSLib::GHEvent, allocator_arg, ContextHandler, std::allocator_arg_t,
//	Unilang::EmplaceLeaf, EnvironmentSwitcher,
//	Unilang::SwitchToFreshEnvironment, Unilang::AssignParent, SingleWeakParent,
//	HasValue;
#include <iterator> // for std::make_move_iterator, std::next;
#include <ystdex/algorithm.hpp> // for ystdex::split;
#include "Parser.h" // for SourceLocation;
#include <ystdex/operators.hpp> // for ystdex::equality_comparable;
#include <ystdex/type_op.hpp> // for ystdex::exclude_self_params_t;
#include <ystdex/function.hpp> // for ystdex::make_function_type_t,
//	ystdex::make_parameter_list_t, function;
#include <ystdex/integral_constant.hpp> // for ystdex::or_, ystdex::nor_,
//	ystdex::size_t_;
#include <ystdex/expanded_function.hpp> // for ystdex::expanded_caller;
#include <ystdex/type_op.hpp> // for ystdex::is_instance_of;
#include <ystdex/variadic.hpp> // for ystdex::vseq::_a;
#include <tuple> // for std::tuple, std::forward_as_tuple;
#include <type_traits> // for std::is_same, std;
#include <ystdex/meta.hpp> // for ystdex::enable_if_t, ystdex::is_same_param;
#include <ystdex/apply.hpp> // for ystdex::apply;
#include <cassert> // for assert;
#include "TermAccess.h" // for TermReference, EnvironmentReference,
//	TryAccessLeafAtom, TryAccessLeaf, AssertCombiningTerm, IsReferenceTerm;
#include <utility> // for std::declval;
#include "Exception.h" // for ArityMismatch, InvalidReference;
#include <ystdex/scope_guard.hpp> // for ystdex::guard;
#include <ystdex/invoke.hpp> // for ystdex::invoke;
#include <ystdex/meta.hpp> // for ystdex::remove_cvref_t;
#include "BasicReduction.h" // for LiftPrefixToReturn;

namespace Unilang
{

// NOTE: The collection of values of unit types.
enum class ValueToken
{
	Unspecified,
	Ignore
};


// NOTE: This is the main entry of the evaluation algorithm.
ReductionStatus
ReduceOnce(TermNode&, Context&);

inline ReductionStatus
ReduceOnceLifted(TermNode& term, Context& ctx, TermNode& tm)
{
	LiftOther(term, tm);
	return ReduceOnce(term, ctx);
}

ReductionStatus
ReduceOrdered(TermNode&, Context&);


struct SeparatorTransformer
{
	template<typename _func, class _tTerm, class _fPred>
	YB_ATTR_nodiscard TermNode
	operator()(_func trans, _tTerm&& term, const ValueObject& pfx,
		_fPred filter) const
	{
		const auto a(term.get_allocator());
		auto res(Unilang::AsTermNode(a, yforward(term).Value));

		if(IsBranch(term))
		{
			using it_t = decltype(std::make_move_iterator(term.begin()));

			res.Add(Unilang::AsTermNode(a, pfx));
			ystdex::split(std::make_move_iterator(term.begin()),
				std::make_move_iterator(term.end()), filter,
				[&](it_t b, it_t e){
				const auto add([&](TermNode& node, it_t i){
					node.Add(trans(Unilang::Deref(i)));
				});

				if(b != e)
				{
					if(std::next(b) == e)
						add(res, b);
					else
					{
						auto child(Unilang::AsTermNode(a));

						do
						{
							add(child, b++);
						}while(b != e);
						res.Add(std::move(child));
					}
				}
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
};


void
ParseLeaf(TermNode&, string_view);

void
ParseLeafWithSourceInformation(TermNode&, string_view,
	const shared_ptr<string>&, const SourceLocation&);


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
	: private ystdex::equality_comparable<FormContextHandler>
{
private:
	struct PTag final
	{};
	template<typename _type>
	using NotTag = ystdex::nor_<ystdex::is_instance_of<_type,
		ystdex::vseq::_a<std::tuple>>, std::is_same<_type, PTag>,
		std::is_same<_type, std::allocator_arg_t>>;
	template<typename _tParam>
	using MaybeFunc = ystdex::enable_if_t<NotTag<_tParam>::value
		&& !ystdex::is_same_param<FormContextHandler, _tParam>::value>;
	using Caller = ReductionStatus(*)(const FormContextHandler&, TermNode&,
		Context&);

public:
	ContextHandler Handler;

private:
	size_t wrapping;
	mutable Caller call_n = DoCallN;

public:
	template<typename _func, typename... _tParams,
		yimpl(typename = MaybeFunc<_func>)>
	inline
	FormContextHandler(_func&& f, _tParams&&... args)
		: FormContextHandler(
		std::forward_as_tuple(yforward(f)), yforward(args)...)
	{}
	template<typename _func, class _tAlloc, typename... _tParams>
	inline
	FormContextHandler(std::allocator_arg_t, const _tAlloc& a, _func&& f,
		_tParams&&... args)
		: FormContextHandler(std::forward_as_tuple(std::allocator_arg, a,
		yforward(f)), yforward(args)...)
	{}

private:
	template<typename... _tFuncParams>
	inline
	FormContextHandler(std::tuple<_tFuncParams...> func_args)
		: Handler(InitWrap(func_args)), wrapping(0), call_n(DoCall0)
	{}
	template<typename... _tFuncParams>
	inline
	FormContextHandler(std::tuple<_tFuncParams...> func_args,
		ystdex::size_t_<0>)
		: Handler(InitWrap(func_args)), wrapping(0), call_n(DoCall0)
	{}
	template<typename... _tFuncParams>
	inline
	FormContextHandler(std::tuple<_tFuncParams...> func_args,
		ystdex::size_t_<1>)
		: Handler(InitWrap(func_args)), wrapping(1), call_n(DoCall1)
	{}
	template<typename... _tFuncParams, size_t _vN,
		yimpl(typename = ystdex::enable_if_t<(_vN > 1)>)>
	inline
	FormContextHandler(std::tuple<_tFuncParams...> func_args,
		ystdex::size_t_<_vN>)
		: Handler(InitWrap(func_args)), wrapping(_vN), call_n(DoCallN)
	{}
	template<typename... _tFuncParams>
	inline
	FormContextHandler(std::tuple<_tFuncParams...> func_args, size_t n)
		: Handler(InitWrap(func_args)), wrapping(n), call_n(InitCall(n))
	{}

public:
	FormContextHandler(const FormContextHandler&) = default;
	FormContextHandler(FormContextHandler&&) = default;

	FormContextHandler&
	operator=(const FormContextHandler&) = default;
	FormContextHandler&
	operator=(FormContextHandler&&) = default;

	YB_ATTR_nodiscard YB_PURE friend bool
	operator==(const FormContextHandler& x, const FormContextHandler& y)
	{
		return x.Equals(y);
	}

	ReductionStatus
	operator()(TermNode& term, Context& ctx) const
	{
		CheckArguments(wrapping, term);
		return CallN(wrapping, term, ctx);
	}

	YB_ATTR_nodiscard bool
	IsDynamicWrapper() const noexcept
	{
		return call_n == DoCallN;
	}

	YB_ATTR_nodiscard YB_PURE size_t
	GetWrappingCount() const noexcept
	{
		return wrapping;
	}

	ReductionStatus
	CallHandler(TermNode&, Context&) const;

private:
	ReductionStatus
	CallN(size_t, TermNode&, Context&) const;

public:
	static void
	CheckArguments(size_t, const TermNode&);

private:
	static ReductionStatus
	DoCall0(const FormContextHandler&, TermNode&, Context&);

	static ReductionStatus
	DoCall1(const FormContextHandler&, TermNode&, Context&);

	static ReductionStatus
	DoCallN(const FormContextHandler&, TermNode&, Context&);

	YB_ATTR_nodiscard YB_PURE bool
	Equals(const FormContextHandler&) const;

	YB_ATTR_nodiscard YB_ATTR_returns_nonnull YB_PURE constexpr static
		PDefH(Caller, InitCall, size_t n) noexcept
	{
		return n == 0 ? DoCall0 : (n == 1 ? DoCall1 : DoCallN);
	}

	template<typename... _tParams>
	YB_ATTR_nodiscard static inline auto
	InitHandler(_tParams&&... args) -> decltype(Unilang::WrapContextHandler<
		ContextHandler>(yforward(args)...))
	{
		return Unilang::WrapContextHandler<ContextHandler>(yforward(args)...);
	}

	template<typename... _tFuncParams>
	YB_ATTR_nodiscard static inline auto
	InitWrap(std::tuple<_tFuncParams...>& func_args) -> decltype(ystdex::apply(
		InitHandler<_tFuncParams...>, std::move(func_args)))
	{
		return
			ystdex::apply(InitHandler<_tFuncParams...>, std::move(func_args));
	}

public:
	void
	Unwrap() noexcept
	{
		assert(wrapping != 0 && "An operative cannot be unwrapped.");
		--wrapping;
		call_n = InitCall(wrapping);
	}
};

template<typename... _tParams>
YB_ATTR_nodiscard YB_PURE inline ContextHandler
MakeForm(TermNode::allocator_type a, _tParams&&... args)
{
	return ContextHandler(std::allocator_arg, a,
		FormContextHandler(yforward(args)...));
}
template<typename... _tParams>
YB_ATTR_nodiscard YB_PURE inline ContextHandler
MakeForm(const TermNode& term, _tParams&&... args)
{
	return Unilang::MakeForm(term.get_allocator(), yforward(args)...);
}


template<class _tTarget>
YB_ATTR_always_inline inline void
RegisterFormHandler(_tTarget& target, string_view name, FormContextHandler fm)
{
	Unilang::EmplaceLeaf<ContextHandler>(target, name,
		std::allocator_arg, ToBindingsAllocator(target), std::move(fm));
}
template<class _tTarget, typename... _tParams, yimpl(typename
	= ystdex::exclude_self_params_t<FormContextHandler, _tParams...>)>
YB_ATTR_always_inline inline void
RegisterFormHandler(_tTarget& target, string_view name, _tParams&&... args)
{
	Unilang::EmplaceLeaf<ContextHandler>(target, name,
		std::allocator_arg, ToBindingsAllocator(target),
		FormContextHandler(yforward(args)...));
}


enum WrappingKind
	: decltype(std::declval<FormContextHandler>().GetWrappingCount())
{
	Form = 0,
	Strict = 1
};


template<class _tTarget, typename... _tParams>
inline void
RegisterForm(_tTarget& target, string_view name, _tParams&&... args)
{
	Unilang::RegisterFormHandler(target, name, yforward(args)..., Form);
}

template<class _tTarget, typename... _tParams>
inline void
RegisterStrict(_tTarget& target, string_view name, _tParams&&... args)
{
	Unilang::RegisterFormHandler(target, name, yforward(args)..., Strict);
}


inline void
CheckArgumentList(const TermNode& term)
{
	AssertCombiningTerm(term);
	if(!IsList(term))
		ThrowListTypeErrorForNonList(term, {}, 1);
}

inline void
CheckVariadicArity(TermNode& term, size_t n)
{
	AssertCombiningTerm(term);
	if(CountPrefix(term) - 1 <= n)
	{
		RemoveHead(term);
		ThrowInsufficientTermsError(term, {});
	}
}

YB_ATTR_nodiscard YB_PURE inline size_t
FetchArgumentN(const TermNode& term) noexcept
{
	CheckArgumentList(term);
	return term.size() - 1;
}

inline ReductionStatus
Retain(const TermNode& term) noexcept
{
	AssertCombiningTerm(term);
	return ReductionStatus::Regular;
}

inline ReductionStatus
RetainList(const TermNode& term)
{
	CheckArgumentList(term);
	return ReductionStatus::Regular;
}

inline size_t
RetainN(const TermNode& term, size_t m = 1)
{
	const auto n(FetchArgumentN(term));

	if(n == m)
		return n;
	throw ArityMismatch(m, n);
}


ReductionStatus
ReduceCombinedBranch(TermNode&, Context&);

ReductionStatus
ReduceLeaf(TermNode&, Context&);

inline ReductionStatus
ReduceReturnUnspecified(TermNode& term) noexcept
{
	term.SetValue(ValueToken::Unspecified);
	return ReductionStatus::Clean;
}


using EnvironmentGuard = ystdex::guard<EnvironmentSwitcher>;


template<typename... _tParams>
YB_ATTR_nodiscard inline EnvironmentGuard
GuardFreshEnvironment(Context& ctx, _tParams&&... args)
{
	return EnvironmentGuard(ctx, Unilang::SwitchToFreshEnvironment(ctx,
		yforward(args)...));
}

template<typename _fCallable, typename... _tParams>
auto
InvokeIn(Context& ctx, _fCallable&& f, _tParams&&... args)
	-> decltype(ystdex::invoke(yforward(f), yforward(args)...))
{
	auto r_env(ctx.WeakenRecord());
	const auto a(ToBindingsAllocator(ctx));
	auto gd(GuardFreshEnvironment(ctx));

	Unilang::AssignParent(ctx, a, in_place_type<SingleWeakParent>,
		std::move(r_env));
	return ystdex::invoke(yforward(f), yforward(args)...);
}

template<typename _fCallable, typename... _tParams>
shared_ptr<Environment>
GetModuleFor(Context& ctx, _fCallable&& f, _tParams&&... args)
{
	auto r_env(ctx.WeakenRecord());
	const auto a(ToBindingsAllocator(ctx));
	auto gd(GuardFreshEnvironment(ctx));

	Unilang::AssignParent(ctx, a, in_place_type<SingleWeakParent>,
		std::move(r_env));
	ystdex::invoke(yforward(f), yforward(args)...);
	ctx.GetRecordRef().Freeze();
	return gd.func.Switch();
}


template<class _tAlloc, typename... _tParams>
YB_ATTR_nodiscard inline shared_ptr<TermNode>
AllocateSharedTerm(const _tAlloc& a, _tParams&&... args)
{
	return Unilang::allocate_shared<TermNode>(a, yforward(args)...);
}

template<class _tAlloc, typename... _tParams>
YB_ATTR_nodiscard inline shared_ptr<TermNode>
AllocateSharedTermValue(const _tAlloc& a, _tParams&&... args)
{
	return Unilang::AllocateSharedTerm(a,
		Unilang::AsTermNode(a, yforward(args)...));
}

inline void
AssertSubobjectReferenceFirstSubterm(const TermNode& tm, const TermNode& nd)
	noexcept
{
	yunused(tm), yunused(nd);
	YAssert(IsSticky(tm.Tags),
		"Invalid 1st subterm of irregular representation of reference found.");
	YAssert(IsLeaf(tm), "Invalid irregular representation of reference with"
		" non-leaf 1st subterm found.");
	YAssert(IsTyped<shared_ptr<TermNode>>(tm), "Invalid type of value object in"
		" the 1st subterm for irregular representation found.");
	YAssert(ystdex::ref_eq<>()(Unilang::Deref(tm.Value.GetObject<shared_ptr<
		TermNode>>()), nd), "Invalid subobject reference found.");
}

inline void
AssertSubobjectReferenceTerm(const TermNode& term) noexcept
{
	yunused(term);
	YAssert(IsReferenceTerm(term), "Invalid value object type for irregular"
		" representation of reference found.");
	YAssert(term.size() == 1 || term.size() == 2, "Invalid number of subterms"
		" in irregular representation of reference found.");
	AssertSubobjectReferenceFirstSubterm(AccessFirstSubterm(term),
		Unilang::Deref(TryAccessLeaf<const TermReference>(term)).get());
}

YB_ATTR_nodiscard inline TermNode
MakeSubobjectReferent(TermNode::allocator_type a, shared_ptr<TermNode> p_sub)
{
	return Unilang::AsTermNodeTagged(a, TermTags::Sticky, std::move(p_sub));
}


YB_ATTR_nodiscard YB_PURE inline bool
IsIgnore(const TermNode& nd) noexcept
{
	return HasValue(nd, ValueToken::Ignore);
}

void
CheckParameterTree(const TermNode&);

void
BindParameter(BindingMap&, const TermNode&, TermNode&,
	const EnvironmentReference&);
void
BindParameter(const shared_ptr<Environment>&, const TermNode&, TermNode&);

void
BindParameterWellFormed(BindingMap&, const TermNode&, TermNode&,
	const EnvironmentReference&);
void
BindParameterWellFormed(const shared_ptr<Environment>&, const TermNode&,
	TermNode&);

void
BindSymbol(BindingMap&, const TokenValue&, TermNode&,
	const EnvironmentReference&);
void
BindSymbol(const shared_ptr<Environment>&, const TokenValue&, TermNode&);


YB_ATTR_nodiscard bool
AddTypeNameTableEntry(const type_info&, string_view);

template<typename _type>
inline void
InitializeTypeNameTableEntry(string_view desc)
{
	assert(desc.data());

	static struct Init final
	{
		Init(string_view sv)
		{
			const auto res(AddTypeNameTableEntry(type_id<_type>(), sv));

			yunused(res);
			assert(res && "Duplicated name found.");
		}
	} init(desc);
}

template<class _tTarget, typename _func, typename _fCallable>
YB_ATTR_nodiscard YB_PURE inline _fCallable
NameExpandedHandler(_fCallable&& x, string_view desc)
{
	static_assert(std::is_constructible<_tTarget, _fCallable>(),
		"Invalid callable type found.");
	using expanded_t
		= ystdex::expanded_caller<_func, ystdex::remove_cvref_t<_fCallable>>;

	Unilang::InitializeTypeNameTableEntry<ystdex::cond_t<ystdex::and_<
		ystdex::not_<std::is_constructible<function<_func>, _fCallable>>,
		std::is_constructible<expanded_t, _fCallable>>,
		expanded_t, ystdex::remove_cvref_t<_fCallable>>>(desc);
	return yforward(x);
}

template<typename _fCallable>
YB_ATTR_nodiscard YB_PURE inline _fCallable
NameTypedContextHandler(_fCallable&& x, string_view desc)
{
	return Unilang::NameExpandedHandler<ContextHandler,
		ContextHandler::FuncType>(yforward(x), desc);
}

template<typename _type>
YB_ATTR_nodiscard YB_PURE inline _type
NameTypedObject(_type&& x, string_view desc)
{
	InitializeTypeNameTableEntry<ystdex::remove_cvref_t<_type>>(desc);
	return yforward(x);
}

template<typename _fCallable>
YB_ATTR_nodiscard YB_PURE inline _fCallable
NameTypedReducerHandler(_fCallable&& x, string_view desc)
{
	return Unilang::NameExpandedHandler<Reducer, ReducerFunctionType>(
		yforward(x), desc);
}

YB_ATTR_nodiscard YB_PURE string_view
QueryContinuationName(const Reducer&);

YB_ATTR_nodiscard YB_PURE const SourceInformation*
QuerySourceInformation(const ValueObject&);

YB_ATTR_nodiscard YB_PURE string_view
QueryTypeName(const type_info&);

void
TraceAction(const Reducer&, YSLib::Logger&);

void
TraceBacktrace(Context::ReducerSequence::const_iterator,
	Context::ReducerSequence::const_iterator, YSLib::Logger&) noexcept;
inline void
TraceBacktrace(const Context::ReducerSequence& backtrace, YSLib::Logger& trace)
	noexcept
{
	return Unilang::TraceBacktrace(backtrace.cbegin(), backtrace.cend(), trace);
}


template<class _tGuard>
inline ReductionStatus
KeepGuard(_tGuard&, Context& ctx) noexcept
{
	return ctx.LastStatus;
}

template<class _tGuard>
using GKeptGuardAction = decltype(std::bind(KeepGuard<_tGuard>,
	std::declval<_tGuard&>(), std::placeholders::_1));

template<class _tGuard>
YB_ATTR_nodiscard inline GKeptGuardAction<_tGuard>
MoveKeptGuard(_tGuard& gd)
{
	return std::bind(KeepGuard<_tGuard>, std::move(gd), std::placeholders::_1);
}
template<class _tGuard>
YB_ATTR_nodiscard inline GKeptGuardAction<_tGuard>
MoveKeptGuard(_tGuard&& gd)
{
	return Unilang::MoveKeptGuard(gd);
}


inline namespace Internals
{

YB_ATTR_nodiscard YB_STATELESS constexpr TermTags
BindReferenceTags(TermTags ref_tags) noexcept
{
	return bool(ref_tags & TermTags::Unique) ? ref_tags | TermTags::Temporary
		: ref_tags;
}
YB_ATTR_nodiscard YB_PURE inline TermTags
BindReferenceTags(const TermReference& ref) noexcept
{
	return BindReferenceTags(GetLValueTagsOf(ref.GetTags()));
}

inline void
CopyTermTags(TermNode& term, const TermNode& tm) noexcept
{
	term.Tags = GetLValueTagsOf(tm.Tags);
}


ReductionStatus
ReduceAsSubobjectReference(TermNode&, shared_ptr<TermNode>,
	const EnvironmentReference&, TermTags);

ReductionStatus
ReduceForCombinerRef(TermNode&, const TermReference&, const ContextHandler&,
	size_t);


struct BindInsert final
{
	TermNode::Container& tcon;

	void
	operator()(const TermNode& tm) const
	{
		CopyTermTags(tcon.emplace_back(tm.GetContainer(), tm.Value), tm);
	}
	TermNode&
	operator()(TermNode::Container&& c, ValueObject&& vo) const
	{
		tcon.emplace_back(std::move(c), std::move(vo));
		return tcon.back();
	}
};


class BindParameterObject final
{
public:
	lref<const EnvironmentReference> Referenced;

private:
	char sigil;

public:
	BindParameterObject(const EnvironmentReference& r_env, char s) noexcept
		: Referenced(r_env), sigil(s)
	{}

	template<typename _fInit>
	void
	operator()(TermTags o_tags, TermNode& o, _fInit init) const
	{
		const bool temp(bool(o_tags & TermTags::Temporary));

		if(sigil != '@')
		{
			const bool can_modify(!bool(o_tags & TermTags::Nonmodifying));
			const auto a(o.get_allocator());

			if(const auto p = TryAccessLeafAtom<TermReference>(o))
			{
				if(sigil != char())
				{
					const auto ref_tags(PropagateTo(sigil == '&'
						? BindReferenceTags(*p) : p->GetTags(), o_tags));

					if(can_modify && temp)
						init(std::move(o.GetContainerRef()),
							ValueObject(in_place_type<TermReference>, ref_tags,
							std::move(*p)));
					else
						init(TermNode::Container(o.GetContainer(), a),
							ValueObject(in_place_type<TermReference>, ref_tags,
							*p));
				}
				else
				{
					auto& src(p->get());

					if(!p->IsMovable())
						init(src);
					else
						init(std::move(src.GetContainerRef()),
							std::move(src.Value));
				}
			}
			else if((can_modify || sigil == '%') && temp)
				MarkTemporaryTerm(init(std::move(o.GetContainerRef()),
					std::move(o.Value)));
			else if(sigil == '&')
				init(TermNode::Container(a), ValueObject(std::allocator_arg, a,
					in_place_type<TermReference>,
					GetLValueTagsOf(o.Tags | o_tags), o, Referenced));
			else
				init(o);
		}
		else if(!temp)
			init(TermNode::Container(o.get_allocator()),
				ValueObject(std::allocator_arg, o.get_allocator(),
				in_place_type<TermReference>, o_tags & TermTags::Nonmodifying,
				o, Referenced));
		else
			throw
				InvalidReference("Invalid operand found on binding sigil '@'.");
	}
	template<typename _fInit>
	void
	operator()(TermTags o_tags, TermNode& o, TNIter first, _fInit init) const
	{
		const bool temp(bool(o_tags & TermTags::Temporary));
		const auto bind_subpair_val_fwd(
			[&](TermNode& src, TNIter j, TermTags tags) -> TermNode&{
			assert((sigil == char() || sigil == '%') && "Invalid sigil found.");

			auto t(CreateForBindSubpairPrefix(src, j, tags));

			if(src.Value)
				BindSubpairCopySuffix(t, src, j);
			return init(std::move(t.GetContainerRef()), std::move(t.Value));
		});
		const auto bind_subpair_ref_at([&](TermTags tags){
			assert((sigil == '&' || sigil == '@') && "Invalid sigil found.");

			const auto a(o.get_allocator());
			auto t(CreateForBindSubpairPrefix(o, first, tags));

			if(o.Value)
				LiftTermRef(t.Value, o.Value);

			auto p_sub(Unilang::AllocateSharedTerm(a, std::move(t)));
			auto& sub(*p_sub);
			auto& tcon(t.GetContainerRef());

			tcon.clear();
			tcon.push_back(MakeSubobjectReferent(a, std::move(p_sub)));
			init(std::move(tcon), ValueObject(std::allocator_arg, a,
				in_place_type<TermReference>, tags, sub, Referenced));
		});

		if(sigil != '@')
		{
			const auto a(o.get_allocator());
			const bool can_modify(!bool(o_tags & TermTags::Nonmodifying));

			if(const auto p = TryAccessLeaf<TermReference>(o))
			{
				if(sigil != char())
				{
					const auto ref_tags(PropagateTo(sigil == '&'
						? BindReferenceTags(*p) : p->GetTags(), o_tags));

					if(can_modify && temp)
						init(MoveSuffix(o, first),
							ValueObject(std::allocator_arg, a, in_place_type<
							TermReference>, ref_tags, std::move(*p)));
					else
						init(TermNode::Container(first, o.end(),
							o.get_allocator()), ValueObject(std::allocator_arg,
							a, in_place_type<TermReference>, ref_tags, *p));
				}
				else
				{
					auto& src(p->get());

					if(!p->IsMovable())
						CopyTermTags(bind_subpair_val_fwd(src, src.begin(),
							GetLValueTagsOf(o_tags & ~TermTags::Unique)), src);
					else
						init(MoveSuffix(o, first), std::move(src.Value));
				}
			}
			else if((can_modify || sigil == '%')
				&& (temp || bool(o_tags & TermTags::Unique)))
			{
				if(sigil == char())
					LiftPrefixToReturn(o, first);
				MarkTemporaryTerm(init(MoveSuffix(o, first),
					std::move(o.Value)));
			}
			else if(sigil == '&')
				bind_subpair_ref_at(GetLValueTagsOf(o.Tags | o_tags));
			else
				MarkTemporaryTerm(bind_subpair_val_fwd(o, first, o_tags));
		}
		else if(!temp)
			bind_subpair_ref_at(o_tags & TermTags::Nonmodifying);
		else
			throw
				InvalidReference("Invalid operand found on binding sigil '@'.");
	}

private:
	static void
	BindSubpairCopySuffix(TermNode& t, TermNode& o, TNIter& j)
	{
		while(j != o.end())
			t.emplace(*j++);
		t.Value = ValueObject(o.Value);
	}

	void
	BindSubpairPrefix(TermNode::Container& tcon, TermNode& o, TNIter& j,
		TermTags tags) const
	{
		assert(!bool(tags & TermTags::Temporary)
			&& "Unexpected temporary tag found.");
		for(; j != o.end() && !IsSticky(j->Tags); ++j)
			BindSubpairSubterm(tcon, tags, Unilang::Deref(j));
	}

	void
	BindSubpairSubterm(TermNode::Container& tcon, TermTags o_tags, TermNode& o)
		const
	{
		assert(!bool(o_tags & TermTags::Temporary)
			&& "Unexpected temporary tag found.");

		const BindInsert init{tcon};

		if(sigil != '@')
		{
			const auto a(o.get_allocator());

			if(const auto p = TryAccessLeafAtom<TermReference>(o))
			{
				if(sigil != char())
					init(TermNode::Container(o.GetContainer(), a),
						ValueObject(in_place_type<TermReference>,
						PropagateTo(p->GetTags(), o_tags), *p));
				else
				{
					auto& src(p->get());

					if(!p->IsMovable())
						init(src);
					else
						init(std::move(src.GetContainerRef()),
							std::move(src.Value));
				}
			}
			else if(sigil == '&')
				init(TermNode::Container(a), ValueObject(std::allocator_arg, a,
					in_place_type<TermReference>,
					GetLValueTagsOf(o.Tags | o_tags), o, Referenced));
			else
				init(o);
		}
		else
			init(TermNode::Container(o.get_allocator()),
				ValueObject(std::allocator_arg, o.get_allocator(),
				in_place_type<TermReference>, o_tags & TermTags::Nonmodifying,
				o, Referenced));
	}

	YB_ATTR_nodiscard TermNode
	CreateForBindSubpairPrefix(TermNode& o, TNIter first, TermTags tags) const
	{
		assert(!bool(tags & TermTags::Temporary)
			&& "Unexpected temporary tag found.");

		const auto a(o.get_allocator());
		TermNode t(a);
		auto& tcon(t.GetContainerRef());

		BindSubpairPrefix(tcon, o, first, tags);
		if(!o.Value)
			assert(first == o.end() && "Invalid representation found.");
		return t;
	}

	void
	MarkTemporaryTerm(TermNode& term) const noexcept
	{
		if(sigil != char())
			term.Tags |= TermTags::Temporary;
	}

	YB_ATTR_nodiscard static TermNode::Container
	MoveSuffix(TermNode& o, TNIter j)
	{
		TermNode::Container tcon(o.get_allocator());

		Unilang::TransferSubtermsAfter(tcon, o, j, o.end());
		return tcon;
	}
};

} // inline namespace Internals;

} // namespace Unilang;

#endif

