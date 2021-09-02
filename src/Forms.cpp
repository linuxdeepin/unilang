// © 2020-2021 Uniontech Software Technology Co.,Ltd.

#include "Forms.h" // for ystdex::sfmt, Unilang::Deref, ystdex::ref_eq,
//	Unilang::TryAccessReferencedTerm;
#include <exception> // for std::throw_with_nested;
#include "Exception.h" // for InvalidSyntax, ThrowInvalidTokenError,
//	UnilangException, ListTypeError;
#include "TermAccess.h" // for ThrowTypeErrorForInvalidType, ResolveTerm,
//	TermToNamePtr, ThrowValueCategoryError;
#include "Evaluation.h" // for IsIgnore, RetainN, BindParameterWellFormed,
//	Unilang::MakeForm, CheckVariadicArity, Form, Strict;
#include "Lexical.h" // for IsUnilangSymbol;
#include "TCO.h" // for MoveGuard, ReduceSubsequent;
#include "Lexical.h" // for CategorizeBasicLexeme, LexemeCategory;
#include <ystdex/utility.hpp> // ystdex::exchange, ystdex::as_const;
#include <ystdex/deref_op.hpp> // for ystdex::invoke_value_or,
//	ystdex::call_value_or;

namespace Unilang
{

namespace
{

YB_ATTR_nodiscard YB_PURE bool
ExtractBool(const TermNode& term)
{
	if(const auto p = Unilang::TryAccessReferencedTerm<bool>(term))
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

[[noreturn]] inline void
ThrowFormalParameterTypeError(const TermNode& term, bool has_ref)
{
	ThrowTypeErrorForInvalidType(ystdex::type_id<TokenValue>(), term, has_ref);
}

YB_ATTR_nodiscard YB_PURE string
CheckEnvironmentFormal(const TermNode& term)
{
	try
	{
		return ResolveTerm([&](const TermNode& nd, bool has_ref) -> string{
			if(const auto p = TermToNamePtr(nd))
			{
				if(!IsIgnore(*p))
				{
					if(IsUnilangSymbol(*p))
						return string(*p, term.get_allocator());
					ThrowInvalidTokenError(*p);
				}
			}
			else
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


template<typename _fNext>
ReductionStatus
RelayForEvalOrDirect(Context& ctx, TermNode& term, EnvironmentGuard&& gd,
	bool, _fNext&& next)
{
	// TODO: Implement TCO.
	auto act(std::bind(MoveGuard, std::move(gd), std::placeholders::_1));

	Continuation cont([&]{
		return ReduceForLiftedResult(term);
	}, ctx);

	ctx.SetupFront(std::move(act));
	ctx.SetNextTermRef(term);
	ctx.SetupFront(std::move(cont));
	return next(ctx);
}

ReductionStatus
RelayForCall(Context& ctx, TermNode& term, EnvironmentGuard&& gd,
	bool no_lift)
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
			if(const auto p = Unilang::TryAccessLeaf<const TermReference>(*i))
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
		std::move(p_fm))), parent(std::move(vo)), p_eval_struct(ShareMoveTerm(
		ystdex::exchange(term, Unilang::AsTermNode(term.get_allocator())))),
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
		if(IsBranchedList(term))
		{
			if(p_eval_struct)
				return call(*this, term, ctx);
			throw UnilangException("Invalid handler of call found.");
		}
		throw UnilangException("Invalid composition found.");
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

	return ReduceSubsequent(tm, ctx, [&, i, no_lift]{
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
	});
}

YB_NORETURN ReductionStatus
ThrowForUnwrappingFailure(const ContextHandler& h)
{
	throw TypeError(ystdex::sfmt("Unwrapping failed with type '%s'.",
		h.target_type().name()));
}


template<typename _fComp, typename _func>
void
EqualTerm(TermNode& term, _fComp f, _func g)
{
	RetainN(term, 2);

	auto i(term.begin());
	const auto& x(*++i);

	term.Value = f(g(x), g(ystdex::as_const(*++i)));
}

template<typename _func>
void
EqTermReference(TermNode& term, _func f)
{
	EqualTerm(term, [f](const TermNode& x, const TermNode& y){
		return IsLeaf(x) && IsLeaf(y) ? f(x.Value, y.Value)
			: ystdex::ref_eq<>()(x, y);
	}, static_cast<const TermNode&(&)(const TermNode&)>(ReferenceTerm));
}


void
MakeValueListOrMove(TermNode& term, TermNode& nd,
	ResolvedTermReferencePtr p_ref)
{
	if(Unilang::IsMovable(p_ref))
		term.GetContainerRef().splice(term.end(),
			std::move(nd.GetContainerRef()));
	else
		for(const auto& sub : nd)
			term.Add(sub);
}


YB_NORETURN void
ThrowConsError(TermNode& nd, ResolvedTermReferencePtr p_ref)
{
	throw ListTypeError(ystdex::sfmt(
		"Expected a list for the 2nd argument, got '%s'.",
		TermToStringWithReferenceMark(nd, p_ref).c_str()));
}

void
ConsItem(TermNode& term, TermNode& y)
{
	ResolveTerm([&](TermNode& nd_y, ResolvedTermReferencePtr p_ref){
		if(IsList(nd_y))
			MakeValueListOrMove(term, nd_y, p_ref);
		else
			ThrowConsError(nd_y, p_ref);
	}, y);
}

ReductionStatus
ConsImpl(TermNode& term)
{
	RetainN(term, 2);
	RemoveHead(term);

	const auto i(std::next(term.begin()));

	ConsItem(term, Unilang::Deref(i));
	term.erase(i);
	return ReductionStatus::Retained;
}


class EncapsulationBase
{
private:
	shared_ptr<void> p_type;

public:
	EncapsulationBase(shared_ptr<void> p) noexcept
		: p_type(std::move(p))
	{}
	EncapsulationBase(const EncapsulationBase&) = default;
	EncapsulationBase(EncapsulationBase&&) = default;

	EncapsulationBase&
	operator=(const EncapsulationBase&) = default;
	EncapsulationBase&
	operator=(EncapsulationBase&&) = default;

	YB_ATTR_nodiscard YB_PURE friend bool
	operator==(const EncapsulationBase& x, const EncapsulationBase& y) noexcept
	{
		return x.p_type == y.p_type;
	}

	const EncapsulationBase&
	Get() const noexcept
	{
		return *this;
	}
	EncapsulationBase&
	GetEncapsulationBaseRef() noexcept
	{
		return *this;
	}
	const shared_ptr<void>&
	GetType() const noexcept
	{
		return p_type;
	}
};


class Encapsulation final : private EncapsulationBase
{
public:
	mutable TermNode TermRef;

	Encapsulation(shared_ptr<void> p, TermNode term)
		: EncapsulationBase(std::move(p)), TermRef(std::move(term))
	{}
	Encapsulation(const Encapsulation&) = default;
	Encapsulation(Encapsulation&&) = default;

	Encapsulation&
	operator=(const Encapsulation&) = default;
	Encapsulation&
	operator=(Encapsulation&&) = default;

	YB_ATTR_nodiscard YB_PURE friend bool
	operator==(const Encapsulation& x, const Encapsulation& y) noexcept
	{
		return x.Get() == y.Get();
	}

	using EncapsulationBase::Get;
	using EncapsulationBase::GetType;
};


class Encapsulate final : private EncapsulationBase
{
public:
	Encapsulate(shared_ptr<void> p)
		: EncapsulationBase(std::move(p))
	{}
	Encapsulate(const Encapsulate&) = default;
	Encapsulate(Encapsulate&&) = default;

	Encapsulate&
	operator=(const Encapsulate&) = default;
	Encapsulate&
	operator=(Encapsulate&&) = default;

	YB_ATTR_nodiscard YB_PURE friend bool
	operator==(const Encapsulate& x, const Encapsulate& y) noexcept
	{
		return x.Get() == y.Get();
	}

	void
	operator()(TermNode& term) const
	{
		RetainN(term);

		auto& tm(*std::next(term.begin()));

		YSLib::EmplaceCallResult(term.Value, Encapsulation(GetType(),
			ystdex::invoke_value_or(&TermReference::get,
			Unilang::TryAccessReferencedLeaf<const TermReference>(tm),
			std::move(tm))));
	}
};


class Encapsulated final : private EncapsulationBase
{
public:
	Encapsulated(shared_ptr<void> p)
		: EncapsulationBase(std::move(p))
	{}
	Encapsulated(const Encapsulated&) = default;
	Encapsulated(Encapsulated&&) = default;

	Encapsulated&
	operator=(const Encapsulated&) = default;
	Encapsulated&
	operator=(Encapsulated&&) = default;

	YB_ATTR_nodiscard YB_PURE friend bool
	operator==(const Encapsulated& x, const Encapsulated& y) noexcept
	{
		return x.Get() == y.Get();
	}

	void
	operator()(TermNode& term) const
	{
		RetainN(term);

		auto& tm(*std::next(term.begin()));

		YSLib::EmplaceCallResult(term.Value,
			ystdex::call_value_or([this](const Encapsulation& enc) noexcept{
				return Get() == enc.Get();
			}, Unilang::TryAccessReferencedTerm<Encapsulation>(tm)));
	}
};


class Decapsulate final : private EncapsulationBase
{
public:
	Decapsulate(shared_ptr<void> p)
		: EncapsulationBase(std::move(p))
	{}
	Decapsulate(const Decapsulate&) = default;
	Decapsulate(Decapsulate&&) = default;

	Decapsulate&
	operator=(const Decapsulate&) = default;
	Decapsulate&
	operator=(Decapsulate&&) = default;

	YB_ATTR_nodiscard YB_PURE friend bool
	operator==(const Decapsulate& x, const Decapsulate& y) noexcept
	{
		return x.Get() == y.Get();
	}

	ReductionStatus
	operator()(TermNode& term, Context& ctx) const
	{
		RetainN(term);

		return Unilang::ResolveTerm(
			[&](TermNode& nd, ResolvedTermReferencePtr p_ref){
			const auto&
				enc(Unilang::AccessRegular<const Encapsulation>(nd, p_ref));

			if(enc.Get() == Get())
			{
				auto& tm(enc.TermRef);

				return MakeValueOrMove(p_ref, [&]() -> ReductionStatus{
					if(const auto p
						= Unilang::TryAccessLeaf<const TermReference>(tm))
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
};


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


void
CheckResolvedListReference(TermNode& nd, bool has_ref)
{
	if(has_ref)
	{
		if(YB_UNLIKELY(!IsBranchedList(nd)))
			throw ListTypeError(ystdex::sfmt("Expected a non-empty list, got"
				" '%s'.", TermToStringWithReferenceMark(nd, true).c_str()));
	}
	else
		ThrowValueCategoryError(nd);
}

} // unnamed namespace;


namespace Forms
{

void
Eq(TermNode& term)
{
	EqTermReference(term, YSLib::HoldSame);
}

void
EqValue(TermNode& term)
{
	EqTermReference(term, ystdex::equal_to<>());
}


ReductionStatus
If(TermNode& term, Context& ctx)
{
	Retain(term);

	const auto size(term.size());

	if(size == 3 || size == 4)
	{
		auto i(std::next(term.begin()));

		return ReduceSubsequent(*i, ctx, [&, i]() -> ReductionStatus{
			auto j(i);

			if(!ExtractBool(*j))
				++j;
			return ++j != term.end() ? ReduceOnceLifted(term, ctx, *j)
				: ReduceReturnUnspecified(term);
		});
	}
	else
		throw InvalidSyntax("Syntax error in conditional form.");
}


ReductionStatus
Cons(TermNode& term)
{
	ConsImpl(term);
	LiftSubtermsToReturn(term);
	return ReductionStatus::Retained;
}

ReductionStatus
ConsRef(TermNode& term)
{
	return ConsImpl(term);
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
	Retain(term);
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

		auto formals(MoveFirstSubterm(term));

		RemoveHead(term);
		return ReduceSubsequent(term, ctx,
			std::bind([&](Context&, const TermNode& saved,
			const shared_ptr<Environment>& p_e){
			CheckBindParameter(p_e, saved, term);
			term.Value = ValueToken::Unspecified;
			return ReductionStatus::Clean;
		}, std::placeholders::_1, std::move(formals), ctx.GetRecordPtr()));
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
	RetainN(term);

	auto& tm(*std::next(term.begin()));

	ResolveTerm([&](TermNode& nd, bool has_ref){
		auto& h(nd.Value.Access<ContextHandler>());

		if(const auto p = h.target<FormContextHandler>())
		{
			auto& fch(*p);

			if(has_ref)
				term.Value = ContextHandler(FormContextHandler(fch.Handler,
					fch.Wrapping + 1));
			else
			{
				++fch.Wrapping;
				LiftOther(term, nd);
			}
		}
		else
		{
			if(has_ref)
				term.Value = ContextHandler(FormContextHandler(h, 1));
			else
				term.Value
					= ContextHandler(FormContextHandler(std::move(h), 1));
		}
	}, tm);
	return ReductionStatus::Clean;
}

ReductionStatus
Unwrap(TermNode& term)
{
	RetainN(term);

	auto& tm(*std::next(term.begin()));

	ResolveTerm([&](TermNode& nd, bool has_ref){
		auto& h(nd.Value.Access<ContextHandler>());

		if(const auto p = h.target<FormContextHandler>())
		{
			auto& fch(*p);

			if(has_ref)
				term.Value = ContextHandler(FormContextHandler(fch.Handler,
					fch.Wrapping - 1));
			else if(fch.Wrapping != 0)
			{
				--fch.Wrapping;
				LiftOther(term, nd);
			}
			else
				throw TypeError("Unwrapping failed on an operative argument.");
		}
		else
			ThrowForUnwrappingFailure(h);
	}, tm);
	return ReductionStatus::Clean;
}


ReductionStatus
CheckListReference(TermNode& term)
{
	return CallResolvedUnary([&](TermNode& nd, bool has_ref){
		CheckResolvedListReference(nd, has_ref);
		LiftTerm(term, Unilang::Deref(std::next(term.begin())));
		return ReductionStatus::Regular;
	}, term);
}


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
	Retain(term);
	RemoveHead(term);
	return ReduceOrdered(term, ctx);
}

} // namespace Forms;

} // namespace Unilang;

