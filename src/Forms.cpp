// © 2020 Uniontech Software Technology Co.,Ltd.

#include "Forms.h" // for ystdex::sfmt, ystdex::ref_eq,
//	Unilang::TryAccessReferencedTerm;
#include "Exception.h" // for InvalidSyntax, UnilangException, ListTypeError;
#include "Lexical.h" // for CategorizeBasicLexeme, LexemeCategory;
#include <exception> // for std::throw_with_nested;
#include <ystdex/utility.hpp> // ystdex::exchange, ystdex::as_const;
#include <ystdex/deref_op.hpp> // for ystdex::invoke_value_or,
//	ystdex::call_value_or;

namespace Unilang
{

namespace
{

[[nodiscard, gnu::pure]] bool
ExtractBool(const TermNode& term)
{
	if(const auto p = Unilang::TryAccessReferencedTerm<bool>(term))
		return *p;
	return true;
}


inline ReductionStatus
MoveGuard(EnvironmentGuard& gd, Context& ctx) noexcept
{
	const auto egd(std::move(gd));

	return ctx.LastStatus;
}


[[noreturn]] inline void
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


[[nodiscard, gnu::pure]] inline bool
IsIgnore(const TokenValue& s) noexcept
{
	return s == "#ignore";
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

[[nodiscard, gnu::pure]] string
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
	return RelayForEvalOrDirect(ctx, term, std::move(gd), no_lift,
		Continuation(ReduceOnce, ctx));
}


template<typename _fCopy, typename _fMove>
[[nodiscard]] auto
MakeValueOrMove(ResolvedTermReferencePtr p_ref, _fCopy cp, _fMove mv)
	-> decltype(Unilang::IsMovable(p_ref) ? mv() : cp())
{
	return Unilang::IsMovable(p_ref) ? mv() : cp();
}

[[nodiscard, gnu::pure]] inline InvalidSyntax
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


class VauHandler final : private ystdex::equality_comparable<VauHandler>
{
private:
	string eformal{};
	shared_ptr<TermNode> p_formals;
	EnvironmentReference parent;
	mutable shared_ptr<Environment> p_static;
	mutable shared_ptr<TermNode> p_eval_struct;
	ReductionStatus(VauHandler::*p_call)(TermNode&, Context&) const;

public:
	bool NoLifting = {};

	VauHandler(string&& ename, shared_ptr<TermNode>&& p_fm,
		shared_ptr<Environment>&& p_env, bool owning, TermNode& term, bool nl)
		: eformal(std::move(ename)), p_formals((CheckParameterTree(*p_fm),
		std::move(p_fm))), parent((Environment::EnsureValid(p_env), p_env)),
		p_static(owning ? std::move(p_env) : nullptr),
		p_eval_struct(ShareMoveTerm(ystdex::exchange(term,
		Unilang::AsTermNode(term.get_allocator())))), p_call(eformal.empty()
		? &VauHandler::CallStatic : &VauHandler::CallDynamic), NoLifting(nl)
	{}

	friend bool
	operator==(const VauHandler& x, const VauHandler& y)
	{
		return x.eformal == y.eformal && x.p_formals == y.p_formals
			&& x.parent == y.parent && x.p_static == y.p_static
			&& x.NoLifting == y.NoLifting;
	}

	ReductionStatus
	operator()(TermNode& term, Context& ctx) const
	{
		if(IsBranchedList(term))
		{
			if(p_eval_struct)
				return (this->*p_call)(term, ctx);
			throw UnilangException("Invalid handler of call found.");
		}
		throw UnilangException("Invalid composition found.");
	}

	static void
	CheckParameterTree(const TermNode& term)
	{
		for(const auto& child : term)
			CheckParameterTree(child);
		if(term.Value)
		{
			if(const auto p = TermToNamePtr(term))
				CheckVauSymbol(*p, "parameter in a parameter tree",
					IsIgnore(*p)
						|| CategorizeBasicLexeme(*p) == LexemeCategory::Symbol);
			else
				ThrowInvalidSymbolType(term, "parameter tree node");
		}
	}

private:
	void
	BindEnvironment(Context& ctx, ValueObject&& vo) const
	{
		assert(!eformal.empty());
		ctx.GetRecordRef().AddValue(eformal, std::move(vo));
	}

	ReductionStatus
	CallDynamic(TermNode& term, Context& ctx) const
	{
		auto wenv(ctx.WeakenRecord());
		EnvironmentGuard gd(ctx, Unilang::SwitchToFreshEnvironment(ctx));

		BindEnvironment(ctx, std::move(wenv));
		return DoCall(term, ctx, gd);
	}

	ReductionStatus
	CallStatic(TermNode& term, Context& ctx) const
	{
		EnvironmentGuard gd(ctx, Unilang::SwitchToFreshEnvironment(ctx));

		return DoCall(term, ctx, gd);
	}

