// SPDX-FileCopyrightText: 2020-2023 UnionTech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Exceptions_h_
#define INC_Unilang_Exceptions_h_ 1

#include "Lexical.h" // for size_t, default_allocator, byte, std::string,
//	string_view, shared_ptr, SourceInformation, Unilang::Deref;
#include <exception> // for std::runtime_error;
#include <ystdex/invoke.hpp> // for ystdex::invoke;
#include "TermNode.h" // for TermNode;

namespace Unilang
{

class UnilangException : public std::runtime_error
{
public:
	using runtime_error::runtime_error;

	UnilangException(const UnilangException&) = default;
	~UnilangException() override;

	virtual void
	ReplaceAllocator(default_allocator<yimpl(byte)>)
	{}
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
	shared_ptr<std::string> p_identifier;

public:
	SourceInformation Source{};

	YB_NONNULL(2)
	BadIdentifier(const char*, size_t = 0);
	BadIdentifier(string_view, size_t = 0);
	BadIdentifier(const BadIdentifier&) = default;
	~BadIdentifier() override;

	YB_ATTR_nodiscard YB_PURE const std::string&
	GetIdentifier() const noexcept
	{
		return Unilang::Deref(p_identifier);
	}

	void
	ReplaceAllocator(default_allocator<yimpl(byte)>) override;
};


class InvalidReference : public UnilangException
{
public:
	using UnilangException::UnilangException;

	InvalidReference(const InvalidReference&) = default;
	~InvalidReference() override;
};


template<typename _fCallable, typename... _tParams>
auto
GuardExceptionsForAllocator(default_allocator<yimpl(byte)> a, _fCallable&& f,
	_tParams&&... args)
	-> decltype(ystdex::invoke(yforward(f), yforward(args)...))
{
	TryExpr(ystdex::invoke(yforward(f), yforward(args)...))
	catch(UnilangException& e)
	{
		e.ReplaceAllocator(a);
		throw;
	}
}


YB_NORETURN void
ThrowInsufficientTermsError(const TermNode&, bool, size_t = 0);

YB_NORETURN void
ThrowListTypeErrorForAtom(const TermNode&, bool, size_t = 0);

YB_NORETURN YB_NONNULL(1) void
ThrowListTypeErrorForInvalidType(const char*, const TermNode&, bool,
	size_t = 0);
YB_NORETURN void
ThrowListTypeErrorForInvalidType(const type_info&, const TermNode&, bool,
	size_t = 0);

YB_NORETURN void
ThrowListTypeErrorForNonList(const TermNode&, bool, size_t = 0);

YB_NORETURN YB_NONNULL(1,2) void
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

