// © 2020-2021 Uniontech Software Technology Co.,Ltd.

#include "TermAccess.h" // for sfmt, ystdex::sfmt, Unilang::Nonnull;
#include "Exception.h" // for ListTypeError, TypeError, ValueCategoryMismatch;
#include <ystdex/functional.hpp> // for ystdex::compose, std::mem_fn,
//	ystdex::invoke_value_or;
#include <ystdex/deref_op.hpp> // for ystdex::call_valu_or;
#include "Context.h" // for complete Environment;

namespace Unilang
{

string
TermToString(const TermNode& term)
{
	if(const auto p = TermToNamePtr(term))
		return *p;
	return sfmt<string>("#<unknown{%zu}:%s>", term.size(),
		term.Value.type().name());
}

string
TermToStringWithReferenceMark(const TermNode& term, bool has_ref)
{
	auto term_str(TermToString(term));

	return has_ref ? "[*] " + std::move(term_str) : std::move(term_str);
}

TermTags
TermToTags(TermNode& term)
{
	return ystdex::call_value_or(ystdex::compose(GetLValueTagsOf,
		std::mem_fn(&TermReference::GetTags)),
		Unilang::TryAccessLeaf<const TermReference>(term), term.Tags);
}

void
ThrowInsufficientTermsError(const TermNode& term, bool has_ref)
{
	throw ParameterMismatch(ystdex::sfmt(
		"Insufficient subterms found in '%s' for the list parameter.",
		TermToStringWithReferenceMark(term, has_ref).c_str()));
}

void
ThrowListTypeErrorForInvalidType(const char* name, const TermNode& term,
	bool has_ref)
{
	throw ListTypeError(ystdex::sfmt("Expected a value of type '%s', got a list"
		" '%s'.", name, TermToStringWithReferenceMark(term, has_ref).c_str()));
}
void
ThrowListTypeErrorForInvalidType(const type_info& ti,
	const TermNode& term, bool has_ref)
{
	ThrowListTypeErrorForInvalidType(ti.name(), term, has_ref);
}

void
ThrowListTypeErrorForNonlist(const TermNode& term, bool has_ref)
{
	throw ListTypeError(ystdex::sfmt("Expected a list, got '%s'.",
		TermToStringWithReferenceMark(term, has_ref).c_str()));
}

void
ThrowTypeErrorForInvalidType(const char* name, const TermNode& term,
	bool has_ref)
{
	throw TypeError(ystdex::sfmt("Expected a value of type '%s', got '%s'.",
		name, TermToStringWithReferenceMark(term, has_ref).c_str()));
}
void
ThrowTypeErrorForInvalidType(const type_info& ti, const TermNode& term,
	bool has_ref)
{
	ThrowTypeErrorForInvalidType(ti.name(), term, has_ref);
}

void
ThrowValueCategoryError(const TermNode& term)
{
	throw ValueCategoryMismatch(ystdex::sfmt("Expected a reference for the 1st "
		"argument, got '%s'.", TermToString(term).c_str()));
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


TermNode
PrepareCollapse(TermNode& term, const shared_ptr<Environment>& p_env)
{
	if(const auto p = Unilang::TryAccessLeaf<const TermReference>(term))
		return term;
	return Unilang::AsTermNode(term.get_allocator(), TermReference(
		p_env->MakeTermTags(term), term, Unilang::Nonnull(p_env)));
}


bool
IsReferenceTerm(const TermNode& term)
{
	return bool(Unilang::TryAccessLeaf<const TermReference>(term));
}

bool
IsBoundLValueTerm(const TermNode& term)
{
	return ystdex::invoke_value_or(&TermReference::IsReferencedLValue,
		Unilang::TryAccessLeaf<const TermReference>(term));
}

bool
IsUncollapsedTerm(const TermNode& term)
{
	return ystdex::call_value_or(ystdex::compose(IsReferenceTerm,
		std::mem_fn(&TermReference::get)),
		Unilang::TryAccessLeaf<const TermReference>(term));
}

} // namespace Unilang;

