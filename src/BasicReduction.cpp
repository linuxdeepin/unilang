// © 2020-2022 Uniontech Software Technology Co.,Ltd.

#include "BasicReduction.h"
#include "TermAccess.h" // for ClearCombiningTags, Unilang::TryAccessLeaf,
//	TermReference;
#include "Exception.h" // for ListTypeError;

namespace Unilang
{

ReductionStatus
RegularizeTerm(TermNode& term, ReductionStatus status) noexcept
{
	ClearCombiningTags(term);
	if(status == ReductionStatus::Clean)
		term.ClearContainer();
	return status;
}


void
LiftOtherOrCopy(TermNode& term, TermNode& tm, bool move)
{
	if(move)
		LiftOther(term, tm);
	else
		term.CopyContent(tm);
}


namespace
{

void
LiftMovedOther(TermNode& term, const TermReference& ref, bool move)
{
	LiftOtherOrCopy(term, ref.get(), move);
	EnsureValueTags(term.Tags);
}

} // unnamed namespace;

void
LiftToReturn(TermNode& term)
{
	if(const auto p = Unilang::TryAccessLeaf<const TermReference>(term))
		LiftMovedOther(term, *p, p->IsMovable());
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

