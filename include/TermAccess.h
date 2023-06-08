// SPDX-FileCopyrightText: 2020-2023 UnionTech Software Technology Co.,Ltd.

#ifndef INC_Unilang_TermAccess_h_
#define INC_Unilang_TermAccess_h_ 1

#include "TermNode.h" // for string, TermNode, IsPair, YSLib::TryAccessValue,
//	weak_ptr, AssertReferentTags, Unilang::IsMovable, PropagateTo, pair,
//	Unilang::Deref;
#include "Exception.h" // for ThrowListTypeErrorForInvalidType;
#include <ystdex/operators.hpp> // for ystdex::equality_comparable;
#include <ystdex/type_op.hpp> // for ystdex::exclude_self_params_t;
#include <ystdex/meta.hpp> // for ystdex::enable_if_constructible_t;
#include <ystdex/expanded_function.hpp> // for ystdex::expand_proxy;
#include <ystdex/compose.hpp> // for ystdex::compose_n;

namespace Unilang
{

// NOTE: The host type of symbol.
class TokenValue final : public string
{
public:
	using base = string;

	TokenValue() = default;
	using base::base;
	TokenValue(const base& b)
		: base(b)
	{}
	TokenValue(base&& b) noexcept
		: base(std::move(b))
	{}
	TokenValue(const TokenValue&) = default;
	TokenValue(TokenValue&&) = default;

	TokenValue&
	operator=(const TokenValue&) = default;
	TokenValue&
	operator=(TokenValue&&) = default;
};


using AnchorPtr = shared_ptr<const void>;

static_assert(ystdex::is_nothrow_copy_constructible<AnchorPtr>(),
	"Invalid type found.");
static_assert(ystdex::is_nothrow_copy_assignable<AnchorPtr>(),
	"Invalid type found.");
static_assert(ystdex::is_nothrow_move_assignable<AnchorPtr>(),
	"Invalid type found.");
static_assert(ystdex::is_nothrow_move_constructible<AnchorPtr>(),
	"Invalid type found.");


inline void
ClearCombiningTags(TermNode& term) noexcept
{
	EnsureValueTags(term.Tags);
	AssertValueTags(term);
}

YB_ATTR_nodiscard YB_PURE inline bool
IsCombiningTerm(const TermNode& term) noexcept
{
	return IsPair(term);
}

inline void
AssertCombiningTerm(const TermNode& term) noexcept
{
	yunused(term);
	assert(IsCombiningTerm(term) && "Invalid term found for combined term.");
	assert(!(IsList(term) && HasStickySubterm(term))
		&& "Invalid representation found.");
}

template<typename _type, class _tTerm>
void
CheckRegular(_tTerm& term, bool has_ref)
{
	if(IsBranch(term))
		ThrowListTypeErrorForInvalidType(type_id<_type>(), term, has_ref);
}

template<typename _type, class _tTerm>
YB_ATTR_nodiscard YB_PURE inline auto
AccessRegular(_tTerm& term, bool has_ref)
	-> decltype(Access<_type>(term))
{
	Unilang::CheckRegular<_type>(term, has_ref);
	return Access<_type>(term);
}

using YSLib::TryAccessValue;

template<typename _type>
YB_ATTR_nodiscard YB_PURE inline observer_ptr<_type>
TryAccessLeaf(TermNode& term)
{
	return TryAccessValue<_type>(term.Value);
}
template<typename _type>
YB_ATTR_nodiscard YB_PURE inline observer_ptr<const _type>
TryAccessLeaf(const TermNode& term)
{
	return TryAccessValue<_type>(term.Value);
}

template<typename _type>
YB_ATTR_nodiscard YB_PURE inline observer_ptr<_type>
TryAccessLeafAtom(TermNode& term)
{
	return IsAtom(term) ? TryAccessLeaf<_type>(term) : nullptr;
}
template<typename _type>
YB_ATTR_nodiscard YB_PURE inline observer_ptr<const _type>
TryAccessLeafAtom(const TermNode& term)
{
	return IsAtom(term) ? TryAccessLeaf<_type>(term) : nullptr;
}

template<typename _type>
YB_ATTR_nodiscard YB_PURE inline observer_ptr<_type>
TryAccessTerm(TermNode& term)
{
	return IsLeaf(term) ? TryAccessLeaf<_type>(term) : nullptr;
}
template<typename _type>
YB_ATTR_nodiscard YB_PURE inline observer_ptr<const _type>
TryAccessTerm(const TermNode& term)
{
	return IsLeaf(term) ? TryAccessLeaf<_type>(term) : nullptr;
}

YB_ATTR_nodiscard YB_PURE string
TermToString(const TermNode&, size_t = 0);

YB_ATTR_nodiscard YB_PURE string
TermToStringWithReferenceMark(const TermNode&, bool, size_t = 0);

YB_ATTR_nodiscard YB_PURE inline observer_ptr<const TokenValue>
TermToNamePtr(const TermNode& term)
{
	return TryAccessTerm<TokenValue>(term);
}

YB_ATTR_nodiscard YB_PURE TermTags
TermToTags(TermNode&);


#ifndef Unilang_CheckTermReferenceIndirection
#	ifndef NDEBUG
#		define Unilang_CheckTermReferenceIndirection true
#else
#		define Unilang_CheckTermReferenceIndirection false
#	endif
#endif


class Environment;


class EnvironmentBase
{
private:
	AnchorPtr p_anchor{};

public:
	EnvironmentBase() = default;
	template<typename... _tParams, yimpl(typename
		= ystdex::exclude_self_params_t<EnvironmentBase, _tParams...>)>
	inline
	EnvironmentBase(_tParams&&... args) noexcept
		: p_anchor(yforward(args)...)
	{}
	EnvironmentBase(const EnvironmentBase&) = default;
	EnvironmentBase(EnvironmentBase&&) = default;

