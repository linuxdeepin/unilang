// © 2020 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Context_h_
#define INC_Unilang_Context_h_ 1

#include "TermAccess.h" // for ValueObject, vector, string, TermNode,
//	ystdex::less, map, AnchorPtr, pmr, EnvironmentReference;
#include "BasicReduction.h" // for ReductionStatus;
#include <ystdex/operators.hpp> // for ystdex::equality_comparable;
#include <ystdex/container.hpp> // for ystdex::try_emplace,
//	ystdex::try_emplace_hint, ystdex::insert_or_assign;
#include <ystdex/functional.hpp> // for ystdex::expanded_function;

namespace Unilang
{

using EnvironmentList = vector<ValueObject>;


class Environment final : private ystdex::equality_comparable<Environment>
{
public:
	using BindingMap = map<string, TermNode, ystdex::less<>>;
	using NameResolution
		= pair<BindingMap::mapped_type*, shared_ptr<Environment>>;
	using allocator_type = BindingMap::allocator_type;

	mutable BindingMap Bindings;
	ValueObject Parent{};

private:
	AnchorPtr p_anchor{InitAnchor()};

public:
	Environment(allocator_type a)
		: Bindings(a)
	{}
	Environment(pmr::memory_resource& rsrc)
		: Environment(allocator_type(&rsrc))
	{}
	explicit
	Environment(const BindingMap& m)
		: Bindings(m)
	{}
	explicit
	Environment(BindingMap&& m)
		: Bindings(std::move(m))
	{}
	Environment(const ValueObject& vo, allocator_type a)
		: Bindings(a), Parent((CheckParent(vo), vo))
	{}
	Environment(ValueObject&& vo, allocator_type a)
		: Bindings(a), Parent((CheckParent(vo), std::move(vo)))
	{}
	Environment(pmr::memory_resource& rsrc, const ValueObject& vo)
		: Environment(vo, allocator_type(&rsrc))
	{}
	Environment(pmr::memory_resource& rsrc, ValueObject&& vo)
		: Environment(std::move(vo), allocator_type(&rsrc))
	{}
	Environment(const Environment& e)
		: Bindings(e.Bindings), Parent(e.Parent)
	{}
	Environment(Environment&&) = default;

	Environment&
	operator=(Environment&&) = default;

	[[nodiscard, gnu::pure]] friend bool
	operator==(const Environment& x, const Environment& y) noexcept
	{
		return &x == &y;
	}

	[[nodiscard, gnu::pure]] const AnchorPtr&
	GetAnchorPtr() const noexcept
	{
		return p_anchor;
	}

	template<typename _tKey, typename... _tParams>
	inline ystdex::enable_if_inconvertible_t<_tKey&&,
		BindingMap::const_iterator, bool>
	AddValue(_tKey&& k, _tParams&&... args)
	{
		return ystdex::try_emplace(Bindings, yforward(k), NoContainer,
			yforward(args)...).second;
	}
	template<typename _tKey, typename... _tParams>
	inline bool
	AddValue(BindingMap::const_iterator hint, _tKey&& k, _tParams&&... args)
	{
		return ystdex::try_emplace_hint(Bindings, hint, yforward(k),
			NoContainer, yforward(args)...).second;
	}

	template<typename _tKey, class _tNode>
	TermNode&
	Bind(_tKey&& k, _tNode&& tm)
	{
		return ystdex::insert_or_assign(Bindings, yforward(k),
			yforward(tm)).first->second;
	}

	static void
	CheckParent(const ValueObject&);

	static Environment&
	EnsureValid(const shared_ptr<Environment>&);

private:
	[[nodiscard, gnu::pure]] AnchorPtr
	InitAnchor() const;

public:
	[[nodiscard, gnu::pure]] NameResolution::first_type
	LookupName(string_view) const;
};


class Context;

using ReducerFunctionType = ReductionStatus(Context&);

using Reducer = ystdex::expanded_function<ReducerFunctionType>;

using ReducerSequence = forward_list<Reducer>;


class Context final
{
private:
	lref<pmr::memory_resource> memory_rsrc;
	shared_ptr<Environment> p_record{std::allocate_shared<Environment>(
		Environment::allocator_type(&memory_rsrc.get()))};
	TermNode* next_term_ptr = {};
	ReducerSequence current{};

public:
	Reducer TailAction{};
	ReductionStatus LastStatus = ReductionStatus::Neutral;

	Context(pmr::memory_resource& rsrc)
		: memory_rsrc(rsrc)
	{}

	bool
	IsAlive() const noexcept
	{
		return !current.empty();
	}

	[[nodiscard]] Environment::BindingMap&
	GetBindingsRef() const noexcept
	{
		return p_record->Bindings;
	}
	[[nodiscard, gnu::pure]] TermNode&
	GetNextTermRef() const;
	[[nodiscard, gnu::pure]] const shared_ptr<Environment>&
	GetRecordPtr() const noexcept
	{
		return p_record;
	}
	[[nodiscard]] Environment&
	GetRecordRef() const noexcept
	{
		return *p_record;
	}

