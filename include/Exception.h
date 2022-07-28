// © 2020-2022 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Exceptions_h_
#define INC_Unilang_Exceptions_h_ 1

#include "Lexical.h" // for size_t, string, string_view, shared_ptr,
//	SourceInformation, Unilang::Deref;
#include <exception> // for std::runtime_error;
#include "TermNode.h" // for TermNode;

namespace Unilang
{

class UnilangException : public std::runtime_error
{
public:
	using runtime_error::runtime_error;

	UnilangException(const UnilangException&) = default;
	~UnilangException() override;
};


class TypeError : public UnilangException
{
public:
	using UnilangException::UnilangException;

	TypeError(const TypeError&) = default;
	~TypeError() override;
};


class ValueCategoryMismatch : public TypeError
{
public:
	using TypeError::TypeError;

	ValueCategoryMismatch(const ValueCategoryMismatch&) = default;
	~ValueCategoryMismatch() override;
};


class ListTypeError : public TypeError
{
public:
	using TypeError::TypeError;

	ListTypeError(const ListTypeError&) = default;
	~ListTypeError() override;
};


class ListReductionFailure : public TypeError
{
public:
	using TypeError::TypeError;

	ListReductionFailure(const ListReductionFailure&) = default;
	~ListReductionFailure() override;
};


class InvalidSyntax : public UnilangException
{
public:
	using UnilangException::UnilangException;

	InvalidSyntax(const InvalidSyntax&) = default;
	~InvalidSyntax() override;
};


class ParameterMismatch : public ListTypeError
{
public:
	using ListTypeError::ListTypeError;

	ParameterMismatch(const ParameterMismatch&) = default;
	~ParameterMismatch() override;
};


class ArityMismatch : public ParameterMismatch
{
private:
	size_t expected;
	size_t received;

public:
	ArityMismatch(size_t, size_t);
	ArityMismatch(const ArityMismatch&) = default;
	~ArityMismatch() override;

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


class BadIdentifier : public InvalidSyntax
{
private:
	shared_ptr<string> p_identifier;

public:
	SourceInformation Source{};

	YB_NONNULL(2)
	BadIdentifier(const char*, size_t = 0);
	BadIdentifier(string_view, size_t = 0);
	BadIdentifier(const BadIdentifier&) = default;
	~BadIdentifier() override;

	YB_ATTR_nodiscard YB_PURE const string&
	GetIdentifier() const noexcept
	{
		return Unilang::Deref(p_identifier);
	}
};


class InvalidReference : public UnilangException
{
public:
	using UnilangException::UnilangException;

	InvalidReference(const InvalidReference&) = default;
	~InvalidReference() override;
};


YB_NORETURN void
ThrowInsufficientTermsError(const TermNode&, bool, size_t = 0);

YB_NORETURN YF_API YB_NONNULL(1) void
ThrowListTypeErrorForInvalidType(const char*, const TermNode&, bool,
	size_t = 0);
YB_NORETURN void
ThrowListTypeErrorForInvalidType(const type_info&, const TermNode&, bool,
	size_t = 0);

YB_NORETURN void
ThrowListTypeErrorForNonlist(const TermNode&, bool, size_t = 0);

YB_NORETURN YF_API YB_NONNULL(1,2) void
ThrowTypeErrorForInvalidType(const char*, const char*);
YB_NORETURN YB_NONNULL(1) void
ThrowTypeErrorForInvalidType(const char*, const TermNode&, bool, size_t = 0);
YB_NORETURN void
ThrowTypeErrorForInvalidType(const type_info&, const TermNode&, bool,
	size_t = 0);

YB_NORETURN void
ThrowValueCategoryError(const TermNode&);


YB_NORETURN void
ThrowNonmodifiableErrorForAssignee();

} // namespace Unilang;

#endif