	EnvironmentBase&
	operator=(const EnvironmentBase&) = default;
	EnvironmentBase&
	operator=(EnvironmentBase&&) = default;

	friend void
	swap(EnvironmentBase& x, EnvironmentBase& y) noexcept
	{
		swap(x.p_anchor, y.p_anchor);
	}

	YB_ATTR_nodiscard YB_PURE bool
	IsOrphan() const noexcept
	{
		return p_anchor.use_count() == 1;
	}

	YB_ATTR_nodiscard YB_PURE size_t
	GetAnchorCount() const noexcept
	{
		return size_t(p_anchor.use_count());
	}

	YB_ATTR_nodiscard YB_PURE const AnchorPtr&
	GetAnchorPtr() const noexcept
	{
		return p_anchor;
	}
};


class EnvironmentReference final : private EnvironmentBase,
	private ystdex::equality_comparable<EnvironmentReference>
{
private:
	weak_ptr<Environment> p_weak{};

public:
	EnvironmentReference() = default;
	EnvironmentReference(const shared_ptr<Environment>&) noexcept;
	template<typename _tParam1, typename _tParam2, yimpl(typename
		= ystdex::enable_if_constructible_t<EnvironmentBase, _tParam2>,
		typename = ystdex::enable_if_constructible_t<weak_ptr<Environment>,
		_tParam1>)>
	EnvironmentReference(_tParam1&& arg1, _tParam2&& arg2) noexcept
		: EnvironmentBase(yforward(arg2)),
		p_weak(yforward(arg1))
	{}
	EnvironmentReference(const EnvironmentReference&) = default;
	EnvironmentReference(EnvironmentReference&&) = default;

	EnvironmentReference&
	operator=(const EnvironmentReference&) = default;
	EnvironmentReference&
	operator=(EnvironmentReference&&) = default;

	YB_ATTR_nodiscard YB_PURE friend bool
	operator==(const EnvironmentReference& x, const EnvironmentReference& y)
		noexcept
	{
		return x.p_weak.lock() == y.p_weak.lock();
	}

	using EnvironmentBase::GetAnchorPtr;

	YB_ATTR_nodiscard YB_PURE const weak_ptr<Environment>&
	GetPtr() const noexcept
	{
		return p_weak;
	}

	shared_ptr<Environment>
	Lock() const noexcept
	{
		return p_weak.lock();
	}
};


// NOTE: The host type of reference values.
class TermReference final : private ystdex::equality_comparable<TermReference>
{
private:
	lref<TermNode> term_ref;
	TermTags tags = TermTags::Unqualified;
	EnvironmentReference r_env;

public:
	template<typename _tParam, typename... _tParams>
	inline
	TermReference(TermNode& term, _tParam&& arg, _tParams&&... args) noexcept
		: TermReference(TermToTags(term), term, yforward(arg),
		yforward(args)...)
	{}
	template<typename _tParam, typename... _tParams>
	inline
	TermReference(TermTags t, TermNode& term, _tParam&& arg, _tParams&&... args)
		noexcept
		: term_ref(term), tags((AssertReferentTags(t), t)),
		r_env(yforward(arg), yforward(args)...)
	{}
	TermReference(TermTags t, const TermReference& ref) noexcept
		: term_ref(ref.term_ref), tags((AssertReferentTags(t), t)),
		r_env(ref.r_env)
	{}
	TermReference(TermTags t, TermReference&& ref) noexcept
		: term_ref(ref.term_ref), tags((AssertReferentTags(t), t)),
		r_env(std::move(ref.r_env))
	{}
	TermReference(const TermReference&) = default;

