// SPDX-FileCopyrightText: 2020-2022 UnionTech Software Technology Co.,Ltd.

#include "Context.h" // for string_view, Unilang::allocate_shared,
//	make_observer, lref, ystdex::retry_on_cond, YSLib::make_string_view;
#include <cassert> // for assert;
#include "Exception.h" // for TypeError, BadIdentifier, UnilangException,
//	ListTypeError;
#include "TermNode.h" // for Unilang::AddValueTo, AssertValueTags, IsAtom;
#include <exception> // for std::throw_with_nested;
#include <ystdex/utility.hpp> // ystdex::exchange;
#include "Evaluation.h" // for ReduceOnce;
#include <ystdex/scope_guard.hpp> // for ystdex::make_guard;
#include "TermAccess.h" // for Unilang::IsMovable;
#include "Forms.h" // for Forms::Sequence, ReduceBranchToList;
#include "Evaluation.h" // for Strict;
#include <ystdex/functor.hpp> // for ystdex::id;
#include <algorithm> // for std::find_if;
#include "Syntax.h" // for ReduceSyntax;

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

BindingMap&
Environment::GetMapCheckedRef()
{
	if(!IsFrozen())
		return GetMapRef();
	throw TypeError("Frozen environment found.");
}

void
Environment::CheckParent(const ValueObject& vo)
{
	const auto& ti(vo.type());

	if(IsTyped<EnvironmentList>(ti))
	{
		for(const auto& env : vo.GetObject<EnvironmentList>())
			CheckParent(env);
	}
	else if(YB_UNLIKELY(!IsTyped<observer_ptr<const Environment>>(ti)
		&& !IsTyped<EnvironmentReference>(ti)
		&& !IsTyped<shared_ptr<Environment>>(ti)))
		ThrowForInvalidType(ti);
#if Unilang_CheckParentEnvironment
	if(IsTyped<observer_ptr<const Environment>>(ti))
	{
		if(YB_UNLIKELY(!vo.GetObject<observer_ptr<const Environment>>()))
			throw std::invalid_argument("Invalid environment found.");
	}
	else if(IsTyped<EnvironmentReference>(ti))
		EnsureValid(vo.GetObject<EnvironmentReference>().Lock());
	else if(IsTyped<shared_ptr<Environment>>(ti))
		EnsureValid(vo.GetObject<shared_ptr<Environment>>());
#endif
}

