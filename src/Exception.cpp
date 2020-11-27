// © 2020 Uniontech Software Technology Co.,Ltd.

#include "Exception.h" // for std::string, make_shared;

namespace Unilang
{

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
	: InvalidSyntax(InitBadIdentifierExceptionString(std::string(id.data(),
	id.data() + id.size()), n)), p_identifier(make_shared<string>(id))
{}

} // namespace Unilang;

