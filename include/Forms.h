// © 2020-2022 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Forms_h_
#define INC_Unilang_Forms_h_ 1

#include "Evaluation.h" // for ReductionStatus, TermNode, ReferenceTerm,
//	YSLib::EmplaceCallResult, yforward, Unilang::Deref,
//	Unilang::EmplaceCallResultOrReturn, Unilang::AccessTypedValue, Strict,
//	Unilang::RegisterHandler, Context;
#include <ystdex/meta.hpp> // for ystdex::exclude_self_t;
#include <cassert> // for assert;
#include <ystdex/functional.hpp> // for ystdex::expand_proxy,
//	ystdex::invoke_nonvoid, ystdex::make_expanded, ystdex::bind1,
//	std::placeholders::_2, std::ref;
#include <iterator> // for std::next;
#include "TermAccess.h" // for ResolvedTermReferencePtr,
//	Unilang::ResolveRegular;
#include <ystdex/iterator.hpp> // for ystdex::make_transform;
#include <numeric> // for std::accumulate;
#include <ystdex/operators.hpp> // for ystdex::equality_comparable;
#include <ystdex/examiner.hpp> // for ystdex::examiners;
#include <ystdex/type_op.hpp> // for ystdex::exclude_self_params_t;

namespace Unilang
{

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
		return x.Get() == y.Get()
			&& Equal(ReferenceTerm(x.TermRef), ReferenceTerm(y.TermRef));
	}

	using EncapsulationBase::Get;
	using EncapsulationBase::GetType;

	static bool
	Equal(const TermNode&, const TermNode&);
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

	ReductionStatus
	operator()(TermNode&) const;
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

	ReductionStatus
	operator()(TermNode&) const;
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
	operator()(TermNode&, Context&) const;
};

