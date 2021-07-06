// © 2020-2021 Uniontech Software Technology Co.,Ltd.

#include "Forms.h" // for ystdex::sfmt, Unilang::Deref, ystdex::ref_eq,
//	Unilang::TryAccessReferencedTerm;
#include <exception> // for std::throw_with_nested;
#include "Exception.h" // for InvalidSyntax, UnilangException, ListTypeError;
#include "TCO.h" // for MoveGuard, ReduceSubsequent;
#include "Lexical.h" // for CategorizeBasicLexeme, LexemeCategory;
#include "Evaluation.h" // for BindParameterWellFormed;
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


[[gnu::nonnull(2)]] void
CheckVauSymbol(const TokenValue& s, const char* target, bool is_valid)
{
	if(!is_valid)
		throw InvalidSyntax(ystdex::sfmt("Token '%s' is not a symbol or"
			" '#ignore' expected for %s.", s.data(), target));
}

[[noreturn, gnu::nonnull(2)]] void
ThrowInvalidSymbolType(const TermNode& term, const char* n)
{
	throw InvalidSyntax(ystdex::sfmt("Invalid %s type '%s' found.", n,
		term.Value.type().name()));
}

YB_ATTR_nodiscard YB_PURE string
CheckEnvFormal(const TermNode& eterm)
{
	const auto& term(ReferenceTerm(eterm));

	if(const auto p = TermToNamePtr(term))
	{
		if(!IsIgnore(*p))
		{
			CheckVauSymbol(*p, "environment formal parameter",
				CategorizeBasicLexeme(*p) == LexemeCategory::Symbol);
			return *p;
		}
	}
	else
		ThrowInvalidSymbolType(term, "environment formal parameter");
	return {};
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
	Forms::RetainN(term, 2);

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
		shared_ptr<Environment>&& p_env, bool owning, TermNode& term, bool nl)
		: eformal(std::move(ename)), p_formals((CheckParameterTree(*p_fm),
		std::move(p_fm))), parent(MakeParentSingle(p_env, owning)),
		p_eval_struct(ShareMoveTerm(ystdex::exchange(term,
		Unilang::AsTermNode(term.get_allocator())))),
		call(eformal.empty() ? CallStatic : CallDynamic), NoLifting(nl)
	{}
	VauHandler(int, string&& ename, shared_ptr<TermNode>&& p_fm,
		ValueObject&& vo, TermNode& term, bool nl)
		: eformal(std::move(ename)),
		p_formals((CheckParameterTree(Deref(p_fm)), std::move(p_fm))),
		parent((Environment::CheckParent(vo), std::move(vo))),
		p_eval_struct(ShareMoveTerm(ystdex::exchange(term,
		Unilang::AsTermNode(term.get_allocator())))),
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

		LiftOtherOrCopy(term, *p_eval_struct, {});
		return RelayForCall(ctx, term, std::move(gd), NoLifting);

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

	YB_ATTR_nodiscard YB_PURE static ValueObject
	MakeParentSingle(const shared_ptr<Environment>& p_env, bool owning)
	{
		Environment::EnsureValid(p_env);
		if(owning)
			return p_env;
		return EnvironmentReference(p_env);
	}

	static void
	SaveNothing(const VauHandler&, TCOAction&)
	{}

	static void
	SaveList(const VauHandler& vau, TCOAction& act)
	{
		for(auto& vo : vau.parent.GetObject<EnvironmentList>())
		{
			if(const auto p = vo.AccessPtr<shared_ptr<Environment>>())
				SaveOwningPtr(*p, act);
		}
	}

	static void
	SaveOwning(const VauHandler& vau, TCOAction& act)
	{
		SaveOwningPtr(vau.parent.GetObject<shared_ptr<Environment>>(), act);
	}

	static void
	SaveOwningPtr(shared_ptr<Environment>& p_static, TCOAction& act)
	{
		if(p_static.use_count() == 1)
			act.RecordList.emplace_front(ContextHandler(), std::move(p_static));
	}
};


template<typename _func>
ReductionStatus
CreateFunction(TermNode& term, _func f, size_t n)
{
	Forms::Retain(term);
	if(term.size() > n)
		return f();
	ThrowInsufficientTermsErrorFor(MakeFunctionAbstractionError(), term);
}

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

