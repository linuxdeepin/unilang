// © 2020 Uniontech Software Technology Co.,Ltd.

#include "BasicReduction.h"
#include "TermAccess.h" // for Unilang::TryAccessLeaf;
#include "Exception.h" // for ListTypeError;

namespace Unilang
{

ReductionStatus
RegularizeTerm(TermNode& term, ReductionStatus status) noexcept
{
	if(status == ReductionStatus::Clean)
		term.ClearContainer();
	return status;
}


void
LiftOther(TermNode& term, TermNode& tm)
{
	assert(&term != &tm);

	const auto t(std::move(term.GetContainerRef()));

	term.GetContainerRef() = std::move(tm.GetContainerRef());
	term.Value = std::move(tm.Value);
}

void
LiftOtherOrCopy(TermNode& term, TermNode& tm, bool move)
{
	if(move)
		LiftOther(term, tm);
	else
	{
		term.GetContainerRef() = tm.GetContainer();
		term.Value = tm.Value;
	}
}


void
LiftToReturn(TermNode& term)
{
	if(const auto p = Unilang::TryAccessLeaf<const TermReference>(term))
		term = p->get();
}

ReductionStatus
ReduceBranchToList(TermNode& term) noexcept
{
	RemoveHead(term);
	return ReductionStatus::Retained;
}

ReductionStatus
ReduceBranchToListValue(TermNode& term) noexcept
{
	RemoveHead(term);
	LiftSubtermsToReturn(term);
	return ReductionStatus::Retained;
}

} // namespace Unilang;