	ReductionStatus
	DoCall(TermNode& term, Context& ctx, EnvironmentGuard& gd) const
	{
		assert(p_eval_struct);

		RemoveHead(term);
		BindParameter(ctx.GetRecordPtr(), *p_formals, term);
		ctx.GetRecordRef().Parent = parent;
		// TODO: Implement TCO.
		LiftOtherOrCopy(term, *p_eval_struct, {});
		return RelayForCall(ctx, term, std::move(gd), NoLifting);
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

ContextHandler
CreateVau(TermNode& term, bool no_lift, TNIter i,
	shared_ptr<Environment>&& p_env, bool owning)
{
	auto formals(ShareMoveTerm(*++i));
	auto eformal(CheckEnvFormal(*++i));

	term.erase(term.begin(), ++i);
	return FormContextHandler(VauHandler(std::move(eformal), std::move(formals),
		std::move(p_env), owning, term, no_lift));
}

[[noreturn]] ReductionStatus
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
EqualTermReference(TermNode& term, _func f)
{
	EqualTerm(term, [f](const TermNode& x, const TermNode& y){
		return IsLeaf(x) && IsLeaf(y) ? f(x.Value, y.Value)
			: ystdex::ref_eq<>()(x, y);
	}, static_cast<const TermNode&(&)(const TermNode&)>(ReferenceTerm));
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

	[[nodiscard, gnu::pure]] friend bool
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

	[[nodiscard, gnu::pure]] friend bool
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

	[[nodiscard, gnu::pure]] friend bool
	operator==(const Encapsulate& x, const Encapsulate& y) noexcept
	{
		return x.Get() == y.Get();
	}

	ReductionStatus
	operator()(TermNode& term) const
	{
		Forms::RetainN(term);

		auto& tm(*std::next(term.begin()));

		YSLib::EmplaceCallResult(term.Value, Encapsulation(GetType(),
			ystdex::invoke_value_or(&TermReference::get,
			Unilang::TryAccessReferencedLeaf<const TermReference>(tm),
			std::move(tm))));
		return ReductionStatus::Clean;
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

	[[nodiscard, gnu::pure]] friend bool
	operator==(const Encapsulated& x, const Encapsulated& y) noexcept
	{
		return x.Get() == y.Get();
	}

	ReductionStatus
	operator()(TermNode& term) const
	{
		Forms::RetainN(term);

		auto& tm(*std::next(term.begin()));

		YSLib::EmplaceCallResult(term.Value,
			ystdex::call_value_or([this](const Encapsulation& enc) noexcept{
				return Get() == enc.Get();
			}, Unilang::TryAccessReferencedTerm<Encapsulation>(tm)));
		return ReductionStatus::Clean;
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

	[[nodiscard, gnu::pure]] friend bool
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


ReductionStatus
Equal(TermNode& term)
{
	EqualTermReference(term, YSLib::HoldSame);
	return ReductionStatus::Clean;
}

ReductionStatus
EqualValue(TermNode& term)
{
	EqualTermReference(term, ystdex::equal_to<>());
	return ReductionStatus::Clean;
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
	RetainN(term, 2);

	const auto i(std::next(term.begin(), 2));

	ResolveTerm([&](TermNode& nd_y, ResolvedTermReferencePtr p_ref){
		if(IsList(nd_y))
			term.GetContainerRef().splice(term.end(),
				std::move(nd_y.GetContainerRef()));
		else
			throw ListTypeError(ystdex::sfmt(
				"Expected a list for the 2nd argument, got '%s'.",
				TermToStringWithReferenceMark(nd_y, p_ref).c_str()));
	}, *i);
	term.erase(i);
	RemoveHead(term);
	LiftSubtermsToReturn(term);
	return ReductionStatus::Retained;
}


ReductionStatus
Eval(TermNode& term, Context& ctx)
{
	RetainN(term, 2);

	const auto i(std::next(term.begin()));
	auto p_env(ResolveEnvironment(*std::next(i)).first);

	ResolveTerm([&](TermNode& nd){
		auto t(std::move(term.GetContainerRef()));

		term.GetContainerRef() = nd.GetContainer();
		term.Value = nd.Value;
	}, *i);
	return RelayForEvalOrDirect(ctx, term,
		EnvironmentGuard(ctx, ctx.SwitchEnvironment(std::move(p_env))), {},
		Continuation(ReduceOnce, ctx));
}


ReductionStatus
MakeEnvironment(TermNode& term)
{
	Retain(term);

	const auto a(term.get_allocator());

	if(term.size() > 1)
	{
		ValueObject parent;
		const auto tr([&](TNCIter iter){
			return ystdex::make_transform(iter, [&](TNCIter i){
				return ReferenceTerm(*i).Value;
			});
		});

		parent = ValueObject(EnvironmentList(tr(std::next(term.begin())),
			tr(term.end()), a));
		term.Value = Unilang::AllocateEnvironment(a, std::move(parent));
	}
	else
		term.Value = Unilang::AllocateEnvironment(a);
	return ReductionStatus::Clean;
}

ReductionStatus
GetCurrentEnvironment(TermNode& term, Context& ctx)
{
	RetainN(term, 0);
	term.Value = ValueObject(ctx.WeakenRecord());
	return ReductionStatus::Clean;
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
			BindParameter(p_e, saved, term);
			term.Value = ValueToken::Unspecified;
			return ReductionStatus::Clean;
		}, std::placeholders::_1, std::move(formals), ctx.GetRecordPtr()));
	}
	throw InvalidSyntax("Invalid syntax found in definition.");
}


ReductionStatus
VauWithEnvironment(TermNode& term, Context& ctx)
{
	return CreateFunction(term, [&]{
		auto i(term.begin());
		auto& tm(*++i);

		return ReduceSubsequent(tm, ctx, [&, i]{
			term.Value = CheckFunctionCreation([&]{
				auto p_env_pr(ResolveEnvironment(tm));

				return CreateVau(term, {}, i, std::move(p_env_pr.first),
					p_env_pr.second);
			});
			return ReductionStatus::Clean;
		});
	}, 3);
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