YB_ATTR_nodiscard ContextHandler
CreateVau(TermNode& term, bool no_lift, TNIter i,
	shared_ptr<Environment>&& p_env, bool owning)
{
	auto formals(ShareMoveTerm(*++i));
	auto eformal(CheckEnvFormal(*++i));

	term.erase(term.begin(), ++i);
	return FormContextHandler(VauHandler(std::move(eformal), std::move(formals),
		std::move(p_env), owning, term, no_lift));
}

YB_ATTR_nodiscard ContextHandler
CreateVauWithParent(TermNode& term, bool no_lift, TNIter i,
	ValueObject&& parent)
{
	auto formals(ShareMoveTerm(Unilang::Deref(++i)));
	auto eformal(CheckEnvFormal(Unilang::Deref(++i)));

	term.erase(term.begin(), ++i);
	return FormContextHandler(VauHandler(0, std::move(eformal),
		std::move(formals), std::move(parent), term, no_lift));
}

ReductionStatus
VauImpl(TermNode& term, Context& ctx, bool no_lift)
{
	return CreateFunction(term, [&, no_lift]{
		term.Value = CheckFunctionCreation([&]{
			return
				CreateVau(term, no_lift, term.begin(), ctx.ShareRecord(), {});
		});
		return ReductionStatus::Clean;
	}, 2);
}

ReductionStatus
VauWithEnvironmentImpl(TermNode& term, Context& ctx, bool no_lift)
{
	return CreateFunction(term, [&, no_lift]{
		auto i(term.begin());
		auto& tm(Unilang::Deref(++i));

		return ReduceSubsequent(tm, ctx, [&, i, no_lift]{
			term.Value = CheckFunctionCreation([&]{
				return ResolveTerm([&](TermNode& nd,
					ResolvedTermReferencePtr p_ref) -> ContextHandler{
					if(!IsExtendedList(nd))
					{
						auto p_env_pr(ResolveEnvironment(nd.Value,
							Unilang::IsMovable(p_ref)));

						Environment::EnsureValid(p_env_pr.first);
						return CreateVau(term, no_lift, i,
							std::move(p_env_pr.first), p_env_pr.second);
					}
					if(IsList(nd))
						return CreateVauWithParent(term, no_lift, i,
							MakeEnvironmentParent(nd.begin(),
							nd.end(), nd.get_allocator(),
							!Unilang::IsMovable(p_ref)));
					ThrowInvalidEnvironmentType(nd, p_ref);
				}, tm);
			});
			return ReductionStatus::Clean;
		});
	}, 3);
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
	Forms::RetainN(term, 2);

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
	Forms::RetainN(term, 2);
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
		Forms::RetainN(term);

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
		Forms::RetainN(term);

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
		Forms::RetainN(term);

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
						term.Value = TermReference(tm,
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

} // unnamed namespace;


namespace Forms
{

size_t
RetainN(const TermNode& term, size_t m)
{
	assert(IsBranch(term));

	const auto n(term.size() - 1);

	if(n == m)
		return n;
	throw ArityMismatch(m, n);
}


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

	const auto a(term.get_allocator());

	term.Value = term.size() > 1 ? Unilang::AllocateEnvironment(a,
		MakeEnvironmentParent(std::next(term.begin()), term.end(), a, {}))
		: Unilang::AllocateEnvironment(a);
}

void
GetCurrentEnvironment(TermNode& term, Context& ctx)
{
	RetainN(term, 0);
	term.Value = ValueObject(ctx.WeakenRecord());
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
MakeEncapsulationType(TermNode& term)
{
	const auto tag(in_place_type<ContextHandler>);
	const auto a(term.get_allocator());
	shared_ptr<void> p_type(new byte);

	term.GetContainerRef() = {Unilang::AsTermNode(a, tag, std::allocator_arg, a,
		FormContextHandler(Encapsulate(p_type), 1)),
		Unilang::AsTermNode(a, tag, std::allocator_arg, a,
		FormContextHandler(Encapsulated(p_type), 1)),
		Unilang::AsTermNode(a, tag, std::allocator_arg, a,
		FormContextHandler(Decapsulate(p_type), 1))};
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

