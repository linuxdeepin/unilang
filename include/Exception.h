// © 2020 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Exceptions_h_
#define INC_Unilang_Exceptions_h_ 1

#include "Unilang.h" // for size_t, string, shared_ptr, std::string;
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


class ListTypeError : public TypeError
{
public:
	using TypeError::TypeError;
};


class ListReductionFailure : public TypeError
{
	using TypeError::TypeError;
};


class InvalidSyntax : public UnilangException
{
	using UnilangException::UnilangException;
};


class ParameterMismatch : public InvalidSyntax
{
	using InvalidSyntax::InvalidSyntax;
};


class ArityMismatch : public ParameterMismatch
{
private:
	size_t expected;
	size_t received;

public:
	ArityMismatch(size_t, size_t);

	[[nodiscard, gnu::pure]] size_t
	GetExpected() const noexcept
	{
		return expected;
	}

	[[nodiscard, gnu::pure]] size_t
	GetReceived() const noexcept
	{
		return received;
	}
};


class BadIdentifier : public InvalidSyntax
{
private:
	shared_ptr<string> p_identifier;

public:
	[[gnu::nonnull(2)]]
	BadIdentifier(const char*, size_t = 0);
	BadIdentifier(string_view, size_t = 0);

	[[nodiscard, gnu::pure]] const string&
	GetIdentifier() const noexcept
	{
		assert(p_identifier);
		return *p_identifier;
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

} // namespace Unilang;

#endif