	TermReference&
	operator=(const TermReference&) = default;

	YB_ATTR_nodiscard YB_PURE friend bool
	operator==(const TermReference& x, const TermReference& y) noexcept
	{
		return &x.term_ref.get() == &y.term_ref.get();
	}

	YB_ATTR_nodiscard YB_PURE explicit
	operator TermNode&() const noexcept
	{
		return term_ref;
	}

	YB_ATTR_nodiscard YB_PURE bool
	IsModifiable() const noexcept
	{
		return !bool(tags & TermTags::Nonmodifying);
	}
	YB_ATTR_nodiscard YB_PURE bool
	IsMovable() const noexcept
	{
		return Unilang::IsMovable(tags);
	}
	YB_ATTR_nodiscard YB_PURE bool
	IsReferencedLValue() const noexcept
	{
		return !(bool(tags & TermTags::Unique)
			|| bool(tags & TermTags::Temporary));
	}
	YB_ATTR_nodiscard YB_PURE bool
	IsUnique() const noexcept
	{
		return bool(tags & TermTags::Unique);
	}

	YB_ATTR_nodiscard YB_PURE TermTags
	GetTags() const noexcept
	{
		return tags;
	}
	YB_ATTR_nodiscard YB_PURE const EnvironmentReference&
	GetEnvironmentReference() const noexcept
	{
		return r_env;
	}

	void
	SetReferent(TermNode& t) noexcept
	{
		term_ref = t;
	}
	void
	SetTags(TermTags t) noexcept
	{
		tags = t;
	}

	void
	AddTags(TermTags t) noexcept
	{
		tags |= t;
	}

	void
	PropagateFrom(TermTags t) noexcept
	{
		tags = PropagateTo(tags, t);
	}

