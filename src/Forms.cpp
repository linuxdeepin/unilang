// SPDX-FileCopyrightText: 2020-2022 UnionTech Software Technology Co.,Ltd.

#include "Forms.h" // for TryAccessReferencedTerm, type_id, TokenValue,
//	ResolveTerm, TermToNamePtr, EnvironmentGuard, ResolvedTermReferencePtr,
//	Unilang::IsMovable, ystdex::sfmt, ClearCombiningTags, TryAccessLeaf, assert,
//	IsList, Unilang::Deref, GuardFreshEnvironment, AssertValueTags, IsLeaf,
//	FormContextHandler, ReferenceLeaf, IsAtom, ystdex::ref_eq, ReferenceTerm,
//	Forms::CallResolvedUnary, LiftTerm, Unilang::EmplaceCallResultOrReturn;
#include "Exception.h" // for InvalidSyntax, ThrowTypeErrorForInvalidType,
//	TypeError, ArityMismatch, ThrowListTypeErrorForNonList, UnilangException,
//	ThrowValueCategoryError;
#include <ystdex/optional.h> // for ystdex::optional;
#include <exception> // for std::throw_with_nested;
#include "Evaluation.h" // for IsIgnore, RetainN, BindParameterWellFormed,
//	Unilang::MakeForm, CheckVariadicArity, Form, RetainList,
//	ReduceForCombinerRef, Strict, Unilang::NameTypedContextHandler;
#include "TermNode.h" // for TNIter, IsTypedRegular, Unilang::AsTermNode,
//	CountPrefix, TNCIter;
#include <ystdex/algorithm.hpp> // for ystdex::fast_all_of;
#include <ystdex/range.hpp> // for ystdex::cbegin, ystdex::cend;
#include <ystdex/utility.hpp> // ystdex::exchange, ystdex::as_const;
#include "TCO.h" // for ReduceSubsequent, Action;
#include <ystdex/deref_op.hpp> // for ystdex::invoke_value_or,
//	ystdex::call_value_or;
#include <ystdex/functional.hpp> // for ystdex::update_thunk;
#include "TCO.h" // for OneShotChecker;

