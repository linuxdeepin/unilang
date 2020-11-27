// © 2020 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_BasicReduction_h_
#define INC_Unilang_BasicReduction_h_ 1

#include "TermNode.h" // for TermNode, ystdex::ref_eq;
#include <algorithm> // for std::for_each;

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

[[nodiscard, gnu::const]] constexpr bool
CheckReducible(ReductionStatus status) noexcept
{
	return status == ReductionStatus::Partial
		|| status == ReductionStatus::Retrying;
}


ReductionStatus
RegularizeTerm(TermNode&, ReductionStatus) noexcept;


void
LiftOther(TermNode&, TermNode&);

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

} // namespace Unilang;

#endif