	void
	RemoveTags(TermTags t) noexcept
	{
		tags &= ~t;
	}

#if Unilang_CheckTermReferenceIndirection
	YB_ATTR_nodiscard YB_PURE TermNode&
	get() const;
#else
	YB_ATTR_nodiscard YB_PURE TermNode&
	get() const noexcept
	{
		return term_ref.get();
	}
#endif

};


YB_ATTR_nodiscard YF_API pair<TermReference, bool>
Collapse(TermReference);

YB_ATTR_nodiscard TermNode
PrepareCollapse(TermNode&, const shared_ptr<Environment>&);

YB_ATTR_nodiscard YB_PURE inline TermNode&
ReferenceLeaf(TermNode& term)
{
	if(const auto p = TryAccessLeaf<const TermReference>(term))
		return p->get();
	return term;
}
YB_ATTR_nodiscard YB_PURE inline const TermNode&
ReferenceLeaf(const TermNode& term)
{
	if(const auto p = TryAccessLeaf<const TermReference>(term))
		return p->get();
	return term;
}

YB_ATTR_nodiscard YB_PURE inline TermNode&
ReferenceTerm(TermNode& term)
{
	if(const auto p = TryAccessLeafAtom<const TermReference>(term))
		return p->get();
	return term;
}
YB_ATTR_nodiscard YB_PURE inline const TermNode&
ReferenceTerm(const TermNode& term)
{
	if(const auto p = TryAccessLeafAtom<const TermReference>(term))
		return p->get();
	return term;
}


using ResolvedTermReferencePtr = const TermReference*;

YB_ATTR_nodiscard YB_PURE constexpr ResolvedTermReferencePtr
ResolveToTermReferencePtr(observer_ptr<const TermReference> p) noexcept
{
	return p.get();
}


YB_ATTR_nodiscard YB_PURE inline bool
IsMovable(const TermReference& ref) noexcept
{
	return ref.IsMovable();
}
template<typename _tPointer>
YB_ATTR_nodiscard YB_PURE inline auto
IsMovable(_tPointer p) noexcept
	-> decltype(!bool(p) || Unilang::IsMovable(Unilang::Deref(p)))
{
	return !bool(p) || Unilang::IsMovable(Unilang::Deref(p));
}


template<typename _type>
YB_ATTR_nodiscard YB_PURE inline observer_ptr<_type>
TryAccessReferencedLeaf(TermNode& term)
{
	return TryAccessLeafAtom<_type>(ReferenceTerm(term));
}
template<typename _type>
YB_ATTR_nodiscard YB_PURE inline observer_ptr<const _type>
TryAccessReferencedLeaf(const TermNode& term)
{
	return TryAccessLeafAtom<_type>(ReferenceTerm(term));
}

template<typename _type>
YB_ATTR_nodiscard YB_PURE inline observer_ptr<_type>
TryAccessReferencedTerm(TermNode& term)
{
	return TryAccessTerm<_type>(ReferenceTerm(term));
}
template<typename _type>
YB_ATTR_nodiscard YB_PURE inline observer_ptr<const _type>
TryAccessReferencedTerm(const TermNode& term)
{
	return TryAccessTerm<_type>(ReferenceTerm(term));
}

YB_ATTR_nodiscard YB_PURE bool
IsReferenceTerm(const TermNode&);

YB_ATTR_nodiscard YB_PURE bool
IsUniqueTerm(const TermNode&);

YB_ATTR_nodiscard YB_PURE bool
IsModifiableTerm(const TermNode&);

YB_ATTR_nodiscard YB_PURE bool
IsBoundLValueTerm(const TermNode&);

YB_ATTR_nodiscard YB_PURE bool
IsUncollapsedTerm(const TermNode&);


template<typename _func, class _tTerm>
inline auto
ResolveBy(_func do_resolve, _tTerm&& term, observer_ptr<const TermReference> p)
	-> yimpl(decltype(ystdex::expand_proxy<yimpl(void)(_tTerm&&,
	ResolvedTermReferencePtr)>::call(do_resolve, yforward(term),
	ResolvedTermReferencePtr())))
{
	using handler_t = yimpl(void)(_tTerm&&, ResolvedTermReferencePtr);

	if(p)
		return ystdex::expand_proxy<handler_t>::call(do_resolve, p->get(),
			Unilang::ResolveToTermReferencePtr(p));
	return ystdex::expand_proxy<handler_t>::call(do_resolve,
		yforward(term), ResolvedTermReferencePtr());
}

template<typename _func, class _tTerm>
inline auto
ResolveSuffix(_func do_resolve, _tTerm&& term)
	-> yimpl(decltype(Unilang::ResolveBy(std::move(do_resolve), yforward(term),
	TryAccessLeaf<const TermReference>(term))))
{
	return Unilang::ResolveBy(std::move(do_resolve), yforward(term),
		TryAccessLeaf<const TermReference>(term));
}

template<typename _func, class _tTerm>
inline auto
ResolveTerm(_func do_resolve, _tTerm&& term)
	-> yimpl(decltype(Unilang::ResolveBy(std::move(do_resolve), yforward(term),
	TryAccessLeafAtom<const TermReference>(term))))
{
	return Unilang::ResolveBy(std::move(do_resolve), yforward(term),
		TryAccessLeafAtom<const TermReference>(term));
}

template<typename _type, class _tTerm>
YB_ATTR_nodiscard YB_PURE inline auto
ResolveRegular(_tTerm& term) -> decltype(Access<_type>(term))
{
	return Unilang::ResolveTerm([&](_tTerm& nd, bool has_ref)
		-> decltype(Access<_type>(term)){
		return AccessRegular<_type>(nd, has_ref);
	}, term);
}


template<typename _type>
struct TypedValueAccessor
{
	template<class _tTerm>
	YB_ATTR_nodiscard YB_PURE inline auto
	operator()(_tTerm& term) const
		-> yimpl(decltype(Access<_type>(term)))
	{
		return Unilang::ResolveRegular<_type>(term);
	}
};

template<typename _type>
struct TypedValueAccessor<const _type>
	: TypedValueAccessor<_type>
{};


template<typename _type, class _tTerm>
YB_ATTR_nodiscard YB_PURE inline auto
AccessTypedValue(_tTerm& term) noexcept(noexcept(TypedValueAccessor<_type>()(
	term))) -> yimpl(decltype(TypedValueAccessor<_type>()(term)))
{
	return TypedValueAccessor<_type>()(term);
}


template<typename _type = TermNode>
struct ResolvedArg : pair<lref<_type>, ResolvedTermReferencePtr>
{
	using BaseType = pair<lref<_type>, ResolvedTermReferencePtr>;

