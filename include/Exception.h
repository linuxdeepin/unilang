// © 2020-2021 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Exceptions_h_
#define INC_Unilang_Exceptions_h_ 1

#include "Unilang.h" // for size_t, string, string_view, shared_ptr,
//	std::string, Unilang::Deref;
#include <exception> // for std::runtime_error;

namespace Unilang
{

class UnilangException : public std::runtime_error
{
	using runtime_error::runtime_error;
};


class TypeError : public UnilangException
{
	using UnilangException::UnilangException;
};


class ValueCategoryMismatch : public TypeError
{
	using TypeError::TypeError;
};


class ListReductionFailure : public TypeError
{
	using TypeError::TypeError;
};


class ListTypeError : public TypeError
{
public:
	using TypeError::TypeError;
};


class ParameterMismatch : public ListTypeError
{
	using ListTypeError::ListTypeError;
};


class ArityMismatch : public ParameterMismatch
{
private:
	size_t expected;
	size_t received;

public:
	ArityMismatch(size_t, size_t);

	YB_ATTR_nodiscard YB_PURE size_t
	GetExpected() const noexcept
	{
		return expected;
	}

	YB_ATTR_nodiscard YB_PURE size_t
	GetReceived() const noexcept
	{
		return received;
	}
};


class InvalidSyntax : public UnilangException
{
	using UnilangException::UnilangException;
};


class BadIdentifier : public InvalidSyntax
{
private:
	shared_ptr<string> p_identifier;

public:
	YB_NONNULL(2)
	BadIdentifier(const char*, size_t = 0);
	BadIdentifier(string_view, size_t = 0);

	YB_ATTR_nodiscard YB_PURE const string&
	GetIdentifier() const noexcept
	{
		return Unilang::Deref(p_identifier);
	}

private:
	std::string
	InitBadIdentifierExceptionString(std::string&& id, size_t n)
	{
		return (n != 0 ? (n == 1 ? "Bad identifier: '" 
			: "Duplicate identifier: '")
			: "Unknown identifier: '") + std::move(id) + "'.";
	}
};


class InvalidReference : public UnilangException
{
public:
	using UnilangException::UnilangException;
};


YB_NORETURN void
ThrowNonmodifiableErrorForAssignee();

} // namespace Unilang;

#endif