namespace Unilang
{

namespace
{

YB_ATTR_nodiscard YB_PURE bool
ExtractBool(const TermNode& term)
{
	if(const auto p = TryAccessReferencedTerm<bool>(term))
		return *p;
	return true;
}


YB_NORETURN inline void
ThrowInsufficientTermsErrorFor(InvalidSyntax&& e, const TermNode& term)
{
	try
	{
		ThrowInsufficientTermsError(term, {});
	}
	catch(...)
	{
		std::throw_with_nested(std::move(e));
	}
}

YB_NORETURN inline void
ThrowFormalParameterTypeError(const TermNode& term, bool has_ref)
{
	ThrowTypeErrorForInvalidType(type_id<TokenValue>(), term, has_ref);
}

YB_ATTR_nodiscard YB_PURE ystdex::optional<string>
ExtractEnvironmentFormal(const TermNode& term)
{
	try
	{
		return
			ResolveTerm([&](const TermNode& nd, ResolvedTermReferencePtr p_ref)
			-> ystdex::optional<string>{
			if(const auto p = TermToNamePtr(nd))
			{
				if(Unilang::IsMovable(p_ref))
					return string(std::move(*p));
				return string(*p, term.get_allocator());
			}
			else if(!IsIgnore(nd))
				ThrowFormalParameterTypeError(nd, p_ref);
			return {};
		}, term);
	}
	catch(...)
	{
		std::throw_with_nested(InvalidSyntax("Failed checking for"
			" environment formal parameter (expected a symbol or #ignore)."));
	}
}


ReductionStatus
RelayForCall(Context& ctx, TermNode& term, EnvironmentGuard&& gd, bool no_lift)
{
	return TailCall::RelayNextGuardedProbe(ctx, term, std::move(gd), !no_lift,
		std::ref(ctx.ReduceOnce));
}


template<typename _fCopy, typename _fMove>
YB_ATTR_nodiscard auto
MakeValueOrMove(ResolvedTermReferencePtr p_ref, _fCopy cp, _fMove mv)
	-> decltype(Unilang::IsMovable(p_ref) ? mv() : cp())
{
	return Unilang::IsMovable(p_ref) ? mv() : cp();
}

EnvironmentReference
FetchTailEnvironmentReference(const TermReference& ref, Context& ctx)
{
	auto r_env(ref.GetEnvironmentReference());

	return
		r_env.GetAnchorPtr() ? r_env : EnvironmentReference(ctx.GetRecordPtr());
}


YB_NORETURN void
ThrowInvalidEnvironmentType(const TermNode& term, bool has_ref)
{
	throw TypeError(ystdex::sfmt("Invalid environment formed from object '%s'"
		" found.", TermToStringWithReferenceMark(term, has_ref).c_str()));
}


ReductionStatus
EvalImpl(TermNode& term, Context& ctx, bool no_lift)
{
	RetainN(term, 2);

	const auto i(std::next(term.begin()));
	auto p_env(ResolveEnvironment(*std::next(i)).first);

	ResolveTerm([&](TermNode& nd, ResolvedTermReferencePtr p_ref){
		LiftOtherOrCopy(term, nd, Unilang::IsMovable(p_ref));
	}, *i);
	ClearCombiningTags(term);
	return TailCall::RelayNextGuardedProbe(ctx, term, EnvironmentGuard(ctx,
		ctx.SwitchEnvironment(std::move(p_env))), !no_lift,
		std::ref(ctx.ReduceOnce));
}


YB_ATTR_nodiscard ValueObject
MakeEnvironmentParent(TNIter first, TNIter last,
	const TermNode::allocator_type& a, bool nonmodifying)
{
	const auto tr([&](TNIter iter){
		return ystdex::make_transform(iter, [&](TNIter i) -> ValueObject{
			if(const auto p = TryAccessLeafAtom<const TermReference>(*i))
			{
				if(nonmodifying || !p->IsMovable())
					return p->get().Value;
				return std::move(p->get().Value);
			}
			return std::move(i->Value);
		});
	});
	ValueObject parent;

	parent.emplace<EnvironmentList>(tr(first), tr(last), a);
	return parent;
}

YB_ATTR_nodiscard shared_ptr<Environment>
CreateEnvironment(TermNode& term)
{
	const auto a(term.get_allocator());

	return !term.empty() ? Unilang::AllocateEnvironment(a,
		MakeEnvironmentParent(term.begin(), term.end(), a, {}))
		: Unilang::AllocateEnvironment(a);
}


inline void
AssignParent(ValueObject& parent, const ValueObject& vo)
{
	parent = vo;
}
inline void
AssignParent(ValueObject& parent, ValueObject&& vo)
{
	parent = std::move(vo);
}
inline void
AssignParent(ValueObject& parent, TermNode::allocator_type a,
	EnvironmentReference&& r_env)
{
	AssignParent(parent, ValueObject(std::allocator_arg, a, std::move(r_env)));
}
inline void
AssignParent(ValueObject& parent, TermNode& term, EnvironmentReference&& r_env)
{
	AssignParent(parent, term.get_allocator(), std::move(r_env));
}
template<typename... _tParams>
inline void
AssignParent(Context& ctx, _tParams&&... args)
{
	AssignParent(ctx.GetRecordRef().Parent, yforward(args)...);
}


YB_ATTR_nodiscard YB_PURE bool
IsSymbolFormal(const TermNode& term) noexcept
{
	return IsTypedRegular<TokenValue>(term);
}

YB_ATTR_nodiscard YB_PURE bool
IsNonTrailingSymbol(const TermNode& term) noexcept
{
	assert(IsSymbolFormal(term) && "Invalid term found.");
	if(const auto p = TryAccessLeaf<TokenValue>(term))
		return p->empty() || p->front() != '.';
	return {};
}

YB_ATTR_nodiscard YB_PURE bool
IsOptimizableFormalList(const TermNode& formals) noexcept
{
	assert(IsList(formals) && "Invalid term found.");
	return ystdex::fast_all_of(ystdex::cbegin(formals), ystdex::cend(formals),
		IsSymbolFormal)
		&& (formals.empty() || IsNonTrailingSymbol(*formals.rbegin()));
}

YB_ATTR_nodiscard YB_PURE ValueObject
MakeParent(ValueObject&& vo)
{
	Environment::CheckParent(vo);
	return std::move(vo);
}

YB_ATTR_nodiscard YB_PURE ValueObject
MakeParentSingle(TermNode::allocator_type a,
	pair<shared_ptr<Environment>, bool> pr)
{
	auto& p_env(pr.first);

	Environment::EnsureValid(p_env);
	if(pr.second)
		return ValueObject(std::allocator_arg, a,
			in_place_type<shared_ptr<Environment>>, std::move(p_env));
	return ValueObject(std::allocator_arg, a,
		in_place_type<EnvironmentReference>, std::move(p_env));
}


YB_ATTR_nodiscard YB_PURE ValueObject
MakeParentSingleNonOwning(TermNode::allocator_type a,
	const shared_ptr<Environment>& p_env)
{
	Environment::EnsureValid(p_env);
	return ValueObject(std::allocator_arg, a,
		in_place_type<EnvironmentReference>, p_env);
}

void
VauBind(Context& ctx, const TermNode& formals, TermNode& term)
{
	BindParameterWellFormed(ctx.GetRecordPtr(), formals, term);
}

void
VauBindList(Context& ctx, const TermNode& formals, TermNode& term)
{
	assert(IsOptimizableFormalList(formals) && "Invalid formals found.");
	AssertValueTags(term);
	ResolveTerm([&](TermNode& nd, ResolvedTermReferencePtr p_ref){
		if(IsList(nd))
		{
			const auto n_p(formals.size()), n_o(nd.size());

			if(n_p == n_o)
			{
				const auto& p_env(ctx.GetRecordPtr());
				auto i(term.begin());

				for(const auto& tm_n : formals)
				{
					BindSymbol(p_env, tm_n.Value.GetObject<TokenValue>(), *i);
					++i;
				}
			}
			else
				throw ArityMismatch(n_p, n_o);
		}
		else
			ThrowListTypeErrorForNonList(nd, p_ref);
	}, term);
}

void
VauBindOne(Context& ctx, const TermNode& formals, TermNode& term)
{
	assert(IsList(formals) && formals.size() == 1 && IsSymbolFormal(
		AccessFirstSubterm(formals)) && IsNonTrailingSymbol(
		AccessFirstSubterm(formals)) && "Invalid formals found.");
	AssertValueTags(term);
	ResolveTerm([&](TermNode& nd, ResolvedTermReferencePtr p_ref){
		if(IsList(nd))
		{
			const auto n_o(term.size());

			if(n_o == 1)
				BindSymbol(ctx.GetRecordPtr(), AccessFirstSubterm(formals).Value
					.GetObject<TokenValue>(), AccessFirstSubterm(nd));
			else
				throw ArityMismatch(1, n_o);
		}
		else
			ThrowListTypeErrorForNonList(nd, p_ref);
	}, term);
}

void
VauPrepareCall(Context& ctx, TermNode& term, ValueObject& parent,
	TermNode& eval_struct, bool move)
{
	AssertNextTerm(ctx, term);
	if(move)
	{
		AssignParent(ctx, std::move(parent));
		term.SetContent(std::move(eval_struct));
		RefTCOAction(ctx).PopTopFrame();
	}
	else
	{
		AssignParent(ctx, parent);
		term.SetContent(eval_struct);
	}
}


class VauHandler : private ystdex::equality_comparable<VauHandler>
{
protected:
	using GuardCall = EnvironmentGuard(const VauHandler&, TermNode&, Context&);
	using GuardDispatch = void(Context&, const TermNode&, TermNode&);

private:
	template<GuardDispatch& _rDispatch>
	struct GuardStatic final
	{
		static EnvironmentGuard
		Call(const VauHandler& vau, TermNode& term, Context& ctx)
		{
			auto gd(GuardFreshEnvironment(ctx));

			_rDispatch(ctx, vau.GetFormalsRef(), term);
			return gd;
		}
	};

