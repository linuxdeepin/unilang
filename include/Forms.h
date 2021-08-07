// © 2020-2021 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Forms_h_
#define INC_Unilang_Forms_h_ 1

#include "Evaluation.h" // for ReductionStatus, TermNode, IsBranch,
//	YSLib::EmplaceCallResult, Unilang::Deref, yforward, Strict,
//	Unilang::RegisterHandler, Context;
#include <ystdex/meta.hpp> // for ystdex::exclude_self_t;
#include <cassert> // for assert;
#include <ystdex/functional.hpp> // for ystdex::expand_proxy,
//	ystdex::invoke_nonvoid, ystdex::make_expanded, ystdex::bind1,
//	std::placeholders::_2, std::ref;
#include <iterator> // for std::next;
#include <ystdex/iterator.hpp> // for ystdex::make_transform;
#include "TermAccess.h" // for Unilang::ResolveRegular;
#include <numeric> // for std::accumulate;
#include <ystdex/operators.hpp> // for ystdex::equality_comparable;
#include <ystdex/examiner.hpp> // for ystdex::examiners;

namespace Unilang
{

namespace Forms
{

inline ReductionStatus
Retain(const TermNode& term) noexcept
{
	static_cast<void>(term);
	assert(IsBranch(term));
	return ReductionStatus::Retained;
}

size_t
RetainN(const TermNode&, size_t = 1);


[[nodiscard, gnu::const]] constexpr ReductionStatus
EmplaceCallResultOrReturn(TermNode&, ReductionStatus status) noexcept
{
	return status;
}
template<typename _tParam, typename... _tParams, yimpl(
	typename = ystdex::exclude_self_t<ReductionStatus, _tParam>)>
YB_ATTR_nodiscard inline ReductionStatus
EmplaceCallResultOrReturn(TermNode& term, _tParam&& arg)
{
	YSLib::EmplaceCallResult(term.Value, yforward(arg), term.get_allocator());
	return ReductionStatus::Clean;
}

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

template<typename _func, typename... _tParams>
ReductionStatus
CallUnary(_func&& f, TermNode& term, _tParams&&... args)
{
	return Forms::CallRawUnary([&](TermNode& tm){
		return Forms::EmplaceCallResultOrReturn(term, ystdex::invoke_nonvoid(
			ystdex::make_expanded<void(TermNode&, _tParams&&...)>(std::ref(f)),
			tm, yforward(args)...));
	}, term);
}

template<typename _type, typename _type2, typename _func, typename... _tParams>
ReductionStatus
CallBinaryAs(_func&& f, TermNode& term, _tParams&&... args)
{
	RetainN(term, 2);

	auto i(term.begin());
	auto& x(Unilang::ResolveRegular<_type>(Unilang::Deref(++i)));

	return Forms::EmplaceCallResultOrReturn(term, ystdex::invoke_nonvoid(
		ystdex::make_expanded<void(_type&, _type2&, _tParams&&...)>(
		std::ref(f)), x, Unilang::ResolveRegular<_type2>(Unilang::Deref(++i)),
		yforward(args)...));
}

template<typename _type, typename _func, typename... _tParams>
ReductionStatus
CallBinaryFold(_func f, _type val, TermNode& term, _tParams&&... args)
{
	const auto n(term.size() - 1);
	auto i(term.begin());
	const auto j(ystdex::make_transform(++i, [](TNIter it){
		return Unilang::ResolveRegular<_type>(Unilang::Deref(it));
	}));

	return Forms::EmplaceCallResultOrReturn(term, std::accumulate(j, std::next(
		j, typename std::iterator_traits<decltype(j)>::difference_type(n)), val,
		ystdex::bind1(f, std::placeholders::_2, yforward(args)...)));
}


template<typename _func>
struct UnaryExpansion
	: private ystdex::equality_comparable<UnaryExpansion<_func>>
{
	_func Function;

	UnaryExpansion(_func f)
		: Function(std::move(f))
	{}

	YB_ATTR_nodiscard YB_PURE friend bool
	operator==(const UnaryExpansion& x, const UnaryExpansion& y)
	{
		return ystdex::examiners::equal_examiner::are_equal(x.Function,
			y.Function);
	}

	template<typename... _tParams>
	inline ReductionStatus
	operator()(TermNode& term, _tParams&&... args) const
	{
		RetainN(term);
		return Forms::EmplaceCallResultOrReturn(term, ystdex::invoke_nonvoid(
			ystdex::make_expanded<void(TermNode&, _tParams&&...)>(
			std::ref(Function)), Unilang::Deref(std::next(term.begin())),
			yforward(args)...));
	}
};


template<typename _type, typename _func>
struct UnaryAsExpansion
	: private ystdex::equality_comparable<UnaryAsExpansion<_type, _func>>
{
	_func Function;

	UnaryAsExpansion(_func f)
		: Function(std::move(f))
	{}

	YB_ATTR_nodiscard YB_PURE friend bool
	operator==(const UnaryAsExpansion& x, const UnaryAsExpansion& y)
	{
		return ystdex::examiners::equal_examiner::are_equal(x.Function,
			y.Function);
	}

	template<typename... _tParams>
	inline ReductionStatus
	operator()(TermNode& term, _tParams&&... args) const
	{
		RetainN(term);
		return Forms::EmplaceCallResultOrReturn(term, ystdex::invoke_nonvoid(
			ystdex::make_expanded<void(_type&, _tParams&&...)>(
			std::ref(Function)), Unilang::ResolveRegular<_type>(
			Unilang::Deref(std::next(term.begin()))), yforward(args)...));
	}
};


template<typename _func>
struct BinaryExpansion
	: private ystdex::equality_comparable<BinaryExpansion<_func>>
{
	_func Function;

	BinaryExpansion(_func f)
		: Function(std::move(f))
	{}

	YB_ATTR_nodiscard YB_PURE friend bool
	operator==(const BinaryExpansion& x, const BinaryExpansion& y)
	{
		return ystdex::examiners::equal_examiner::are_equal(x.Function,
			y.Function);
	}

	template<typename... _tParams>
	inline ReductionStatus
	operator()(TermNode& term, _tParams&&... args) const
	{
		RetainN(term, 2);

		auto i(term.begin());
		auto& x(Unilang::Deref(++i));

		return Forms::EmplaceCallResultOrReturn(term, ystdex::invoke_nonvoid(
			ystdex::make_expanded<void(TermNode&, TermNode&, _tParams&&...)>(
			std::ref(Function)), x, Unilang::Deref(++i), yforward(args)...));
	}
};


template<typename _type, typename _type2, typename _func>
struct BinaryAsExpansion : private
	ystdex::equality_comparable<BinaryAsExpansion<_type, _type2, _func>>
{
	_func Function;

	BinaryAsExpansion(_func f)
		: Function(std::move(f))
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
Unwrap(TermNode&);


ReductionStatus
MakeEncapsulationType(TermNode& term);


ReductionStatus
Sequence(TermNode&, Context&);

} // namespace Forms;

} // namespace Unilang;

#endif

