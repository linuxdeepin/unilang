// © 2020 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Forms_h_
#define INC_Unilang_Forms_h_ 1

#include "Evaluation.h" // for ReductionStatus, TermNode, IsBranch,
//	YSLib::EmplaceCallResult, Strict, Unilang::RegisterHandler, Context,
//	YSLib::HoldSame;
#include <cassert> // for assert;
#include "TermAccess.h" // for Unilang::ResolveRegular;
#include <ystdex/operators.hpp> // for ystdex::equality_comparable;
#include <ystdex/examiner.hpp> // for ystdex::invoke_nonvoid;
#include <iterator> // for std::next;
#include <ystdex/functional.hpp> // for function, ystdex::make_expanded,
//	std::ref;

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


template<typename _func>
struct UnaryExpansion
	: private ystdex::equality_comparable<UnaryExpansion<_func>>
{
	_func Function;

	UnaryExpansion(_func f)
		: Function(std::move(f))
	{}

	[[nodiscard, gnu::pure]] friend bool
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
		YSLib::EmplaceCallResult(term.Value, ystdex::invoke_nonvoid(
			ystdex::make_expanded<void(TermNode&, _tParams&&...)>(
			std::ref(Function)), *std::next(term.begin()), yforward(args)...));
		return ReductionStatus::Clean;
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

	[[nodiscard, gnu::pure]] friend bool
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
		YSLib::EmplaceCallResult(term.Value, ystdex::invoke_nonvoid(
			ystdex::make_expanded<void(_type&, _tParams&&...)>(
			std::ref(Function)), Unilang::ResolveRegular<_type>(
			*std::next(term.begin())), yforward(args)...));
		return ReductionStatus::Clean;
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


ReductionStatus
Equal(TermNode&);

ReductionStatus
EqualValue(TermNode&);


ReductionStatus
If(TermNode&, Context&);


ReductionStatus
Cons(TermNode&);


ReductionStatus
Eval(TermNode&, Context&);


ReductionStatus
MakeEnvironment(TermNode&);

ReductionStatus
GetCurrentEnvironment(TermNode&, Context&);


ReductionStatus
Define(TermNode&, Context&);


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

