// SPDX-FileCopyrightText: 2021, 2023 UnionTech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Math_h_
#define INC_Unilang_Math_h_ 1

#include "TermAccess.h" // for ValueObject, TypedValueAccessor,
//	Unilang::ResolveTerm, ThrowTypeErrorForInvalidType;
#include "Context.h" // for ReductionStatus;

namespace Unilang
{

inline namespace Math
{

struct NumberLeaf
{};

struct NumberNode
{};


YB_ATTR_nodiscard YB_PURE bool
IsExactValue(const ValueObject&) noexcept;

YB_ATTR_nodiscard YB_PURE bool
IsInexactValue(const ValueObject&) noexcept;

YB_ATTR_nodiscard YB_PURE inline bool
IsNumberValue(const ValueObject& vo) noexcept
{
	return IsExactValue(vo) || IsInexactValue(vo);
}

YB_ATTR_nodiscard YB_PURE bool
IsRationalValue(const ValueObject&) noexcept;

YB_ATTR_nodiscard YB_PURE bool
IsIntegerValue(const ValueObject&) noexcept;


YB_ATTR_nodiscard YB_PURE bool
IsFinite(const ValueObject&) noexcept;

YB_ATTR_nodiscard YB_PURE bool
IsInfinite(const ValueObject&) noexcept;

YB_ATTR_nodiscard YB_PURE bool
IsNaN(const ValueObject&) noexcept;


YB_ATTR_nodiscard YB_PURE bool
Equal(const ValueObject&, const ValueObject&) noexcept;

YB_ATTR_nodiscard YB_PURE bool
Less(const ValueObject&, const ValueObject&) noexcept;

YB_ATTR_nodiscard YB_PURE bool
Greater(const ValueObject&, const ValueObject&) noexcept;

YB_ATTR_nodiscard YB_PURE bool
LessEqual(const ValueObject&, const ValueObject&) noexcept;

YB_ATTR_nodiscard YB_PURE bool
GreaterEqual(const ValueObject&, const ValueObject&) noexcept;


YB_ATTR_nodiscard YB_PURE bool
IsZero(const ValueObject&) noexcept;

YB_ATTR_nodiscard YB_PURE bool
IsPositive(const ValueObject&) noexcept;

YB_ATTR_nodiscard YB_PURE bool
IsNegative(const ValueObject&) noexcept;

YB_ATTR_nodiscard YB_PURE bool
IsOdd(const ValueObject&) noexcept;

YB_ATTR_nodiscard YB_PURE bool
IsEven(const ValueObject&) noexcept;


YB_ATTR_nodiscard YB_PURE ValueObject
Max(ResolvedArg<>&&, ResolvedArg<>&&);

YB_ATTR_nodiscard YB_PURE ValueObject
Min(ResolvedArg<>&&, ResolvedArg<>&&);

YB_ATTR_nodiscard YB_PURE ValueObject
Add1(ResolvedArg<>&&);

YB_ATTR_nodiscard YB_PURE ValueObject
Sub1(ResolvedArg<>&&);

YB_ATTR_nodiscard YB_PURE ValueObject
Plus(ResolvedArg<>&&, ResolvedArg<>&&);

YB_ATTR_nodiscard YB_PURE ValueObject
Minus(ResolvedArg<>&&, ResolvedArg<>&&);

YB_ATTR_nodiscard YB_PURE ValueObject
Multiplies(ResolvedArg<>&&, ResolvedArg<>&&);

YB_ATTR_nodiscard YB_PURE ValueObject
Divides(ResolvedArg<>&&, ResolvedArg<>&&);

YB_ATTR_nodiscard YB_PURE ValueObject
Abs(ResolvedArg<>&&);

YB_ATTR_nodiscard YB_PURE array<ValueObject, 2>
FloorDivides(ResolvedArg<>&&, ResolvedArg<>&&);

YB_ATTR_nodiscard YB_PURE ValueObject
FloorQuotient(ResolvedArg<>&&, ResolvedArg<>&&);

YB_ATTR_nodiscard YB_PURE ValueObject
FloorRemainder(ResolvedArg<>&&, ResolvedArg<>&&);

YB_ATTR_nodiscard YB_PURE array<ValueObject, 2>
TruncateDivides(ResolvedArg<>&&, ResolvedArg<>&&);

YB_ATTR_nodiscard YB_PURE ValueObject
TruncateQuotient(ResolvedArg<>&&, ResolvedArg<>&&);

YB_ATTR_nodiscard YB_PURE ValueObject
TruncateRemainder(ResolvedArg<>&&, ResolvedArg<>&&);

YB_ATTR_nodiscard YB_PURE ValueObject
Inexact(ResolvedArg<>&&);


YB_ATTR_nodiscard YB_PURE ValueObject
StringToNumber(const string&) noexcept;

YB_ATTR_nodiscard YB_PURE string
NumberToString(const ResolvedArg<>&);

} // inline namespace Math;

void
ReadDecimal(ValueObject&, string_view, string_view::const_iterator);

ReductionStatus
ReadNumber(ValueObject&, string_view);


// XXX: These functions are known problematic with non-default locales.
YB_ATTR_nodiscard YB_PURE string
FPToString(float, string::allocator_type = {});
YB_ATTR_nodiscard YB_PURE string
FPToString(double, string::allocator_type = {});
YB_ATTR_nodiscard YB_PURE string
FPToString(long double, string::allocator_type = {});

YB_ATTR_nodiscard YB_PURE string
NumberValueToString(const ValueObject&, string::allocator_type = {});


template<>
struct TypedValueAccessor<NumberLeaf>
{
	template<class _tTerm>
	YB_ATTR_nodiscard YB_PURE inline auto
	operator()(_tTerm& term) const -> yimpl(decltype((term.Value)))
	{
		return Unilang::ResolveTerm(
			[](_tTerm& nd, bool has_ref) -> yimpl(decltype((term.Value))){
			if(IsLeaf(nd))
			{
				if(IsNumberValue(nd.Value))
					return nd.Value;
				ThrowTypeErrorForInvalidType("number", nd, has_ref);
			}
			ThrowListTypeErrorForInvalidType("number", nd, has_ref);
		}, term);
	}
};

template<>
struct TypedValueAccessor<NumberNode>
{
	template<class _tTerm>
	YB_ATTR_nodiscard YB_PURE inline ResolvedArg<>
	operator()(_tTerm& term) const
	{
		return Unilang::ResolveTerm(
			[](_tTerm& nd, ResolvedTermReferencePtr p_ref) -> ResolvedArg<>{
			if(IsLeaf(nd))
			{
				if(IsNumberValue(nd.Value))
					return {nd, p_ref};
				ThrowTypeErrorForInvalidType("number", nd, p_ref);
			}
			ThrowListTypeErrorForInvalidType("number", nd, p_ref);
		}, term);
	}
};

} // namespace Unilang;

#endif