	using BaseType::first;
	using BaseType::second;

	using BaseType::BaseType;

	bool
	IsModifiable() const noexcept
	{
		return !second || second->IsModifiable();
	}
	bool
	IsMovable() const noexcept
	{
		return Unilang::IsMovable(second);
	}

	_type&
	get() const noexcept
	{
		return first.get();
	}
};


template<typename _type>
struct TypedValueAccessor<ResolvedArg<_type>>
{
	template<class _tTerm>
	YB_ATTR_nodiscard YB_PURE inline ResolvedArg<_type>
	operator()(_tTerm& term) const
	{
		return Unilang::ResolveTerm([](_tTerm& nd, ResolvedTermReferencePtr p_ref){
			return ResolvedArg<_type>(
				AccessRegular<_type>(nd, p_ref), p_ref);
		}, term);
	}
};

template<>
struct TypedValueAccessor<ResolvedArg<>>
{
	template<class _tTerm>
	YB_ATTR_nodiscard YB_PURE inline ResolvedArg<>
	operator()(_tTerm& term) const
	{
		return Unilang::ResolveTerm([](_tTerm& nd, ResolvedTermReferencePtr p_ref){
			return ResolvedArg<>(nd, p_ref);
		}, term);
	}
};


struct ReferenceLeafOp
{
	template<typename _type>
	auto
	operator()(_type&& term) const
		noexcept(noexcept(Unilang::ReferenceLeaf(yforward(term))))
		-> decltype(Unilang::ReferenceLeaf(yforward(term)))
	{
		return Unilang::ReferenceLeaf(yforward(term));
	}
};

struct ReferenceTermOp
{
	template<typename _type>
	auto
	operator()(_type&& term) const
		noexcept(noexcept(Unilang::ReferenceTerm(yforward(term))))
		-> decltype(Unilang::ReferenceTerm(yforward(term)))
	{
		return Unilang::ReferenceTerm(yforward(term));
	}
};

template<typename _func>
YB_STATELESS auto
ComposeReferencedLeafOp(_func f)
	-> decltype(ystdex::compose_n(f, ReferenceLeafOp()))
{
	return ystdex::compose_n(f, ReferenceLeafOp());
}

template<typename _func>
YB_STATELESS auto
ComposeReferencedTermOp(_func f)
	-> decltype(ystdex::compose_n(f, ReferenceTermOp()))
{
	return ystdex::compose_n(f, ReferenceTermOp());
}

} // namespace Unilang;

#endif