namespace Forms
{

template<typename _func, typename... _tParams>
inline auto
CallRawUnary(_func&& f, TermNode& term, _tParams&&... args)
	-> yimpl(decltype(ystdex::expand_proxy<void(TermNode&, _tParams&&...)>
	::call(f, Unilang::Deref(std::next(term.begin())), yforward(args)...)))
{
	RetainN(term);
	return ystdex::expand_proxy<yimpl(void)(TermNode&, _tParams&&...)>::call(f,
		Unilang::Deref(std::next(term.begin())), yforward(args)...);
}

template<typename _func>
inline auto
CallResolvedUnary(_func&& f, TermNode& term)
	-> yimpl(decltype(Unilang::ResolveTerm(yforward(f), term)))
{
	return Forms::CallRawUnary([&](TermNode& tm)
		-> yimpl(decltype(Unilang::ResolveTerm(yforward(f), term))){
		return Unilang::ResolveTerm(yforward(f), tm);
	}, term);
}

template<typename _type, typename _func, typename... _tParams>
inline auto
CallRegularUnaryAs(_func&& f, TermNode& term, _tParams&&... args)
	-> yimpl(decltype(ystdex::expand_proxy<void(_type&, const
	ResolvedTermReferencePtr&, _tParams&&...)>::call(f, Unilang::Access<_type>(
	term), ResolvedTermReferencePtr(), std::forward<_tParams>(args)...)))
{
	using handler_t
		= yimpl(void)(_type&, const ResolvedTermReferencePtr&, _tParams&&...);

	return Forms::CallResolvedUnary(
		[&](TermNode& nd, ResolvedTermReferencePtr p_ref)
		-> decltype(ystdex::expand_proxy<handler_t>::call(f,
		std::declval<_type&>(), p_ref)){
		return ystdex::expand_proxy<handler_t>::call(f, Unilang::AccessRegular<
			_type>(nd, p_ref), p_ref, std::forward<_tParams>(args)...);
	}, term);
}

template<typename _func, typename... _tParams>
ReductionStatus
CallUnary(_func&& f, TermNode& term, _tParams&&... args)
{
	return Forms::CallRawUnary([&](TermNode& tm){
		return Unilang::EmplaceCallResultOrReturn(term, ystdex::invoke_nonvoid(
			ystdex::make_expanded<void(TermNode&, _tParams&&...)>(std::ref(f)),
			tm, yforward(args)...));
	}, term);
}

template<typename _type, typename _func, typename... _tParams>
ReductionStatus
CallUnaryAs(_func&& f, TermNode& term, _tParams&&... args)
{
	return Forms::CallUnary([&](TermNode& tm){
		return ystdex::make_expanded<void(decltype(Unilang::AccessTypedValue<
			_type>(tm)), _tParams&&...)>(std::ref(f))(Unilang::AccessTypedValue<
			_type>(tm), std::forward<_tParams>(args)...);
	}, term);
}

template<typename _func, typename... _tParams>
ReductionStatus
CallBinary(_func&& f, TermNode& term, _tParams&&... args)
{
	RetainN(term, 2);

	auto i(term.begin());
	auto& x(Unilang::Deref(++i));

	return Unilang::EmplaceCallResultOrReturn(term, ystdex::invoke_nonvoid(
		ystdex::make_expanded<void(TermNode&, TermNode&, _tParams&&...)>(
		std::ref(f)), x, Unilang::Deref(++i), yforward(args)...));
}

template<typename _type, typename _type2, typename _func, typename... _tParams>
ReductionStatus
CallBinaryAs(_func&& f, TermNode& term, _tParams&&... args)
{
	RetainN(term, 2);

	auto i(term.begin());
	auto&& x(Unilang::AccessTypedValue<_type>(Unilang::Deref(++i)));

	return Unilang::EmplaceCallResultOrReturn(term, ystdex::invoke_nonvoid(
		ystdex::make_expanded<void(decltype(x), decltype(
		Unilang::AccessTypedValue<_type2>(*i)), _tParams&&...)>(std::ref(f)),
		yforward(x), Unilang::AccessTypedValue<_type2>(Unilang::Deref(++i)),
		yforward(args)...));
}

template<typename _type, typename _func, typename... _tParams>
ReductionStatus
CallBinaryFold(_func f, _type val, TermNode& term, _tParams&&... args)
{
	const auto n(term.size() - 1);
	auto i(term.begin());
	const auto j(ystdex::make_transform(++i, [](TNIter it){
		return Unilang::AccessTypedValue<_type>(Unilang::Deref(it));
	}));

	return Unilang::EmplaceCallResultOrReturn(term, std::accumulate(j, std::next(
		j, typename std::iterator_traits<decltype(j)>::difference_type(n)), val,
		ystdex::bind1(f, std::placeholders::_2, yforward(args)...)));
}


template<typename _func>
struct UnaryExpansion
	: private ystdex::equality_comparable<UnaryExpansion<_func>>
{
	_func Function;

	template<typename... _tParams, yimpl(typename
		= ystdex::exclude_self_params_t<UnaryExpansion, _tParams...>)>
	UnaryExpansion(_tParams&&... args)
		: Function(yforward(args)...)
	{}

	YB_ATTR_nodiscard YB_PURE friend bool
	operator==(const UnaryExpansion& x, const UnaryExpansion& y)
	{
		return ystdex::examiners::equal_examiner::are_equal(x.Function,
			y.Function);
	}

	template<typename... _tParams>
	inline ReductionStatus
	operator()(_tParams&&... args) const
	{
		return Forms::CallUnary(Function, yforward(args)...);
	}
};


template<typename _type, typename _func>
struct UnaryAsExpansion
	: private ystdex::equality_comparable<UnaryAsExpansion<_type, _func>>
{
	_func Function;

	template<typename... _tParams, yimpl(typename
		= ystdex::exclude_self_params_t<UnaryAsExpansion, _tParams...>)>
	UnaryAsExpansion(_tParams&&... args)
		: Function(yforward(args)...)
	{}

	YB_ATTR_nodiscard YB_PURE friend bool
	operator==(const UnaryAsExpansion& x, const UnaryAsExpansion& y)
	{
		return ystdex::examiners::equal_examiner::are_equal(x.Function,
			y.Function);
	}

	template<typename... _tParams>
	inline ReductionStatus
	operator()(_tParams&&... args) const
	{
		return Forms::CallUnaryAs<_type>(Function, yforward(args)...);
	}
};


template<typename _func>
struct BinaryExpansion
	: private ystdex::equality_comparable<BinaryExpansion<_func>>
{
	_func Function;

	template<typename... _tParams, yimpl(typename
		= ystdex::exclude_self_params_t<BinaryExpansion, _tParams...>)>
	BinaryExpansion(_tParams&&... args)
		: Function(yforward(args)...)
	{}

	YB_ATTR_nodiscard YB_PURE friend bool
	operator==(const BinaryExpansion& x, const BinaryExpansion& y)
	{
		return ystdex::examiners::equal_examiner::are_equal(x.Function,
			y.Function);
	}

	template<typename... _tParams>
	inline ReductionStatus
	operator()(_tParams&&... args) const
	{
		return Forms::CallBinary(Function, yforward(args)...);
	}
};


template<typename _type, typename _type2, typename _func>
struct BinaryAsExpansion : private
	ystdex::equality_comparable<BinaryAsExpansion<_type, _type2, _func>>
{
	_func Function;

	template<typename... _tParams, yimpl(typename
		= ystdex::exclude_self_params_t<BinaryAsExpansion, _tParams...>)>
	BinaryAsExpansion(_tParams&&... args)
		: Function(yforward(args)...)
	{}

	YB_ATTR_nodiscard YB_PURE friend bool
	operator==(const BinaryAsExpansion& x, const BinaryAsExpansion& y)
	{
		return ystdex::examiners::equal_examiner::are_equal(x.Function,
			y.Function);
	}

	template<typename... _tParams>
	inline ReductionStatus
	operator()(_tParams&&... args) const
	{
		return Forms::CallBinaryAs<_type, _type2>(Function, yforward(args)...);
	}
};


template<size_t _vWrapping = Strict, typename _func, class _tTarget>
inline void
RegisterUnary(_tTarget& target, string_view name, _func f)
{
	Unilang::RegisterHandler<_vWrapping>(target, name,
		UnaryExpansion<_func>(std::move(f)));
}
template<size_t _vWrapping = Strict, typename _type, typename _func,
	class _tTarget>
inline void
RegisterUnary(_tTarget& target, string_view name, _func f)
{
	Unilang::RegisterHandler<_vWrapping>(target, name,
		UnaryAsExpansion<_type, _func>(std::move(f)));
}

template<size_t _vWrapping = Strict, typename _func, class _tTarget>
inline void
RegisterBinary(_tTarget& target, string_view name, _func f)
{
	Unilang::RegisterHandler<_vWrapping>(target, name,
		BinaryExpansion<_func>(std::move(f)));
}
template<size_t _vWrapping = Strict, typename _type, typename _type2,
	typename _func, class _tTarget>
inline void
RegisterBinary(_tTarget& target, string_view name, _func f)
{
	Unilang::RegisterHandler<_vWrapping>(target, name,
		BinaryAsExpansion<_type, _type2, _func>(std::move(f)));
}


void
Eq(TermNode&);

void
EqValue(TermNode&);


ReductionStatus
If(TermNode&, Context&);


ReductionStatus
Cons(TermNode&);

ReductionStatus
ConsRef(TermNode&);


ReductionStatus
Eval(TermNode&, Context&);

ReductionStatus
EvalRef(TermNode&, Context&);


void
MakeEnvironment(TermNode&);

void
GetCurrentEnvironment(TermNode&, Context&);


ReductionStatus
Define(TermNode&, Context&);


ReductionStatus
Vau(TermNode&, Context&);

ReductionStatus
VauRef(TermNode&, Context&);

ReductionStatus
VauWithEnvironment(TermNode&, Context&);

ReductionStatus
VauWithEnvironmentRef(TermNode&, Context&);


ReductionStatus
Wrap(TermNode&);

ReductionStatus
WrapRef(TermNode&);

ReductionStatus
Unwrap(TermNode&);


ReductionStatus
CheckListReference(TermNode&);


ReductionStatus
MakeEncapsulationType(TermNode& term);


ReductionStatus
Sequence(TermNode&, Context&);


ReductionStatus
Call1CC(TermNode&, Context&);

ReductionStatus
ContinuationToApplicative(TermNode&);

} // namespace Forms;

} // namespace Unilang;

#endif

