// SPDX-FileCopyrightText: 2020-2022 UnionTech Software Technology Co.,Ltd.

#include "Context.h" // for Unilang::allocate_shared, lref, type_id;
#include <cassert> // for assert;
#include "TermNode.h" // for AssertValueTags, IsAtom;
#include "Exception.h" // for BadIdentifier, TypeError, UnilangException,
//	ListTypeError;
#include <exception> // for std::throw_with_nested;
#include "Evaluation.h" // for ReduceOnce;
#include <ystdex/utility.hpp> // ystdex::exchange;
#include <ystdex/scope_guard.hpp> // for ystdex::make_guard;
#include "TermAccess.h" // for Unilang::IsMovable;

namespace Unilang
{

EnvironmentReference::EnvironmentReference(const shared_ptr<Environment>& p_env)
	noexcept
	: EnvironmentReference(p_env, p_env ? p_env->GetAnchorPtr() : nullptr)
{}

namespace
{

struct AnchorData final
{
public:
	AnchorData() = default;
	AnchorData(AnchorData&&) = default;

	AnchorData&
	operator=(AnchorData&&) = default;
};


#if Unilang_CheckParentEnvironment
YB_ATTR_nodiscard YB_PURE bool
IsReserved(string_view id) noexcept
{
	YAssertNonnull(id.data());
	return ystdex::begins_with(id, "__");
}
#endif

shared_ptr<Environment>
RedirectToShared(string_view id, shared_ptr<Environment> p_env)
{
#if Unilang_CheckParentEnvironment
	if(p_env)
#else
	yunused(id);
	assert(p_env);
#endif
		return p_env;
#if Unilang_CheckParentEnvironment
	// XXX: Consider use more concrete semantic failure exception.
	throw InvalidReference(ystdex::sfmt("Invalid reference found for%s name"
		" '%s', probably due to invalid context access by a dangling"
		" reference.",
		IsReserved(id) ? " reserved" : "", id.data()));
#endif
}


using Redirector = function<const ValueObject*()>;

const ValueObject*
RedirectEnvironmentList(EnvironmentList::const_iterator first,
	EnvironmentList::const_iterator last, string_view id, Redirector& cont)
{
	if(first != last)
	{
		cont = std::bind(
			[=, &cont](EnvironmentList::const_iterator i, Redirector& c){
			cont = std::move(c);
			return RedirectEnvironmentList(i, last, id, cont);
		}, std::next(first), std::move(cont));
		return &*first;
	}
	return {};
}

} // unnamed namespace;

void
Environment::CheckParent(const ValueObject&)
{
	// TODO: Check parent type.
}

void
Environment::DefineChecked(string_view id, ValueObject&& vo)
{
	assert(id.data());
	if(!AddValue(id, std::move(vo)))
		throw BadIdentifier(id, 2);
}

Environment&
Environment::EnsureValid(const shared_ptr<Environment>& p_env)
{
	if(p_env)
		return *p_env;
	throw std::invalid_argument("Invalid environment found.");
}

AnchorPtr
Environment::InitAnchor() const
{
	return Unilang::allocate_shared<AnchorData>(Bindings.get_allocator());
}

Environment::NameResolution::first_type
Environment::LookupName(string_view id) const
{
	assert(id.data());

	const auto i(Bindings.find(id));

	return i != Bindings.cend() ? &i->second : nullptr;
}

void
Environment::ThrowForInvalidType(const type_info& tp)
{
	throw TypeError(
		ystdex::sfmt("Invalid environment type '%s' found.", tp.name()));
}


TermNode&
Context::GetNextTermRef() const
{
	if(const auto p = next_term_ptr)
		return *p;
	throw UnilangException("No next term found to evaluation.");
}

ReductionStatus
Context::ApplyTail()
{
	assert(IsAlive() && "No tail action found.");
	TailAction = std::move(current.front());
	current.pop_front();
	try
	{
		LastStatus = TailAction(*this);
	}
	catch(...)
	{
		HandleException(std::current_exception());
	}
	return LastStatus;
}

void
Context::DefaultHandleException(std::exception_ptr p)
{
	assert(bool(p));
	try
	{
		std::rethrow_exception(std::move(p));
	}
	catch(bad_any_cast& ex)
	{
		std::throw_with_nested(TypeError(ystdex::sfmt("Mismatched types ('%s',"
			" '%s') found.", ex.from(), ex.to()).c_str()));
	}
}

Environment::NameResolution
Context::Resolve(shared_ptr<Environment> p_env, string_view id) const
{
	assert(bool(p_env));

	Redirector cont;
	// NOTE: Blocked. Use ISO C++14 deduced lambda return type (cf. CWG 975)
	//	compatible to G++ attribute.
	Environment::NameResolution::first_type p_obj;

	do
	{
		p_obj = p_env->LookupName(id);
	}while([&]() -> bool{
		if(!p_obj)
		{
			lref<const ValueObject> cur(p_env->Parent);
			shared_ptr<Environment> p_redirected{};
			bool search_next;

			do
			{
				const ValueObject& parent(cur);
				const auto& tp(parent.type());

				if(IsTyped<EnvironmentReference>(tp))
				{
					p_redirected = RedirectToShared(id,
						parent.GetObject<EnvironmentReference>().Lock());
					p_env.swap(p_redirected);
				}
				else if(IsTyped<shared_ptr<Environment>>(tp))
				{
					p_redirected = RedirectToShared(id,
						parent.GetObject<shared_ptr<Environment>>());
					p_env.swap(p_redirected);
				}
				else
				{
					const ValueObject* p_next = {};

					if(IsTyped<EnvironmentList>(tp))
					{
						auto& envs(parent.GetObject<EnvironmentList>());

						p_next = RedirectEnvironmentList(envs.cbegin(),
							envs.cend(), id, cont);
					}
					while(!p_next && bool(cont))
						p_next = ystdex::exchange(cont, Redirector())();
					if(p_next)
					{
						// XXX: Cyclic parent found is not allowed.
						assert(&cur.get() != p_next);
						cur = *p_next;
						search_next = true;
						continue;
					}
				}
				search_next = {};
			}while(search_next);
			return bool(p_redirected);
		}
		return false;
	}());
	return {p_obj, std::move(p_env)};
}

ReductionStatus
Context::Rewrite(Reducer reduce)
{
	SetupCurrent(std::move(reduce));
	// NOTE: Rewrite until no actions remain.
	do
	{
		ApplyTail();
	}while(IsAlive());
	return LastStatus;
}

ReductionStatus
Context::RewriteGuarded(TermNode&, Reducer reduce)
{
	// XXX: The term is not used.
	const auto unwind(ystdex::make_guard([this]() noexcept{
		TailAction = nullptr;
		UnwindCurrent();
	}));

	return Rewrite(std::move(reduce));
}

ReductionStatus
Context::RewriteTerm(TermNode& term)
{
	AssertValueTags(term);
	next_term_ptr = &term;
	return Rewrite(Unilang::ToReducer(get_allocator(), std::ref(ReduceOnce)));
}

ReductionStatus
Context::RewriteTermGuarded(TermNode& term)
{
	AssertValueTags(term);
	next_term_ptr = &term;
	return RewriteGuarded(term,
		Unilang::ToReducer(get_allocator(), std::ref(ReduceOnce)));
}

shared_ptr<Environment>
Context::ShareRecord() const noexcept
{
	return p_record;
}

shared_ptr<Environment>
Context::SwitchEnvironment(const shared_ptr<Environment>& p_env)
{
	if(p_env)
		return SwitchEnvironmentUnchecked(p_env);
	throw std::invalid_argument("Invalid environment record pointer found.");
}

shared_ptr<Environment>
Context::SwitchEnvironmentUnchecked(const shared_ptr<Environment>& p_env)
	noexcept
{
	assert(p_env);
	return ystdex::exchange(p_record, p_env);
}

bool
Context::TrySetTailOperatorName(TermNode& term) const noexcept
{
	if(combining_term_ptr)
	{
		auto& comb(*combining_term_ptr);

		if(IsBranch(comb) && ystdex::ref_eq<>()(AccessFirstSubterm(comb), term))
		{
			OperatorName = std::move(term.Value);
			return true;
		}
	}
	return {};
}

void
Context::UnwindCurrent() noexcept
{
	// XXX: The order is significant.
	while(!current.empty())
		current.pop_front();
}

EnvironmentReference
Context::WeakenRecord() const noexcept
{
	return p_record;
}


TermNode
MoveResolved(const Context& ctx, string_view id)
{
	auto pr(ResolveName(ctx, id));

	if(const auto p = pr.first)
	{
		if(Unilang::Deref(pr.second).Frozen)
			return *p;
		return std::move(*p);
	}
	throw BadIdentifier(id);
}

TermNode
ResolveIdentifier(const Context& ctx, string_view id)
{
	auto pr(ResolveName(ctx, id));

	if(pr.first)
		return PrepareCollapse(*pr.first, pr.second);
	throw BadIdentifier(id);
}

pair<shared_ptr<Environment>, bool>
ResolveEnvironment(const ValueObject& vo)
{
	if(const auto p = vo.AccessPtr<const EnvironmentReference>())
		return {p->Lock(), {}};
	if(const auto p = vo.AccessPtr<const shared_ptr<Environment>>())
		return {*p, true};
	Environment::ThrowForInvalidType(vo.type());
}
pair<shared_ptr<Environment>, bool>
ResolveEnvironment(ValueObject& vo, bool move)
{
	if(const auto p = vo.AccessPtr<const EnvironmentReference>())
		return {p->Lock(), {}};
	if(const auto p = vo.AccessPtr<shared_ptr<Environment>>())
		return {move ? std::move(*p) : *p, true};
	Environment::ThrowForInvalidType(vo.type());
}
pair<shared_ptr<Environment>, bool>
ResolveEnvironment(const TermNode& term)
{
	return ResolveTerm([&](const TermNode& nd, bool has_ref){
		if(IsAtom(nd))
			return ResolveEnvironment(nd.Value);
		throw ListTypeError(
			ystdex::sfmt("Invalid environment formed from list '%s' found.",
			TermToStringWithReferenceMark(nd, has_ref).c_str()));
	}, term);
}
pair<shared_ptr<Environment>, bool>
ResolveEnvironment(TermNode& term)
{
	return ResolveTerm([&](TermNode& nd, ResolvedTermReferencePtr p_ref){
		if(IsAtom(nd))
			return ResolveEnvironment(nd.Value, Unilang::IsMovable(p_ref));
		throw ListTypeError(
			ystdex::sfmt("Invalid environment formed from list '%s' found.",
			TermToStringWithReferenceMark(nd, p_ref).c_str()));
	}, term);
}

} // namespace Unilang;

