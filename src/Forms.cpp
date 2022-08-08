// © 2020-2022 Uniontech Software Technology Co.,Ltd.

#include "Forms.h" // for TryAccessReferencedTerm, ThrowTypeErrorForInvalidType,
//	ResolveTerm, TermToNamePtr, ResolvedTermReferencePtr, Unilang::IsMovable,
//	ystdex::sfmt, Unilang::Deref, ClearCombiningTags, AssertValueTags,
//	IsBranchedList, IsList, IsLeaf, FormContextHandler, IsAtom, ystdex::ref_eq,
//	ReferenceTerm, Forms::CallResolvedUnary, LiftTerm,
//	ThrowListTypeErrorForNonList, ThrowValueCategoryError,
//	Unilang::EmplaceCallResultOrReturn;
#include <exception> // for std::throw_with_nested;
#include "Exception.h" // for InvalidSyntax, TypeError, UnilangException,
//	ListTypeError;
#include "Evaluation.h" // for IsIgnore, RetainN, BindParameterWellFormed,
//	Unilang::MakeForm, CheckVariadicArity, Form, RetainList,
//	ReduceForCombinerRef, Strict, Unilang::NameTypedContextHandler,
//	NameTypedContextHandler;
#include "Lexical.h" // for IsUnilangSymbol;
#include "TCO.h" // for ReduceSubsequent, Action;
#include <ystdex/utility.hpp> // ystdex::exchange, ystdex::as_const;
#include "TermNode.h" // for TNCIter;
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

YB_ATTR_nodiscard YB_PURE string
CheckEnvironmentFormal(const TermNode& term)
{
	try
	{
		return ResolveTerm([&](const TermNode& nd, bool has_ref) -> string{
			if(const auto p = TermToNamePtr(nd))
				return string(*p, term.get_allocator());
			else if(!IsIgnore(nd))
				ThrowFormalParameterTypeError(nd, has_ref);
			return string(term.get_allocator());
		}, term);
	}
	catch(...)
	{
		std::throw_with_nested(InvalidSyntax("Failed checking for"
			" environment formal parameter (expected a symbol)."));
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

YB_ATTR_nodiscard YB_PURE inline InvalidSyntax
MakeFunctionAbstractionError() noexcept
{
	return InvalidSyntax("Invalid syntax found in function abstraction.");
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
	}, Unilang::Deref(i));
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


class VauHandler final : private ystdex::equality_comparable<VauHandler>
{
private:
	string eformal{};
	shared_ptr<TermNode> p_formals;
	mutable ValueObject parent;
	mutable shared_ptr<TermNode> p_eval_struct;
	ReductionStatus(&call)(const VauHandler&, TermNode&, Context&);

public:
	bool NoLifting = {};

	VauHandler(string&& ename, shared_ptr<TermNode>&& p_fm,
		ValueObject&& vo, TermNode& term, bool nl)
		: eformal(std::move(ename)), p_formals((CheckParameterTree(Deref(p_fm)),
		std::move(p_fm))), parent(std::move(vo)),
		p_eval_struct((AssertValueTags(term), ShareMoveTerm(
		ystdex::exchange(term, Unilang::AsTermNode(term.get_allocator()))))),
		call(eformal.empty() ? CallStatic : CallDynamic), NoLifting(nl)
	{}

	friend bool
	operator==(const VauHandler& x, const VauHandler& y)
	{
		return x.eformal == y.eformal && x.p_formals == y.p_formals
			&& x.parent == y.parent && x.NoLifting == y.NoLifting;
	}

	ReductionStatus
	operator()(TermNode& term, Context& ctx) const
	{
		Retain(term);
		if(p_eval_struct)
			return call(*this, term, ctx);
		throw UnilangException("Invalid handler of call found.");
	}

private:
	void
	BindEnvironment(Context& ctx, ValueObject&& vo) const
	{
		assert(!eformal.empty());
		ctx.GetRecordRef().AddValue(eformal, std::move(vo));
	}

	static ReductionStatus
	CallDynamic(const VauHandler& vau, TermNode& term, Context& ctx)
	{
		auto wenv(ctx.WeakenRecord());
		EnvironmentGuard gd(ctx, Unilang::SwitchToFreshEnvironment(ctx));

		vau.BindEnvironment(ctx, std::move(wenv));
		return vau.DoCall(term, ctx, gd);
	}

	static ReductionStatus
	CallStatic(const VauHandler& vau, TermNode& term, Context& ctx)
	{
		EnvironmentGuard gd(ctx, Unilang::SwitchToFreshEnvironment(ctx));

		return vau.DoCall(term, ctx, gd);
	}

	ReductionStatus
	DoCall(TermNode& term, Context& ctx, EnvironmentGuard& gd) const
	{
		assert(p_eval_struct);

		const bool move(p_eval_struct.use_count() == 1
			&& bool(term.Tags & TermTags::Temporary));

		RemoveHead(term);
		BindParameterWellFormed(ctx.GetRecordPtr(), Unilang::Deref(p_formals),
			term);
		ctx.GetRecordRef().Parent = parent;
		AssertNextTerm(ctx, term);

		const bool no_lift(NoLifting);

		if(move)
		{
			ctx.GetRecordRef().Parent = std::move(parent);
			term.SetContent(std::move(Unilang::Deref(p_eval_struct)));

			auto& act(RefTCOAction(ctx));

			yunused(act.MoveFunction());
		}
		else
		{
			ctx.GetRecordRef().Parent = parent;
			term.SetContent(Unilang::Deref(p_eval_struct));
		}
		return RelayForCall(ctx, term, std::move(gd), no_lift);
	}

public:
	YB_ATTR_nodiscard YB_PURE static ValueObject
	MakeParent(ValueObject&& vo)
	{
		Environment::CheckParent(vo);
		return std::move(vo);
	}

	YB_ATTR_nodiscard YB_PURE static ValueObject
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


	YB_ATTR_nodiscard YB_PURE static ValueObject
	MakeParentSingleNonOwning(TermNode::allocator_type a,
		const shared_ptr<Environment>& p_env)
	{
		Environment::EnsureValid(p_env);
		return ValueObject(std::allocator_arg, a,
			in_place_type<EnvironmentReference>, p_env);
	}
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
		std::throw_with_nested(MakeFunctionAbstractionError());
	}
}