void
Environment::DefineChecked(BindingMap& m, string_view id, ValueObject&& vo)
{
	assert(id.data());
	if(!Unilang::AddValueTo(m, id, std::move(vo)))
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
Environment::InitAnchor(allocator_type a) const
{
	return Unilang::allocate_shared<AnchorData>(a);
}

NameResolution::first_type
Environment::LookupName(string_view id) const
{
	assert(id.data());

	const auto i(bindings.find(id));

	return make_observer(i != bindings.cend() ? &i->second : nullptr);
}

void
Environment::ThrowForInvalidType(const type_info& tp)
{
	throw TypeError(
		ystdex::sfmt("Invalid environment type '%s' found.", tp.name()));
}


Context::Context(const GlobalState& g)
	: memory_rsrc(*g.Allocator.resource()), Global(g)
{}

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

NameResolution
Context::Resolve(shared_ptr<Environment> p_env, string_view id) const
{
	assert(bool(p_env));

	Redirector cont;
	// NOTE: Blocked. Use ISO C++14 deduced lambda return type (cf. CWG 975)
	//	compatible to G++ attribute.
	NameResolution::first_type p_obj;

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
Context::RewriteGuarded(TermNode&, Reducer reduce)
{
	const auto i(GetCurrent().cbegin());
	const auto unwind(ystdex::make_guard([i, this]() noexcept{
		TailAction = nullptr;
		UnwindCurrentUntil(i);
	}));

	return RewriteUntil(std::move(reduce), i);
}

ReductionStatus
Context::RewriteLoop()
{
	// NOTE: Rewrite until no actions remain.
	do
	{
		ApplyTail();
	}while(IsAlive());
	return LastStatus;
}

ReductionStatus
Context::RewriteLoopUntil(ReducerSequence::const_iterator i)
{
	assert(IsAliveBefore(i) && "No action to reduce.");
	// NOTE: Rewrite until no actions before the parameter pointed to remain.
	return ystdex::retry_on_cond(
		std::bind(&Context::IsAliveBefore, this, i), [&]{
		return ApplyTail();
	});
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
		if(Unilang::Deref(pr.second).IsFrozen())
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


struct SeparatorPass::TransformationSpec final
{
	enum SeparatorKind
	{
		NAry,
		BinaryAssocLeft,
		BinaryAssocRight
	};

	function<bool(const TermNode&)> Filter;
	function<ValueObject(const ValueObject&)> MakePrefix;
	SeparatorKind Kind;

	TransformationSpec(decltype(Filter), decltype(MakePrefix),
		SeparatorKind = NAry);
	TransformationSpec(TokenValue, ValueObject, SeparatorKind = NAry);
};

SeparatorPass::TransformationSpec::TransformationSpec(
	decltype(Filter) filter, decltype(MakePrefix) make_pfx, SeparatorKind kind)
	: Filter(std::move(filter)), MakePrefix(std::move(make_pfx)), Kind(kind)
{}
SeparatorPass::TransformationSpec::TransformationSpec(TokenValue delim,
	ValueObject pfx, SeparatorKind kind)
	: TransformationSpec(ystdex::bind1(HasValue<TokenValue>, delim),
		[=](const ValueObject&){
		return pfx;
	}, kind)
{}

SeparatorPass::SeparatorPass(TermNode::allocator_type a)
	: allocator(a), transformations({{";", ContextHandler(Forms::Sequence)},
	{",", ContextHandler(FormContextHandler(ReduceBranchToList, Strict))},
	{[](const TermNode& nd) noexcept{
		return HasValue<TokenValue>(nd, ":=");
	}, [](const ValueObject&){
		return TokenValue("assign!");
	}, TransformationSpec::BinaryAssocRight},
	{[](const TermNode& nd) noexcept{
		return HasValue<TokenValue>(nd, "=") || HasValue<TokenValue>(nd, "!=");
	}, ystdex::id<>(), TransformationSpec::BinaryAssocLeft},
	{[](const TermNode& nd) noexcept{
		return HasValue<TokenValue>(nd, "<") || HasValue<TokenValue>(nd, ">")
			|| HasValue<TokenValue>(nd, "<=") || HasValue<TokenValue>(nd, ">=");
	}, ystdex::id<>(), TransformationSpec::BinaryAssocLeft},
	{[](const TermNode& nd) noexcept{
		return HasValue<TokenValue>(nd, "+") || HasValue<TokenValue>(nd, "-"); 
	}, ystdex::id<>(), TransformationSpec::BinaryAssocLeft},
	{[](const TermNode& nd) noexcept{
		return HasValue<TokenValue>(nd, "*") || HasValue<TokenValue>(nd, "/"); 
	}, ystdex::id<>(), TransformationSpec::BinaryAssocLeft}}, a)
{}
SeparatorPass::~SeparatorPass() = default;

ReductionStatus
SeparatorPass::operator()(TermNode& term) const
{
	assert(remained.empty() && "Invalid state found.");
	Transform(term, {}, remained);
	while(!remained.empty())
	{
		const auto entry(std::move(remained.top()));

		remained.pop();
		for(auto& tm : entry.first.get())
			Transform(tm, entry.second, remained);
	}
	return ReductionStatus::Clean;
}

void
SeparatorPass::Transform(TermNode& term, bool skip_binary,
	SeparatorPass::TermStack& terms) const
{
	if(IsBranch(term))
	{
		if(IsEmpty(*term.begin()))
			skip_binary = true;
		terms.push({term, skip_binary});
		for(const auto& trans : transformations)
		{
			const auto& filter(trans.Filter);
			auto i(std::find_if(term.begin(), term.end(), filter));

			if(i != term.end())
				switch(trans.Kind)
				{
				case TransformationSpec::NAry:
					term = SeparatorTransformer::Process(std::move(term),
						trans.MakePrefix(i->Value), filter);
					break;
				case TransformationSpec::BinaryAssocLeft:
				case TransformationSpec::BinaryAssocRight:
					if(skip_binary)
						break;
					if(trans.Kind == TransformationSpec::BinaryAssocLeft)
					{
						const auto ri(std::find_if(term.rbegin(), term.rend(),
							filter));

						assert(ri != term.rend());
						i = ri.base();
						--i;
					}
					if(i != term.begin() && std::next(i) != term.end())
					{
						const auto a(term.get_allocator());
						auto res(Unilang::AsTermNode(a, yforward(term).Value));
						auto im(std::make_move_iterator(i));
						using it_t = decltype(im);
						const auto range_add([&](it_t b, it_t e){
							const auto add([&](TermNode& node, it_t j){
								node.Add(Unilang::Deref(j));
							});

							assert(b != e);
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
						});

						res.Add(
							Unilang::AsTermNode(a, trans.MakePrefix(i->Value)));
						range_add(std::make_move_iterator(term.begin()), im);
						range_add(++im, std::make_move_iterator(term.end()));
						term = std::move(res);
					}
				}
		}
	}
}


GlobalState::GlobalState(TermNode::allocator_type a)
	: Allocator(a), ConvertLeaf([this](const GParsedValue<ByteParser>& str){
	TermNode term(Allocator);
	const auto id(YSLib::make_string_view(str));

	if(!id.empty())
		ParseLeaf(term, id);
	return term;
}), ConvertLeafSourced([this](const GParsedValue<SourcedByteParser>& val,
	const Context& ctx){
	TermNode term(Allocator);
	const auto id(YSLib::make_string_view(val.second));

	if(!id.empty())
		ParseLeafWithSourceInformation(term, id, ctx.CurrentSource,
			val.first);
	return term;
})
{}

TermNode
GlobalState::Read(string_view unit, Context& ctx)
{
	LexicalAnalyzer lexer;

	if(UseSourceLocation)
	{
		SourcedByteParser parse(lexer, Allocator);

		return Prepare(ctx, unit.begin(), unit.end(), ystdex::ref(parse));
	}
	return Prepare(lexer, ctx, unit.begin(), unit.end());
}

TermNode
GlobalState::ReadFrom(std::streambuf& buf, Context& ctx) const
{
	using s_it_t = std::istreambuf_iterator<char>;
	LexicalAnalyzer lexer;

	if(UseSourceLocation)
	{
		SourcedByteParser parse(lexer, Allocator);

		return Prepare(ctx, s_it_t(&buf), s_it_t(), ystdex::ref(parse));
	}
	return Prepare(lexer, ctx, s_it_t(&buf), s_it_t());
}
TermNode
GlobalState::ReadFrom(std::istream& is, Context& ctx) const
{
	if(is)
	{
		if(const auto p = is.rdbuf())
			return ReadFrom(*p, ctx);
		throw std::invalid_argument("Invalid stream buffer found.");
	}
	else
		throw std::invalid_argument("Invalid stream found.");
}

} // namespace Unilang;

