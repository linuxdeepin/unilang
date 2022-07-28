// © 2020-2022 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_BasicReduction_h_
#define INC_Unilang_BasicReduction_h_ 1

#include "TermNode.h" // for TermNode, ystdex::ref_eq, yforward;
#include <algorithm> // for std::for_each;
#include <ystdex/meta.hpp> // for ystdex::exclude_self_t;
#include YFM_YSLib_Core_YObject // for YSLib::EmplaceCallResult;

namespace Unilang
{

enum class ReductionStatus : size_t
{
	Partial = 0x00,
	Neutral = 0x01,
	Clean = 0x02,
	Retained = 0x03,
	Regular = Retained,
	Retrying = 0x10
};

YB_ATTR_nodiscard YB_STATELESS constexpr bool
CheckReducible(ReductionStatus status) noexcept
{
	return status == ReductionStatus::Partial
		|| status == ReductionStatus::Retrying;
}


ReductionStatus
RegularizeTerm(TermNode&, ReductionStatus) noexcept;


inline void
LiftOther(TermNode& term, TermNode& tm)
{
	term.MoveContent(std::move(tm));
}

inline void
LiftOtherValue(TermNode& term, TermNode& tm)
{
	AssertValueTags(tm);
	LiftOther(term, tm);
}

inline void
LiftTerm(TermNode& term, TermNode& tm)
{
	if(!ystdex::ref_eq<>()(term, tm))
		LiftOther(term, tm);
}

void
LiftOtherOrCopy(TermNode&, TermNode&, bool);


void
LiftToReturn(TermNode&);

TNIter
LiftPrefixToReturn(TermNode&, TNCIter);
inline TNIter
LiftPrefixToReturn(TermNode& term)
{
	return LiftPrefixToReturn(term, term.begin());
}

inline void
LiftSubtermsToReturn(TermNode& term)
{
	std::for_each(term.begin(), term.end(), LiftToReturn);
}

ReductionStatus
ReduceBranchToList(TermNode&) noexcept;

ReductionStatus
ReduceBranchToListValue(TermNode& term) noexcept;

inline ReductionStatus
ReduceForLiftedResult(TermNode& term)
{
	LiftToReturn(term);
	return ReductionStatus::Retained;
}


YB_ATTR_nodiscard YB_STATELESS constexpr ReductionStatus
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
template<size_t _vN>
YB_ATTR_nodiscard inline ReductionStatus
EmplaceCallResultOrReturn(TermNode& term, array<ValueObject, _vN> arg)
{
	TermNode::Container con(term.get_allocator());

	for(auto& vo : arg)
		con.emplace_back(NoContainer, std::move(vo));
	con.swap(term.GetContainerRef());
	return ReductionStatus::Retained;
}

} // namespace Unilang;

#endif
