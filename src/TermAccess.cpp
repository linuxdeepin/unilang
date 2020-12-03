// © 2020 Uniontech Software Technology Co.,Ltd.

#include "TermAccess.h"
#include "Exception.h" // for ListTypeError;

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

void
ThrowInsufficientTermsError(const TermNode& term, bool has_ref)
{
	throw ParameterMismatch(ystdex::sfmt(
		"Insufficient subterms found in '%s' for the list parameter.",
		TermToStringWithReferenceMark(term, has_ref).c_str()));
}

void
ThrowListTypeErrorForInvalidType(const ystdex::type_info& tp,
	const TermNode& term, bool is_ref)
{
	throw ListTypeError(ystdex::sfmt("Expected a value of type '%s', got a list"
		" '%s'.", tp.name(),
		TermToStringWithReferenceMark(term, is_ref).c_str()));
}

void
ThrowListTypeErrorForNonlist(const TermNode& term, bool has_ref)
{
	throw ListTypeError(ystdex::sfmt("Expected a list, got '%s'.",
		TermToStringWithReferenceMark(term, has_ref).c_str()));
}

} // namespace Unilang;

