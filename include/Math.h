// © 2021 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Math_h_
#define INC_Unilang_Math_h_ 1

#include "TermAccess.h" // for ValueObject, TypedValueAccessor,
//	Unilang::ResolveTerm, ThrowTypeErrorForInvalidType;
#include <ystdex/operators.hpp> // for ystdex::ordered_field_operators;

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


using Number = int;

} // inline namespace Math;

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

