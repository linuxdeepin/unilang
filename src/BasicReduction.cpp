// © 2020-2022 Uniontech Software Technology Co.,Ltd.

#include "BasicReduction.h" // for IsSticky;
#include "TermAccess.h" // for ClearCombiningTags, EnsureValueTags,
//	TryAccessLeafAtom, TermReference;
#include "TermNode.h" // for AssertValueTags;
#include <cassert> // for assert;
#include <algorithm> // for std::distance;
#include <ystdex/utility.hpp> // for ystdex::as_const;
#include <ystdex/container.hpp> // for ystdex::cast_mutable;

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

void
LiftTermOrCopy(TermNode& term, TermNode& tm, bool move)
{
	if(move)
		LiftTerm(term, tm);
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
	AssertValueTags(term);
}

} // unnamed namespace;

void
LiftToReturn(TermNode& term)
{
	if(const auto p = TryAccessLeafAtom<const TermReference>(term))
		LiftMovedOther(term, *p, p->IsMovable());
	AssertValueTags(term);
}

TNIter
LiftPrefixToReturn(TermNode& term, TNCIter it)
{
	assert(size_t(std::distance(ystdex::as_const(term).begin(), it))
		<= CountPrefix(term) && "Invalid arguments found.");

	auto i(ystdex::cast_mutable(term.GetContainerRef(), it));

	while(i != term.end() && !IsSticky(i->Tags))
	{
		LiftToReturn(*i);
		++i;
	}
	assert((term.Value || i == term.end()) && "Invalid representation found.");
	return i;
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