YB_ATTR_nodiscard VauHandler
MakeVau(TermNode& term, bool no_lift, TNIter i, ValueObject&& vo)
{
	auto formals(ShareMoveTerm(Unilang::Deref(++i)));
	auto eformal(CheckEnvironmentFormal(Unilang::Deref(++i)));

	term.erase(term.begin(), ++i);
	ClearCombiningTags(term);
	return VauHandler(std::move(eformal), std::move(formals), std::move(vo),
		term, no_lift);
}

template<typename _func>
inline ReductionStatus
ReduceCreateFunction(TermNode& term, _func f, size_t wrap)
{
	term.Value = Unilang::MakeForm(term, CheckFunctionCreation(f), wrap);
	return ReductionStatus::Clean;
}

ReductionStatus
VauImpl(TermNode& term, Context& ctx, bool no_lift)
{
	CheckVariadicArity(term, 1);
	return ReduceCreateFunction(term, [&]{
		return MakeVau(term, no_lift, term.begin(),
			VauHandler::MakeParentSingleNonOwning(term.get_allocator(),
			ctx.GetRecordPtr()));
	}, Form);
}

ReductionStatus
VauWithEnvironmentImpl(TermNode& term, Context& ctx, bool no_lift)
{
	CheckVariadicArity(term, 2);

	auto i(term.begin());
	auto& tm(Unilang::Deref(++i));

	ResolveTerm([&](TermNode& nd, ResolvedTermReferencePtr p_ref){
		LiftTermOrCopy(tm, nd, Unilang::IsMovable(p_ref));
	}, tm);
	EnsureValueTags(tm.Tags);
	return ReduceSubsequent(tm, ctx, NameTypedReducerHandler([&, i, no_lift]{
		return ReduceCreateFunction(term, [&]{
			return
				ResolveTerm([&](TermNode& nd, ResolvedTermReferencePtr p_ref){
				return MakeVau(term, no_lift, i, [&]() -> ValueObject{
					if(IsList(nd))
						return VauHandler::MakeParent(MakeEnvironmentParent(
							nd.begin(), nd.end(), nd.get_allocator(),
							!Unilang::IsMovable(p_ref)));
					if(IsLeaf(nd))
						return VauHandler::MakeParentSingle(
							term.get_allocator(), ResolveEnvironment(
							nd.Value, Unilang::IsMovable(p_ref)));
					ThrowInvalidEnvironmentType(nd, p_ref);
				}());
			}, tm);
		}, Form);
	}, "eval-vau-parent"));
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
		const auto n(fch.Wrapping + 1);

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
		return ReferenceTerm(x).Value;
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
	return x.size() != y.size() || x.Value != y.Value;
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
	ConsSplice(cons_item(Unilang::Deref(++i)), term);
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
		LiftTerm(term, Unilang::Deref(std::next(term.begin())));
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

	return Unilang::ResolveTerm(
		[&](TermNode& nd, ResolvedTermReferencePtr p_ref){
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
			: WrapH(term, FormContextHandler(std::move(std::move(h)), 1));
	});
}

ReductionStatus
Unwrap(TermNode& term)
{
	return WrapUnwrap(term,
		[&](FormContextHandler& fch, ResolvedTermReferencePtr p_ref){
		if(fch.Wrapping != 0)
			return MakeValueOrMove(p_ref, [&]{
				return ReduceForCombinerRef(term, Unilang::Deref(p_ref),
					fch.Handler, fch.Wrapping - 1);
			}, [&]{
				--fch.Wrapping;
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
		Unilang::Deref(std::next(term.begin()))));
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

