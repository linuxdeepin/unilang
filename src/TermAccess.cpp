// SPDX-FileCopyrightText: 2020-2023 UnionTech Software Technology Co.,Ltd.

#include "TermAccess.h" // for TermToNamePtr, sfmt, ystdex::sfmt, TryAccessLeafAtom, IsTyped,
//	Unilang::Nonnull;
#include <cassert> // for assert;
#include "Exception.h" // for ListTypeError, TypeError, ValueCategoryMismatch;
#include <ystdex/deref_op.hpp> // for ystdex::call_value_or;
#include <ystdex/functional.hpp> // for ystdex::compose, std::mem_fn,
//	ystdex::invoke_value_or;
#include "Context.h" // for complete Environment;

namespace Unilang
{

string
TermToString(const TermNode& term, size_t n_skip)
{
	if(const auto p = TermToNamePtr(term))
		return *p;

	const bool non_list(!IsList(term));

	assert(n_skip <= CountPrefix(term) && "Invalid skip number found.");

	const bool s_is_pair(n_skip < term.size()
		&& IsSticky(std::next(term.begin(), ptrdiff_t(n_skip))->Tags));

	return !non_list && n_skip == term.size() ? string("()") : sfmt<string>(
		"#<%s{%zu}%s%s>", s_is_pair ? (non_list ? "improper-list" : "list")
		: "unknown", term.size() - n_skip, s_is_pair ? " . "
		: (non_list ? ":" : ""), non_list ? term.Value.type().name() : "");
}

string
TermToStringWithReferenceMark(const TermNode& term, bool has_ref, size_t n_skip)
{
	auto term_str(TermToString(term, n_skip));

	return has_ref ? "[*] " + std::move(term_str) : std::move(term_str);
}

TermTags
TermToTags(TermNode& term)
{
	AssertReferentTags(term);
	return ystdex::call_value_or(ystdex::compose(GetLValueTagsOf,
		std::mem_fn(&TermReference::GetTags)),
		TryAccessLeafAtom<const TermReference>(term), term.Tags);
}


#if Unilang_CheckTermReferenceIndirection
TermNode&
TermReference::get() const
{
	if(r_env.GetAnchorPtr() && r_env.GetPtr().lock())
		return term_ref.get();
	throw InvalidReference("Invalid reference found on indirection, probably"
		" due to invalid context access by a dangling reference.");
}
#endif


namespace
{

YB_ATTR_nodiscard YB_STATELESS TermTags
MergeTermTags(TermTags x, TermTags y) noexcept
{
	return (((x & ~TermTags::Temporary) | y) & ~TermTags::Unique)
		| (x & y & TermTags::Unique);
}

} // unnamed namespace;

pair<TermReference, bool>
Collapse(TermReference ref)
{
	if(const auto p = TryAccessLeafAtom<TermReference>(ref.get()))
	{
		ref.SetTags(MergeTermTags(ref.GetTags(), p->GetTags())),
		ref.SetReferent(p->get());
		return {std::move(ref), true};
	}
	return {std::move(ref), {}};
}

TermNode
PrepareCollapse(TermNode& term, const shared_ptr<Environment>& p_env)
{
	return IsTyped<TermReference>(term) ? term : Unilang::AsTermNode(
		term.get_allocator(), TermReference(p_env->MakeTermTags(term), term,
		Unilang::Nonnull(p_env), Unilang::Deref(p_env).GetAnchorPtr()));
}


bool
IsReferenceTerm(const TermNode& term)
{
	return bool(TryAccessLeafAtom<const TermReference>(term));
}

bool
IsUniqueTerm(const TermNode& term)
{
	return ystdex::invoke_value_or(&TermReference::IsUnique,
		TryAccessLeafAtom<const TermReference>(term), true);
}

bool
IsModifiableTerm(const TermNode& term)
{
	return ystdex::invoke_value_or(&TermReference::IsModifiable,
		TryAccessLeafAtom<const TermReference>(term),
		!bool(term.Tags & TermTags::Nonmodifying));
}

bool
IsBoundLValueTerm(const TermNode& term)
{
	return ystdex::invoke_value_or(&TermReference::IsReferencedLValue,
		TryAccessLeafAtom<const TermReference>(term));
}

bool
IsUncollapsedTerm(const TermNode& term)
{
	return ystdex::call_value_or(ystdex::compose(IsReferenceTerm,
		std::mem_fn(&TermReference::get)),
		TryAccessLeafAtom<const TermReference>(term));
}

} // namespace Unilang;

