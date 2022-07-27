// © 2020-2022 Uniontech Software Technology Co.,Ltd.

#include "Exception.h" // for std::string, YSLib::to_std_string, ystdex::sfmt,
//	make_shared;
#include "Lexical.h" // for EscapeLiteral;
#include "TermAccess.h" // for TermToStringWithReferenceMark;
#include <cassert> // for assert;

namespace Unilang
{

UnilangException::~UnilangException() = default;


TypeError::~TypeError() = default;


ValueCategoryMismatch::~ValueCategoryMismatch() = default;


ListTypeError::~ListTypeError() = default;


ListReductionFailure::~ListReductionFailure() = default;


InvalidSyntax::~InvalidSyntax() = default;


ParameterMismatch::~ParameterMismatch() = default;


ArityMismatch::~ArityMismatch() = default;


BadIdentifier::~BadIdentifier() = default;


InvalidReference::~InvalidReference() = default;

namespace
{

std::string
InitBadIdentifierExceptionString(string_view id, size_t n)
{
	return YSLib::to_std_string((n != 0 
		? (n == 1 ? "Bad identifier: '" : "Duplicate identifier: '")
		: "Unknown identifier: '") + EscapeLiteral(id) + "'.");
}

} // unnamed namespace;

ArityMismatch::ArityMismatch(size_t e, size_t r)
	: ParameterMismatch(
	ystdex::sfmt("Arity mismatch: expected %zu, received %zu.", e, r)),
	expected(e), received(r)
{}


BadIdentifier::BadIdentifier(const char* id, size_t n)
	: InvalidSyntax(InitBadIdentifierExceptionString(id, n)),
	p_identifier(make_shared<string>(id))
{}
BadIdentifier::BadIdentifier(string_view id, size_t n)
	: InvalidSyntax(InitBadIdentifierExceptionString(id, n)),
	p_identifier(make_shared<string>(id))
{}


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


void
ThrowNonmodifiableErrorForAssignee()
{
	throw TypeError("Destination operand of assignment shall be modifiable.");
}

} // namespace Unilang;