	shared_ptr<TermNode> p_formals;
	GuardCall& guard_call;
	mutable ValueObject parent;
	mutable shared_ptr<TermNode> p_eval_struct;

public:
	bool NoLifting = {};

protected:
	VauHandler(shared_ptr<TermNode>&& p_fm, ValueObject&& vo,
		shared_ptr<TermNode>&& p_es, bool nl, GuardCall& gd_call)
		: p_formals(std::move(p_fm)), guard_call(gd_call),
		parent(std::move(vo)), p_eval_struct(std::move(p_es)), NoLifting(nl)
	{
		assert(p_eval_struct.use_count() == 1
			&& "Unexpected shared evaluation structure found.");
		AssertValueTags(Unilang::Deref(p_eval_struct)); 
	}

public:
	VauHandler(shared_ptr<TermNode>&& p_fm, ValueObject&& vo,
		shared_ptr<TermNode>&& p_es, bool nl)
		: VauHandler(std::move(p_fm), std::move(vo), std::move(p_es), nl,
		InitCall<GuardStatic>(p_fm))
	{}

	friend bool
	operator==(const VauHandler& x, const VauHandler& y)
	{
		return x.p_formals == y.p_formals && x.parent == y.parent
			&& x.NoLifting == y.NoLifting;
	}

	ReductionStatus
	operator()(TermNode& term, Context& ctx) const
	{
		Retain(term);
		if(p_eval_struct)
		{
			bool move = {};

			if(bool(term.Tags & TermTags::Temporary))
			{
				ClearCombiningTags(term);
				move = p_eval_struct.use_count() == 1;
			}
			RemoveHead(term);

			auto gd(guard_call(*this, term, ctx));
			const bool no_lift(NoLifting);

			VauPrepareCall(ctx, term, parent, *p_eval_struct, move);
			return RelayForCall(ctx, term, std::move(gd), no_lift);
		}
		throw UnilangException("Invalid handler of call found.");
	}

