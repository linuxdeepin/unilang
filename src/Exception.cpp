// © 2020-2021 Uniontech Software Technology Co.,Ltd.

#include "Exception.h" // for std::string, YSLib::to_std_string, ystdex::sfmt,
//	make_shared;
#include "Lexical.h" // for EscapeLiteral;
#include "TermAccess.h" // for TermToStringWithReferenceMark;
#include <cassert> // for assert;

namespace Unilang
{

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
ThrowNonmodifiableErrorForAssignee()
{
	throw TypeError("Destination operand of assignment shall be modifiable.");
}

} // namespace Unilang;