	void
	SetNextTermRef(TermNode& term) noexcept
	{
		next_term_ptr = &term;
	}

	ReductionStatus
	ApplyTail();

	ReductionStatus
	Evaluate(TermNode&);

	ReductionStatus
	Rewrite(Reducer);

	ReductionStatus
	RewriteTerm(TermNode&);

	Environment::NameResolution
	Resolve(shared_ptr<Environment>, string_view);

	template<typename... _tParams>
	inline void
	SetupCurrent(_tParams&&... args)
	{
		assert(!IsAlive());
		return SetupFront(yforward(args)...);
	}

	template<typename... _tParams>
	inline void
	SetupFront(_tParams&&... args)
	{
		current.emplace_front(yforward(args)...);
	}

	[[nodiscard, gnu::pure]] shared_ptr<Environment>
	ShareRecord() const noexcept;

	shared_ptr<Environment>
	SwitchEnvironment(const shared_ptr<Environment>&);

	shared_ptr<Environment>
	SwitchEnvironmentUnchecked(const shared_ptr<Environment>&) noexcept;

	void
	UnwindCurrent() noexcept;

	[[nodiscard, gnu::pure]] EnvironmentReference
	WeakenRecord() const noexcept;

	[[nodiscard, gnu::pure]] pmr::polymorphic_allocator<byte>
	get_allocator() const noexcept
	{
		return pmr::polymorphic_allocator<byte>(&memory_rsrc.get());
	}
};


// NOTE: This is the host type for combiners.
using ContextHandler
	= ystdex::expanded_function<ReductionStatus(TermNode&, Context&)>;


template<typename... _tParams>
inline shared_ptr<Environment>
AllocateEnvironment(const Environment::allocator_type& a, _tParams&&... args)
{
	return std::allocate_shared<Environment>(a, yforward(args)...);
}
template<typename... _tParams>
inline shared_ptr<Environment>
AllocateEnvironment(Context& ctx, _tParams&&... args)
{
	return Unilang::AllocateEnvironment(ctx.GetBindingsRef().get_allocator(),
		yforward(args)...);
}
template<typename... _tParams>
inline shared_ptr<Environment>
AllocateEnvironment(TermNode& term, Context& ctx, _tParams&&... args)
{
	const auto a(ctx.GetBindingsRef().get_allocator());

	static_cast<void>(term);
	assert(a == term.get_allocator());
	return Unilang::AllocateEnvironment(a, yforward(args)...);
}

template<typename... _tParams>
inline shared_ptr<Environment>
SwitchToFreshEnvironment(Context& ctx, _tParams&&... args)
{
	return ctx.SwitchEnvironmentUnchecked(Unilang::AllocateEnvironment(ctx,
		yforward(args)...));
}


template<typename _type, typename... _tParams>
inline bool
EmplaceLeaf(Environment::BindingMap& m, string_view name, _tParams&&... args)
{
	assert(name.data());

	const auto i(m.find(name));

	if(i == m.end())
	{
		m[string(name)].Value = _type(yforward(args)...);
		return true;
	}

	auto& nd(i->second);

	nd.Value = _type(yforward(args)...);
	nd.GetContainerRef().clear();
	return {};
}
template<typename _type, typename... _tParams>
inline bool
EmplaceLeaf(Environment& env, string_view name, _tParams&&... args)
{
	return Unilang::EmplaceLeaf<_type>(env.Bindings, name, yforward(args)...);
}
template<typename _type, typename... _tParams>
inline bool
EmplaceLeaf(Context& ctx, string_view name, _tParams&&... args)
{
	return Unilang::EmplaceLeaf<_type>(ctx.GetRecordRef(), name, yforward(args)...);
}


[[nodiscard]] pair<shared_ptr<Environment>, bool>
ResolveEnvironment(const ValueObject&);
[[nodiscard]] pair<shared_ptr<Environment>, bool>
ResolveEnvironment(const TermNode&);


struct EnvironmentSwitcher
{
	lref<Context> ContextRef;
	mutable shared_ptr<Environment> SavedPtr;

	EnvironmentSwitcher(Context& ctx,
		shared_ptr<Environment>&& p_saved = {})
		: ContextRef(ctx), SavedPtr(std::move(p_saved))
	{}
	EnvironmentSwitcher(EnvironmentSwitcher&&) = default;

	EnvironmentSwitcher&
	operator=(EnvironmentSwitcher&&) = default;

	void
	operator()() const noexcept
	{
		if(SavedPtr)
			ContextRef.get().SwitchEnvironmentUnchecked(std::move(SavedPtr));
	}
};

} // namespace Unilang;

#endif