	YB_ATTR_nodiscard YB_PURE TermNode&
	GetFormalsRef() const noexcept
	{
		return Unilang::Deref(p_formals);
	}

protected:
	template<template<GuardDispatch&> class _func>
	YB_ATTR_nodiscard YB_PURE static GuardCall&
	InitCall(shared_ptr<TermNode>& p_fm)
	{
		auto& formals(Unilang::Deref(p_fm));

		if(IsList(formals))
		{
			if(formals.size() == 1)
			{
				auto& sub(AccessFirstSubterm(formals));

				if(IsSymbolFormal(sub) && IsNonTrailingSymbol(sub))
					return _func<VauBindOne>::Call;
			}
			if(IsOptimizableFormalList(formals))
				return _func<VauBindList>::Call;
		}
		CheckParameterTree(formals);
		return _func<VauBind>::Call;
	}
};


class DynamicVauHandler final
	: private VauHandler, private ystdex::equality_comparable<DynamicVauHandler>
{
private:
	template<GuardDispatch& _rDispatch>
	struct GuardDynamic final
	{
		static EnvironmentGuard
		Call(const VauHandler& vau, TermNode& term, Context& ctx)
		{
			auto r_env(ctx.WeakenRecord());
			auto gd(GuardFreshEnvironment(ctx));

			ctx.GetRecordRef().AddValue(static_cast<const DynamicVauHandler&>(
				vau).eformal, std::allocator_arg, ctx.get_allocator(),
				std::move(r_env));
			_rDispatch(ctx, vau.GetFormalsRef(), term);
			return gd;
		}
	};

	string eformal{};

public:
	DynamicVauHandler(shared_ptr<TermNode>&& p_fm, ValueObject&& vo,
		shared_ptr<TermNode>&& p_es, bool nl, string&& ename)
		: VauHandler(std::move(p_fm), std::move(vo), std::move(p_es), nl,
		InitCall<GuardDynamic>(p_fm)),
		eformal(std::move(ename))
	{}

	YB_ATTR_nodiscard friend bool
	operator==(const DynamicVauHandler& x, const DynamicVauHandler& y)
	{
		return static_cast<const VauHandler&>(x)
			== static_cast<const VauHandler&>(y) && x.eformal == y.eformal;
	}

	using VauHandler::operator();
};


template<typename _func>
inline auto
CheckFunctionCreation(_func f) -> decltype(f())
{
	try
	{
		return f();
	}
	catch(...)
	{
		std::throw_with_nested(
			InvalidSyntax("Invalid syntax found in function abstraction."));
	}
}

YB_ATTR_nodiscard shared_ptr<TermNode>
MakeCombinerEvalStruct(TermNode& term, TNIter i)
{
	term.erase(term.begin(), i);
	ClearCombiningTags(term);
	return ShareMoveTerm(ystdex::exchange(term,
		Unilang::AsTermNode(term.get_allocator())));
}

template<typename _func>
inline ReductionStatus
ReduceCreateFunction(TermNode& term, _func f)
{
	term.Value = CheckFunctionCreation(f);
	return ReductionStatus::Clean;
}

template<typename... _tParams>
YB_ATTR_nodiscard ReductionStatus
ReduceVau(TermNode& term, bool no_lift, TNIter i, ValueObject&& vo,
	_tParams&&... args)
{
	return ReduceCreateFunction(term, [&]() -> ContextHandler{
		auto formals(ShareMoveTerm(Unilang::Deref(++i)));
		auto eformal(ExtractEnvironmentFormal(Unilang::Deref(++i)));
		auto p_es(MakeCombinerEvalStruct(term, ++i));

		if(eformal)
			return Unilang::MakeForm(term, DynamicVauHandler(
				std::move(formals), std::move(vo), std::move(p_es), no_lift,
				std::move(*eformal)), yforward(args)...);
		return Unilang::MakeForm(term, VauHandler(std::move(formals),
			std::move(vo), std::move(p_es), no_lift), yforward(args)...);
	});
}

template<size_t _vWrapping>
ReductionStatus
LambdaVau(TermNode& term, Context& ctx, bool no_lift)
{
	CheckVariadicArity(term, 1);
	return ReduceVau(term, no_lift, term.begin(),
		MakeParentSingleNonOwning(term.get_allocator(), ctx.GetRecordPtr()),
		ystdex::size_t_<_vWrapping>());
}

template<size_t _vWrapping>
ReductionStatus
LambdaVauWithEnvironment(TermNode& term, Context& ctx, bool no_lift)
{
	CheckVariadicArity(term, 2);

	auto i(term.begin());
	auto& tm(*++i);

	ResolveTerm([&](TermNode& nd, ResolvedTermReferencePtr p_ref){
		LiftTermOrCopy(tm, nd, Unilang::IsMovable(p_ref));
	}, tm);
	EnsureValueTags(tm.Tags);
	return ReduceSubsequent(tm, ctx, NameTypedReducerHandler([&, i, no_lift]{
		return ReduceVau(term, no_lift, i, ResolveTerm([](TermNode& nd,
			ResolvedTermReferencePtr p_ref) -> ValueObject{
			if(IsList(nd))
				return MakeParent(MakeEnvironmentParent(
					nd.begin(), nd.end(), nd.get_allocator(),
					!Unilang::IsMovable(p_ref)));
			if(IsLeaf(nd))
				return MakeParentSingle(nd.get_allocator(),
					ResolveEnvironment(nd.Value, Unilang::IsMovable(p_ref)));
			ThrowInvalidEnvironmentType(nd, p_ref);
		}, tm), ystdex::size_t_<_vWrapping>());
	}, "eval-vau-parent"));
}

ReductionStatus
VauImpl(TermNode& term, Context& ctx, bool no_lift)
{
	return LambdaVau<Form>(term, ctx, no_lift);
}

ReductionStatus
VauWithEnvironmentImpl(TermNode& term, Context& ctx, bool no_lift)
{
	return LambdaVauWithEnvironment<Form>(term, ctx, no_lift);
}

YB_NORETURN ReductionStatus
ThrowForUnwrappingFailure(const ContextHandler& h)
{
	throw TypeError(ystdex::sfmt("Unwrapping failed with type '%s'.",
		h.target_type().name()));
}

ReductionStatus
WrapH(TermNode& term, FormContextHandler h)
{
	term.Value = ContextHandler(std::allocator_arg, term.get_allocator(),
		std::move(h));
	return ReductionStatus::Clean;
}
ReductionStatus
WrapH(TermNode& term, ContextHandler h, size_t n)
{
	term.Value = Unilang::MakeForm(term, std::move(h), n);
	return ReductionStatus::Clean;
}

ReductionStatus
WrapO(TermNode& term, ContextHandler& h, ResolvedTermReferencePtr p_ref)
{
	return WrapH(term, MakeValueOrMove(p_ref, [&]{
		return h;
	}, [&]{
		return std::move(h);
	}), 1);
}

ReductionStatus
WrapN(TermNode& term, ResolvedTermReferencePtr p_ref,
	FormContextHandler& fch, size_t n)
{
	return WrapH(term, MakeValueOrMove(p_ref, [&]{
		return FormContextHandler(fch.Handler, n);
	}, [&]{
		return
			FormContextHandler(std::move(fch.Handler), n);
	}));
}

ReductionStatus
WrapRefN(TermNode& term, ResolvedTermReferencePtr p_ref,
	FormContextHandler& fch, size_t n)
{
	if(p_ref)
		return ReduceForCombinerRef(term, *p_ref, fch.Handler, n);
	return WrapH(term, FormContextHandler(std::move(fch.Handler), n));
}

template<typename _func, typename _func2>
ReductionStatus
WrapUnwrap(TermNode& term, _func f, _func2 f2)
{
	return Forms::CallRegularUnaryAs<ContextHandler>([&](ContextHandler& h,
		ResolvedTermReferencePtr p_ref) -> ReductionStatus{
		if(const auto p = h.target<FormContextHandler>())
			return f(*p, p_ref);
		return ystdex::expand_proxy<ReductionStatus(ContextHandler&,
			const ResolvedTermReferencePtr&)>::call(f2, h, p_ref);
	}, term);
}

template<ReductionStatus(&_rWrap)(TermNode&,
	ResolvedTermReferencePtr, FormContextHandler&, size_t), typename _func>
ReductionStatus
WrapOrRef(TermNode& term, _func f)
{
	return WrapUnwrap(term,
		[&](FormContextHandler& fch, ResolvedTermReferencePtr p_ref){
		const auto n(fch.GetWrappingCount() + 1);

		if(n != 0)
			return _rWrap(term, p_ref, fch, n);
		throw UnilangException("Wrapping count overflow detected.");
	}, f);
}


template<typename _fComp, typename _func>
auto
EqTerm(TermNode& term, _fComp f, _func g) -> decltype(f(
	std::declval<_func&>()(std::declval<const TermNode&>()),
	std::declval<_func&>()(std::declval<const TermNode&>())))
{
	RetainN(term, 2);

	auto i(term.begin());
	const auto& x(*++i);

	return f(g(x), g(ystdex::as_const(*++i)));
}

template<typename _fComp, typename _func>
void
EqTermRet(TermNode& term, _fComp f, _func g)
{
	using type = decltype(g(term));

	EqTerm(term, [&, f](const type& x, const type& y){
		term.Value = f(x, y);
	}, g);
}

template<typename _func>
void
EqTermValue(TermNode& term, _func f)
{
	EqTermRet(term, f, [](const TermNode& x) -> const ValueObject&{
		return ReferenceLeaf(x).Value;
	});
}

template<typename _func>
void
EqTermReference(TermNode& term, _func f)
{
	EqTermRet(term, [f](const TermNode& x, const TermNode& y){
		return IsAtom(x) && IsAtom(y) ? f(x.Value, y.Value)
			: ystdex::ref_eq<>()(x, y);
	}, static_cast<const TermNode&(&)(const TermNode&)>(ReferenceTerm));
}

YB_ATTR_nodiscard YB_PURE inline bool
TermUnequal(const TermNode& x, const TermNode& y)
{
	return CountPrefix(x) != CountPrefix(y) || x.Value != y.Value;
}

void
EqualSubterm(bool& r, Action& act, TermNode::allocator_type a, TNCIter first1,
	TNCIter first2, TNCIter last1)
{
	if(first1 != last1)
	{
		auto& x(ReferenceTerm(*first1));
		auto& y(ReferenceTerm(*first2));

		if(TermUnequal(x, y))
			r = {};
		else
		{
			yunseq(++first1, ++first2);
			ystdex::update_thunk(act, a, [&, a, first1, first2, last1]{
				if(r)
					EqualSubterm(r, act, a, first1, first2, last1);
			});
			ystdex::update_thunk(act, a, [&, a]{
				if(r)
					EqualSubterm(r, act, a, x.begin(), y.begin(), x.end());
			});
		}
	}
}


template<typename... _tParams>
void
ConsSplice(TermNode t, TermNode& nd, _tParams&&... args)
{
	auto& tcon(t.GetContainerRef());
	auto& con(nd.GetContainerRef());

	assert(!ystdex::ref_eq<>()(con, tcon) && "Invalid self move found.");
	tcon.splice(tcon.begin(), con, con.begin(), yforward(args)...),
	tcon.swap(con);
	nd.Value = std::move(t.Value);
}

YB_ATTR_nodiscard TermNode
ConsItem(TermNode& y)
{
	return ResolveTerm([&](TermNode& nd_y, ResolvedTermReferencePtr p_ref_y){
		return MakeValueOrMove(p_ref_y, [&]{
			return nd_y;
		}, [&]{
			return std::move(nd_y);
		});
	}, y);
}

YB_ATTR_nodiscard inline TermNode
ConsItemRef(TermNode& y)
{
	return std::move(y);
}

void
ConsImpl(TermNode& term, TermNode(&cons_item)(TermNode& y))
{
	RetainN(term, 2);
	RemoveHead(term);

	auto i(term.begin());

	if(cons_item == ConsItem)
		LiftToReturn(*i);
	ConsSplice(cons_item(*++i), term);
}


template<typename... _tParams>
YB_ATTR_nodiscard YB_PURE inline TermNode
AsForm(TermNode::allocator_type a, _tParams&&... args)
{
	return Unilang::AsTermNode(a, std::allocator_arg, a,
		in_place_type<ContextHandler>, std::allocator_arg, a,
		FormContextHandler(yforward(args)...));
}


void
CheckFrozenEnvironment(const shared_ptr<Environment>& p_env)
{
	if(YB_UNLIKELY(Unilang::Deref(p_env).Frozen))
		throw TypeError("Cannot define variables in a frozen environment.");
}

void
CheckBindParameter(const shared_ptr<Environment>& p_env, const TermNode& t,
	TermNode& o)
{
	CheckFrozenEnvironment(p_env);
	BindParameter(p_env, t, o);
}


ReductionStatus
CheckReference(TermNode& term, void(&f)(TermNode&, bool))
{
	return Forms::CallResolvedUnary([&](TermNode& nd, bool has_ref){
		f(nd, has_ref);
		LiftTerm(term, *std::next(term.begin()));
		return ReductionStatus::Regular;
	}, term);
}

void
CheckResolvedListReference(TermNode& nd, bool has_ref)
{
	if(has_ref)
	{
		if(YB_UNLIKELY(!IsList(nd)))
			ThrowListTypeErrorForNonList(nd, true);
	}
	else
		ThrowValueCategoryError(nd);
}

void
CheckResolvedPairReference(TermNode& nd, bool has_ref)
{
	if(has_ref)
	{
		if(YB_UNLIKELY(IsAtom(nd)))
			ThrowListTypeErrorForAtom(nd, true);
	}
	else
		ThrowValueCategoryError(nd);
}

} // unnamed namespace;

bool
Encapsulation::Equal(const TermNode& x, const TermNode& y)
{
	if(TermUnequal(x, y))
		return {};

	bool r(true);
	Action act{};

	EqualSubterm(r, act, x.get_allocator(), x.begin(), y.begin(), x.end());
	while(act)
	{
		const auto a(std::move(act));

		a();
	}
	return r;
}


ReductionStatus
Encapsulate::operator()(TermNode& term) const
{
	RetainN(term);

	auto& tm(*std::next(term.begin()));

	return Unilang::EmplaceCallResultOrReturn(term,
		Encapsulation(GetType(), ystdex::invoke_value_or(&TermReference::get,
		TryAccessReferencedLeaf<const TermReference>(tm), std::move(tm))));
}


ReductionStatus
Encapsulated::operator()(TermNode& term) const
{
	RetainN(term);

	auto& tm(*std::next(term.begin()));

	return Unilang::EmplaceCallResultOrReturn(term,
		ystdex::call_value_or([this](const Encapsulation& enc) noexcept{
		return Get() == enc.Get();
	}, TryAccessReferencedTerm<Encapsulation>(tm)));
}


ReductionStatus
Decapsulate::operator()(TermNode& term, Context& ctx) const
{
	RetainN(term);
	return
		Unilang::ResolveTerm([&](TermNode& nd, ResolvedTermReferencePtr p_ref){
		const auto& enc(AccessRegular<const Encapsulation>(nd, p_ref));

		if(enc.Get() == Get())
		{
			auto& tm(enc.TermRef);

			return MakeValueOrMove(p_ref, [&]() -> ReductionStatus{
				if(const auto p = TryAccessLeafAtom<const TermReference>(tm))
				{
					term.GetContainerRef() = tm.GetContainer();
					term.Value = *p;
				}
				else
				{
					term.SetValue(in_place_type<TermReference>, tm,
						FetchTailEnvironmentReference(*p_ref, ctx));
					return ReductionStatus::Clean;
				}
				return ReductionStatus::Retained;
			}, [&]{
				LiftTerm(term, tm);		
				return ReductionStatus::Retained;
			});
		}
		throw TypeError("Mismatched encapsulation type found.");
	}, *std::next(term.begin()));
}

namespace Forms
{

void
Eq(TermNode& term)
{
	EqTermReference(term, YSLib::HoldSame);
}

void
EqLeaf(TermNode& term)
{
	EqTermValue(term, ystdex::equal_to<>());
}

void
EqValue(TermNode& term)
{
	EqTermReference(term, ystdex::equal_to<>());
}


ReductionStatus
If(TermNode& term, Context& ctx)
{
	RetainList(term);

	const auto size(term.size());

	if(size == 3 || size == 4)
	{
		auto i(std::next(term.begin()));

		return ReduceSubsequent(*i, ctx, NameTypedReducerHandler([&, i]{
			auto j(i);

			if(!ExtractBool(*j))
				++j;
			return ++j != term.end() ? ReduceOnceLifted(term, ctx, *j)
				: ReduceReturnUnspecified(term);
		}, "select-clause"));
	}
	else
		throw InvalidSyntax("Syntax error in conditional form.");
}


ReductionStatus
Cons(TermNode& term)
{
	ConsImpl(term, ConsItem);
	return ReductionStatus::Retained;
}

ReductionStatus
ConsRef(TermNode& term)
{
	ConsImpl(term, ConsItemRef);
	return ReductionStatus::Retained;
}


ReductionStatus
Eval(TermNode& term, Context& ctx)
{
	return EvalImpl(term, ctx, {});
}

ReductionStatus
EvalRef(TermNode& term, Context& ctx)
{
	return EvalImpl(term, ctx, true);
}


void
MakeEnvironment(TermNode& term)
{
	RetainList(term);
	RemoveHead(term);
	term.SetValue(CreateEnvironment(term));
}

void
GetCurrentEnvironment(TermNode& term, Context& ctx)
{
	RetainN(term, 0);
	term.SetValue(ctx.WeakenRecord());
}


ReductionStatus
Define(TermNode& term, Context& ctx)
{
	Retain(term);
	if(term.size() > 2)
	{
		RemoveHead(term);
		ClearCombiningTags(term);

		auto formals(MoveFirstSubterm(term));

		RemoveHead(term);
		return ReduceSubsequent(term, ctx,
			// XXX: This is certainly unsafe for the type initializing the
			//	handler in 'any' due to the 'TermNode' is not trivially
			//	swappable (since the 'list' data member is not trivially
			//	swappable) if the target object is stored in-place. However, it
			//	is known that in the supported platform configurations, the ABI
			//	makes it not in-place storable in
			//	'ystdex::any_ops::any_storage', so there is no need to change.
			Unilang::NameTypedReducerHandler(std::bind([&](Context&,
			const TermNode& saved, const shared_ptr<Environment>& p_e){
			CheckBindParameter(p_e, saved, term);
			term.Value = ValueToken::Unspecified;
			return ReductionStatus::Clean;
		}, std::placeholders::_1, std::move(formals), ctx.GetRecordPtr()),
			"match-ptree"));
	}
	throw InvalidSyntax("Invalid syntax found in definition.");
}


ReductionStatus
Vau(TermNode& term, Context& ctx)
{
	return VauImpl(term, ctx, {});
}

ReductionStatus
VauRef(TermNode& term, Context& ctx)
{
	return VauImpl(term, ctx, true);
}

ReductionStatus
VauWithEnvironment(TermNode& term, Context& ctx)
{
	return VauWithEnvironmentImpl(term, ctx, {});
}

ReductionStatus
VauWithEnvironmentRef(TermNode& term, Context& ctx)
{
	return VauWithEnvironmentImpl(term, ctx, true);
}


ReductionStatus
Wrap(TermNode& term)
{
	return WrapOrRef<WrapN>(term,
		[&](ContextHandler& h, ResolvedTermReferencePtr p_ref){
		return WrapO(term, h, p_ref);
	});
}

ReductionStatus
WrapRef(TermNode& term)
{
	return WrapOrRef<WrapRefN>(term,
		[&](ContextHandler& h, ResolvedTermReferencePtr p_ref){
		return p_ref ? ReduceForCombinerRef(term, *p_ref, h, 1)
			: WrapH(term, FormContextHandler(std::move(std::move(h)), Strict));
	});
}

ReductionStatus
Unwrap(TermNode& term)
{
	return WrapUnwrap(term,
		[&](FormContextHandler& fch, ResolvedTermReferencePtr p_ref){
		if(fch.GetWrappingCount() != 0)
			return MakeValueOrMove(p_ref, [&]{
				return ReduceForCombinerRef(term, Unilang::Deref(p_ref),
					fch.Handler, fch.GetWrappingCount() - 1);
			}, [&]{
				fch.Unwrap();
				return WrapH(term, std::move(fch));
			});
		throw TypeError("Unwrapping failed on an operative argument.");
	}, ThrowForUnwrappingFailure);
}


ReductionStatus
CheckListReference(TermNode& term)
{
	return CheckReference(term, CheckResolvedListReference);
}

ReductionStatus
CheckPairReference(TermNode& term)
{
	return CheckReference(term, CheckResolvedPairReference);
}


#if YB_IMPL_GNUCPP >= 120000
YB_ATTR(optimize("no-tree-pta"))
#endif
ReductionStatus
MakeEncapsulationType(TermNode& term)
{
	const auto a(term.get_allocator());
	{
		TermNode::Container tcon(a);
		shared_ptr<void> p_type(new yimpl(byte));

		tcon.push_back(Unilang::AsForm(a, Encapsulate(p_type), Strict));
		tcon.push_back(Unilang::AsForm(a, Encapsulated(p_type), Strict));
		tcon.push_back(Unilang::AsForm(a, Decapsulate(p_type), Strict));
		tcon.swap(term.GetContainerRef());
	}
	return ReductionStatus::Retained;
}


ReductionStatus
Sequence(TermNode& term, Context& ctx)
{
	RetainList(term);
	RemoveHead(term);
	return ReduceOrdered(term, ctx);
}


ReductionStatus
Call1CC(TermNode& term, Context& ctx)
{
	RetainN(term);
	yunused(Unilang::ResolveRegular<const ContextHandler>(
		*std::next(term.begin())));
	RemoveHead(term);

	auto& current(ctx.GetCurrentRef());
	const auto i_captured(current.begin());

	term.GetContainerRef().push_back(Unilang::AsTermNode(term.get_allocator(),
		Continuation(Unilang::NameTypedContextHandler(
		ystdex::bind1([&, i_captured](TermNode& t, OneShotChecker& osc){
		Retain(t);
		osc.Check();

		auto& cur(current);
		const auto i_top(i_captured);
		auto con(std::move(t.GetContainerRef()));
		auto vo(std::move(t.Value));

#ifndef NDEBUG
		{
			auto i(cur.begin());

			while(i != cur.end() && i != i_captured)
				++i;
			assert(i != cur.end() && "The top frame of the reinstated captured"
				" continuation is missing.");
		}
#endif
		while(cur.begin() != i_top)
			cur.pop_front();
		RefTCOAction(ctx).ReleaseOneShotGuard();
		yunseq(term.GetContainerRef() = std::move(con),
			term.Value = std::move(vo));
		ctx.SetNextTermRef(term);
		ClearCombiningTags(term);
		RemoveHead(term);
		return ReductionStatus::Retained;
	},
		RefTCOAction(ctx).MakeOneShotChecker()
	), "captured-one-shot-continuation"), ctx)));
	return ReduceCombinedBranch(term, ctx);
}

ReductionStatus
ContinuationToApplicative(TermNode& term)
{
	return Forms::CallRegularUnaryAs<Continuation>(
		[&](Continuation& cont, ResolvedTermReferencePtr p_ref){
		return WrapO(term, cont.Handler, p_ref);
	}, term);
}

} // namespace Forms;

} // namespace Unilang;

