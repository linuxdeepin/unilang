// © 2021-2022 Uniontech Software Technology Co.,Ltd.

#include "Math.h" // for size_t, type_info, Unilang::TryAccessValue,
//	Unilang::Nonnull, Unilang::Deref, ptrdiff_t, string_view, sfmt;
#include <ystdex/exception.h> // for ystdex::unsupported;
#include <ystdex/string.hpp> // for ystdex::sfmt;
#include <ystdex/meta.hpp> // for ystdex::_t, ystdex::exclude_self_t,
//	ystdex::enable_if_t, std::is_floating_point, ystdex::common_type_t;
#include <cassert> // for assert;
#include <cstdlib> // for std::abs;
#include <cmath> // for std::isfinite, std::nearbyint, std::isinf, std::isnan,
//	std::fmod, std::ldexp, std::pow, std::fp_classify, FP_INFINITE, FP_NAN;
#include <string> // for std::to_string;
#include <limits> // for std::numeric_limits;
#include <ystdex/functional.hpp> // for ystdex::retry_on_cond, ystdex::id;
#include <ystdex/cctype.h> // for ystdex::isdigit;
#include "BasicReduction.h" // for ReductionStatus;

namespace Unilang
{

inline namespace Math
{

namespace
{

YB_ATTR_nodiscard ValueObject
MoveUnary(ResolvedArg<>& x)
{
	ValueObject res;

	if(x.IsMovable())
		res = std::move(x.get().Value);
	else
		res = x.get().Value;
	return res;
}


YB_NORETURN void
ThrowForUnsupportedNumber()
{
	throw ystdex::unsupported("Invalid numeric type found.");
}


enum NumCode : size_t
{
	SChar = 0,
	UChar,
	Short,
	UShort,
	Int,
	UInt,
	Long,
	ULong,
	LongLong,
	ULongLong,
	IntMax = ULongLong,
	Float,
	Double,
	LongDouble,
	Max = LongDouble,
	None = size_t(-1)
};

YB_ATTR_nodiscard YB_PURE NumCode
MapTypeIdToNumCode(const type_info& ti) noexcept
{
	if(IsTyped<int>(ti))
		return Int;
	if(IsTyped<unsigned>(ti))
		return UInt;
	if(IsTyped<long long>(ti))
		return LongLong;
	if(IsTyped<unsigned long long>(ti))
		return ULongLong;
	if(IsTyped<double>(ti))
		return Double;
	if(IsTyped<long>(ti))
		return Long;
	if(IsTyped<unsigned long>(ti))
		return ULong;
	if(IsTyped<short>(ti))
		return Short;
	if(IsTyped<unsigned short>(ti))
		return UShort;
	if(IsTyped<signed char>(ti))
		return SChar;
	if(IsTyped<unsigned char>(ti))
		return UChar;
	if(IsTyped<float>(ti))
		return Float;
	if(IsTyped<long double>(ti))
		return LongDouble;
	return None;
}
YB_ATTR_nodiscard YB_PURE inline NumCode
MapTypeIdToNumCode(const ValueObject& vo) noexcept
{
	return MapTypeIdToNumCode(vo.type());
}
YB_ATTR_nodiscard YB_PURE inline NumCode
MapTypeIdToNumCode(const TermNode& nd) noexcept
{
	return MapTypeIdToNumCode(nd.Value);
}
YB_ATTR_nodiscard YB_PURE inline NumCode
MapTypeIdToNumCode(const ResolvedArg<>& arg) noexcept
{
	return MapTypeIdToNumCode(arg.get());
}


template<typename _type>
struct ExtType : ystdex::identity<ystdex::conditional_t<ystdex::integer_width<
	decltype(std::declval<_type>() + 1)>::value == ystdex::integer_width<
	_type>::value, long long, int>>
{};

template<>
struct ExtType<int> : ystdex::identity<long long>
{};

template<>
struct ExtType<long> : ystdex::identity<long long>
{};

template<>
struct ExtType<long long> : ystdex::identity<unsigned long long>
{};

template<>
struct ExtType<unsigned long long> : ystdex::identity<double>
{};

template<typename _type>
struct NExtType : ystdex::cond_t<std::is_signed<_type>, ExtType<_type>,
	ystdex::identity<ystdex::conditional_t<(ystdex::integer_width<_type>::value
	<= ystdex::integer_width<int>::value), int, long long>>>
{};

template<>
struct NExtType<long long> : ystdex::identity<double>
{};


template<typename _type>
struct MulExtType : ystdex::conditional_t<ystdex::integer_width<_type>::value
	== 64, ystdex::identity<double>, ystdex::make_widen_int<_type>>
{};


template<typename _type>
using MakeExtType = ystdex::_t<ExtType<_type>>;

template<typename _type>
using MakeNExtType = ystdex::_t<NExtType<_type>>;

template<typename _type>
using MakeMulExtType = ystdex::_t<MulExtType<_type>>;


template<typename _type, typename _func, class _tValue>
YB_ATTR_nodiscard _type
DoNumLeaf(_tValue& x, _func f)
{
	if(const auto p_i = Unilang::TryAccessValue<int>(x))
		return f(*p_i);
	if(const auto p_u = Unilang::TryAccessValue<unsigned>(x))
		return f(*p_u);
	if(const auto p_ll = Unilang::TryAccessValue<long long>(x))
		return f(*p_ll);
	if(const auto p_ull = Unilang::TryAccessValue<unsigned long long>(x))
		return f(*p_ull);
	if(const auto p_d = Unilang::TryAccessValue<double>(x))
		return f(*p_d);
	if(const auto p_l = Unilang::TryAccessValue<long>(x))
		return f(*p_l);
	if(const auto p_ul = Unilang::TryAccessValue<unsigned long>(x))
		return f(*p_ul);
	if(const auto p_s = Unilang::TryAccessValue<short>(x))
		return f(*p_s);
	if(const auto p_us = Unilang::TryAccessValue<unsigned short>(x))
		return f(*p_us);
	if(const auto p_sc = Unilang::TryAccessValue<signed char>(x))
		return f(*p_sc);
	if(const auto p_uc = Unilang::TryAccessValue<unsigned char>(x))
		return f(*p_uc);
	if(const auto p_f = Unilang::TryAccessValue<float>(x))
		return f(*p_f);
	if(const auto p_ld = Unilang::TryAccessValue<long double>(x))
		return f(*p_ld);
	return f(x);
}

template<typename _type, class... _tValue, typename _func>
YB_ATTR_nodiscard _type
DoNumLeafHinted(NumCode code, _func f, _tValue&... xs)
{
	switch(code)
	{
	case Int:
		return f(xs.template GetObject<int>()...);
	case UInt:
		return f(xs.template GetObject<unsigned>()...);
	case LongLong:
		return f(xs.template GetObject<long long>()...);
	case ULongLong:
		return f(xs.template GetObject<unsigned long long>()...);
	case Double:
		return f(xs.template GetObject<double>()...);
	case Long:
		return f(xs.template GetObject<long>()...);
	case ULong:
		return f(xs.template GetObject<unsigned long>()...);
	case Short:
		return f(xs.template GetObject<short>()...);
	case UShort:
		return f(xs.template GetObject<unsigned short>()...);
	case SChar:
		return f(xs.template GetObject<signed char>()...);
	case UChar:
		return f(xs.template GetObject<unsigned char>()...);
	case Float:
		return f(xs.template GetObject<float>()...);
	case LongDouble:
		return f(xs.template GetObject<long double>()...);
	default:
		return f(xs...);
	}
}


template<typename _type>
YB_ATTR_nodiscard YB_PURE inline ValueObject
QuotientOverflow(const _type& x)
{
	return -MakeExtType<_type>(x);
}


YB_NORETURN void
AssertMismatch()
{
	assert(false && "Invalid number type found for number leaf conversion.");
	YB_ASSUME(false);
}

YB_NORETURN YB_NONNULL(1) void
ThrowTypeErrorForInteger(const char* val)
{
	throw
		TypeError(ystdex::sfmt("Expected a value of type 'integer', got '%s'.",
		Unilang::Nonnull(val)));
}
template<typename _type>
YB_NORETURN yimpl(ystdex::enable_if_t)<std::is_floating_point<_type>::value>
ThrowTypeErrorForInteger(const _type& val)
{
	ThrowTypeErrorForInteger(std::to_string(val).c_str());
}

template<typename _type>
YB_ATTR_nodiscard YB_PURE bool
FloatIsInteger(const _type& x) noexcept
{
#if YB_IMPL_GNUCPP || YB_IMPL_CLANGPP
	YB_Diag_Push
	YB_Diag_Ignore(float-equal)
#endif
	return std::isfinite(x) && std::nearbyint(x) == x;
#if YB_IMPL_GNUCPP || YB_IMPL_CLANGPP
	YB_Diag_Pop
#endif
}

YB_NORETURN ValueObject
ThrowDivisionByZero()
{
	throw std::domain_error("Division by zero.");
}


template<typename _tRet = void>
struct GUAssertMismatch
{
	using result_type = _tRet;

	YB_NORETURN result_type
	operator()(const ValueObject&) const noexcept
	{
		AssertMismatch();
	}
};


template<typename _tRet = ValueObject>
struct GBAssertMismatch
{
	using result_type = _tRet;

	YB_NORETURN result_type
	operator()(const ValueObject&, const ValueObject&) const noexcept
	{
		AssertMismatch();
	}
};


template<typename _type = void>
struct ReportMismatch
{
	YB_NORETURN _type
	operator()(const ValueObject&) const
	{
		ThrowForUnsupportedNumber();
	}
};


struct DynNumCast : ReportMismatch<ValueObject>
{
	NumCode Code;

	DynNumCast(NumCode code) noexcept
		: Code(code)
	{}

	using ReportMismatch<ValueObject>::operator();
	template<typename _type,
		yimpl(typename = ystdex::exclude_self_t<ValueObject, _type>)>
	inline ValueObject
	operator()(const _type& x) const
	{
		switch(Code)
		{
		case Int:
			return static_cast<int>(x);
		case UInt:
			return static_cast<unsigned>(x);
		case LongLong:
			return static_cast<long long>(x);
		case ULongLong:
			return static_cast<unsigned long long>(x);
		case Double:
			return static_cast<double>(x);
		case Long:
			return static_cast<long>(x);
		case ULong:
			return static_cast<unsigned long>(x);
		case Short:
			return static_cast<short>(x);
		case UShort:
			return static_cast<unsigned short>(x);
		case SChar:
			return static_cast<signed char>(x);
		case UChar:
			return static_cast<unsigned char>(x);
		case Float:
			return static_cast<float>(x);
		case LongDouble:
			return static_cast<long double>(x);
		default:
			ThrowForUnsupportedNumber();
		}
	}
};


template<class _tBase, typename _tRet = void>
struct GUOp : GUAssertMismatch<_tRet>, _tBase
{
	using _tBase::_tBase;

	using GUAssertMismatch<_tRet>::operator();
	template<typename _tParam>
	YB_ATTR_nodiscard yconstfn
		yimpl(ystdex::exclude_self_t)<ValueObject, _tParam, _tRet>
	operator()(_tParam&& x) const
		noexcept(noexcept(_tBase::operator()(yforward(x))))
	{
		return _tBase::operator()(yforward(x));
	}
};


template<class _tBase, typename _tRet = ValueObject>
struct GBOp : GBAssertMismatch<_tRet>, _tBase
{
	using _tBase::_tBase;

	using GBAssertMismatch<_tRet>::operator();
	template<typename _tParam1, typename _tParam2,
		yimpl(typename = ystdex::exclude_self_t<ValueObject, _tParam1>)>
	YB_ATTR_nodiscard yconstfn
		yimpl(ystdex::exclude_self_t)<ValueObject, _tParam2, _tRet>
	operator()(_tParam1&& x, _tParam2&& y) const
		noexcept(noexcept(_tBase::operator()(yforward(x), yforward(y))))
	{
		return _tBase::operator()(yforward(x), yforward(y));
	}
};


struct EqZero
{
	template<typename _type>
	YB_ATTR_nodiscard YB_PURE yconstfn bool
	operator()(const _type& x) const noexcept
	{
#if YB_IMPL_GNUCPP || YB_IMPL_CLANGPP
	YB_Diag_Push
	YB_Diag_Ignore(float-equal)
#endif
		return x == _type(0);
#if YB_IMPL_GNUCPP || YB_IMPL_CLANGPP
	YB_Diag_Pop
#endif
	}
};


struct Positive
{
	template<typename _type>
	YB_ATTR_nodiscard YB_PURE yconstfn bool
	operator()(const _type& x) const noexcept
	{
		return x > _type(0);
	}
};


struct Negative
{
	template<typename _type>
	YB_ATTR_nodiscard YB_PURE yconstfn bool
	operator()(const _type& x) const noexcept
	{
		return x < _type(0);
	}
};


struct Odd
{
	template<typename _type>
	YB_ATTR_nodiscard YB_PURE inline
		yimpl(ystdex::enable_if_t)<std::is_floating_point<_type>::value, bool>
	operator()(const _type& x) const
	{
#if YB_IMPL_GNUCPP || YB_IMPL_CLANGPP
	YB_Diag_Push
	YB_Diag_Ignore(float-equal)
#endif
		if(FloatIsInteger(x))
			return std::fmod(x, _type(2)) != _type(0);
#if YB_IMPL_GNUCPP || YB_IMPL_CLANGPP
	YB_Diag_Pop
#endif
		ThrowTypeErrorForInteger(x);
	}
	template<typename _type, yimpl(ystdex::enable_if_t<
		!std::is_floating_point<_type>::value, int> = 0)>
	YB_ATTR_nodiscard YB_PURE inline bool
	operator()(const _type& x) const noexcept
	{
		return x % _type(2) != _type(0);
	}
};


struct Even
{
	template<typename _type>
	YB_ATTR_nodiscard YB_PURE inline
		yimpl(ystdex::enable_if_t)<std::is_floating_point<_type>::value, bool>
	operator()(const _type& x) const
	{
#if YB_IMPL_GNUCPP || YB_IMPL_CLANGPP
	YB_Diag_Push
	YB_Diag_Ignore(float-equal)
#endif
		if(FloatIsInteger(x))
			return std::fmod(x, _type(2)) == _type(0);
#if YB_IMPL_GNUCPP || YB_IMPL_CLANGPP
	YB_Diag_Pop
#endif
		ThrowTypeErrorForInteger(x);
	}
	template<typename _type, yimpl(ystdex::enable_if_t<
		!std::is_floating_point<_type>::value, int> = 0)>
	YB_ATTR_nodiscard YB_PURE inline bool
	operator()(const _type& x) const noexcept
	{
		return x % _type(2) == _type(0);
	}
};


struct BMax
{
	template<typename _type>
	YB_ATTR_nodiscard YB_PURE inline ValueObject
	operator()(const _type& x, const _type& y) const noexcept
	{
		return x > y ? x : y;
	}
};


struct BMin
{
	template<typename _type>
	YB_ATTR_nodiscard YB_PURE inline ValueObject
	operator()(const _type& x, const _type& y) const noexcept
	{
		return x < y ? x : y;
	}
};


struct AddOne
{
	lref<ValueObject> Result;

	AddOne(ValueObject& res) noexcept
		: Result(res)
	{}

	template<typename _type>
	inline yimpl(ystdex::enable_if_t)<std::is_floating_point<_type>::value>
	operator()(_type& x) const noexcept
	{
		x += _type(1);
	}
	template<typename _type, yimpl(ystdex::enable_if_t<
		!std::is_floating_point<_type>::value, int> = 0)>
	inline void
	operator()(_type& x) const
	{
		if(x != std::numeric_limits<_type>::max())
			++x;
		else
			Result.get() = ValueObject(MakeExtType<_type>(x) + 1);
	}
};

struct SubOne
{
	lref<ValueObject> Result;

	SubOne(ValueObject& res) noexcept
		: Result(res)
	{}

	template<typename _type>
	inline yimpl(ystdex::enable_if_t)<std::is_floating_point<_type>::value>
	operator()(_type& x) const noexcept
	{
		x -= _type(1);
	}
	template<typename _type, yimpl(ystdex::enable_if_t<
		!std::is_floating_point<_type>::value, int> = 0)>
	inline void
	operator()(_type& x) const
	{
		if(x != std::numeric_limits<_type>::min())
			--x;
		else
			Result.get() = ValueObject(MakeNExtType<_type>(x) - 1);
	}
};


struct BPlus
{
	template<typename _type>
	YB_ATTR_nodiscard YB_PURE inline
		yimpl(ystdex::enable_if_t)<!std::is_integral<_type>::value, _type>
	operator()(const _type& x, const _type& y) const noexcept
	{
		return x + y;
	}
	template<typename _type, yimpl(ystdex::enable_if_t<ystdex::and_<
		std::is_integral<_type>, std::is_signed<_type>>::value, int> = 0)>
	YB_ATTR_nodiscard YB_PURE inline ValueObject
	operator()(const _type& x, const _type& y) const
	{
#if __has_builtin(__builtin_add_overflow)
		_type r;

		if(!__builtin_add_overflow(x, y, &r))
			return r;
		if(x > 0 && y > std::numeric_limits<_type>::max() - x)
			return MakeExtType<_type>(x) + MakeExtType<_type>(y);
		return MakeNExtType<_type>(x) + MakeNExtType<_type>(y);
#else
		if(x > 0 && y > std::numeric_limits<_type>::max() - x)
			return MakeExtType<_type>(x) + MakeExtType<_type>(y);
		if(x < 0 && y < std::numeric_limits<_type>::min() - x)
			return MakeNExtType<_type>(x) + MakeNExtType<_type>(y);
		return x + y;
#endif
	}
	template<typename _type, yimpl(ystdex::enable_if_t<
		std::is_unsigned<_type>::value, long> = 0L)>
	inline ValueObject
	operator()(const _type& x, const _type& y) const
	{
#if __has_builtin(__builtin_add_overflow)
		_type r;

		if(!__builtin_add_overflow(x, y, &r))
			return r;
#else
		const _type r(x + y);

		if(r >= x)
			return r;
#endif
		return MakeExtType<_type>(x) + MakeExtType<_type>(y);
	}
};


struct BMinus
{
	template<typename _type>
	YB_ATTR_nodiscard YB_PURE inline
		yimpl(ystdex::enable_if_t)<!std::is_integral<_type>::value, _type>
	operator()(const _type& x, const _type& y) const noexcept
	{
		return x - y;
	}
	template<typename _type, yimpl(ystdex::enable_if_t<ystdex::and_<
		std::is_integral<_type>, std::is_signed<_type>>::value, int> = 0)>
	YB_ATTR_nodiscard YB_PURE inline ValueObject
	operator()(const _type& x, const _type& y) const
	{
#if __has_builtin(__builtin_sub_overflow)
		_type r;

		if(!__builtin_sub_overflow(x, y, &r))
			return r;
		if(YB_UNLIKELY(y == std::numeric_limits<_type>::min())
			|| (x > 0 && -y > std::numeric_limits<_type>::max() - x))
			return MakeExtType<_type>(x) - MakeExtType<_type>(y);
		return MakeNExtType<_type>(x) - MakeNExtType<_type>(y);
#else
		if(YB_UNLIKELY(y == std::numeric_limits<_type>::min())
			|| (x > 0 && -y > std::numeric_limits<_type>::max() - x))
			return MakeExtType<_type>(x) - MakeExtType<_type>(y);
		if(x < 0 && -y < std::numeric_limits<_type>::min() - x)
			return MakeNExtType<_type>(x) - MakeNExtType<_type>(y);
		return x - y;
#endif
	}
	template<typename _type, yimpl(ystdex::enable_if_t<
		std::is_unsigned<_type>::value, long> = 0L)>
	YB_ATTR_nodiscard YB_PURE inline ValueObject
	operator()(const _type& x, const _type& y) const
	{
		if(y <= x)
			return x - y;
		return MakeExtType<_type>(x) - MakeExtType<_type>(y);
	}
};


struct BMultiplies
{
	template<typename _type>
	YB_ATTR_nodiscard YB_PURE inline
		yimpl(ystdex::enable_if_t)<!std::is_integral<_type>::value, _type>
	operator()(const _type& x, const _type& y) const noexcept
	{
		return x * y;
	}
	template<typename _type, yimpl(ystdex::enable_if_t<
		std::is_integral<_type>::value, int> = 0)>
	YB_ATTR_nodiscard YB_PURE inline ValueObject
	operator()(const _type& x, const _type& y) const
	{
#if __has_builtin(__builtin_mul_overflow)
		_type r;

		if(!__builtin_mul_overflow(x, y, &r))
			return r;
		return MakeMulExtType<_type>(x) * MakeMulExtType<_type>(y);
#else
		using r_t = MakeMulExtType<_type>;
		const r_t r(r_t(x) * r_t(y));

		if(r <= r_t(std::numeric_limits<_type>::max()))
			return _type(r);
		return r;
#endif
	}
};


struct BDivides
{
	template<typename _type>
	YB_ATTR_nodiscard YB_PURE inline
		yimpl(ystdex::enable_if_t)<!std::is_integral<_type>::value, _type>
	operator()(const _type& x, const _type& y) const noexcept
	{
		return x / y;
	}
	template<typename _type, yimpl(ystdex::enable_if_t<ystdex::and_<
		std::is_integral<_type>, std::is_signed<_type>>::value, int> = 0)>
	YB_ATTR_nodiscard YB_PURE inline ValueObject
	operator()(const _type& x, const _type& y) const
	{
		return y != 0 ? (y != _type(-1) || x != std::numeric_limits<
			_type>::min() ? DoInt(x, y) : QuotientOverflow(x))
			: ThrowDivisionByZero();
	}
	template<typename _type, yimpl(ystdex::enable_if_t<
		std::is_unsigned<_type>::value, long> = 0L)>
	YB_ATTR_nodiscard YB_PURE inline ValueObject
	operator()(const _type& x, const _type& y) const
	{
		return y != 0 ? DoInt(x, y) : ThrowDivisionByZero();
	}

private:
	template<typename _type>
	YB_ATTR_nodiscard YB_PURE static inline ValueObject
	DoInt(const _type& x, const _type& y) noexcept
	{
		static_assert(std::is_integral<_type>(), "Invalid type found.");

		assert(y != 0 && "Invalid value found.");

		const _type r(x / y);

		if(r * y == x)
			return r;
		return double(x) / double(y);
	}
};


struct ReplaceAbs
{
	lref<ValueObject> Result;

	ReplaceAbs(ValueObject& res) noexcept
		: Result(res)
	{}

	template<typename _type>
	inline yimpl(ystdex::enable_if_t)<!std::is_integral<_type>::value>
	operator()(_type& x) const
	{
		using std::abs;

		x = abs(x);
	}
	template<typename _type, yimpl(ystdex::enable_if_t<ystdex::and_<
		std::is_integral<_type>, std::is_signed<_type>>::value, int> = 0)>
	inline void
	operator()(_type& x) const
	{
		if(x != std::numeric_limits<_type>::min())
			x = _type(std::abs(x));
		else
			Result.get() = QuotientOverflow(x);
	}
	template<typename _type, yimpl(ystdex::enable_if_t<
		std::is_unsigned<_type>::value, long> = 0L)>
	inline void
	operator()(_type&) const noexcept
	{}
};


template<class _tOpPolicy>
struct GBDivRemBase
{
	using result_type = typename _tOpPolicy::result_type;

	template<typename _type>
	YB_ATTR_nodiscard YB_PURE inline result_type
	operator()(const _type& x, const _type& y, yimpl(
		ystdex::enable_if_t<std::is_unsigned<_type>::value, int> = 0)) const
	{
		if(y != 0)
			return _tOpPolicy::DoInt(x, y);
		ThrowDivisionByZero();
	}
};


template<class _tPolicy, class _tOpPolicy>
struct GBDivRem : GBDivRemBase<_tOpPolicy>
{
	using typename GBDivRemBase<_tOpPolicy>::result_type;

	template<typename _type>
	YB_ATTR_nodiscard YB_PURE inline yimpl(ystdex::enable_if_t)<
		std::is_floating_point<_type>::value, result_type>
	operator()(const _type& x, const _type& y) const
	{
#if YB_IMPL_GNUCPP || YB_IMPL_CLANGPP
	YB_Diag_Push
	YB_Diag_Ignore(float-equal)
#endif
		if(FloatIsInteger(x))
		{
			if(FloatIsInteger(y))
				return _tOpPolicy::DoFloat(_tPolicy::FloatQuotient(x, y), x, y);
			ThrowTypeErrorForInteger(y);
		}
#if YB_IMPL_GNUCPP || YB_IMPL_CLANGPP
	YB_Diag_Pop
#endif
		ThrowTypeErrorForInteger(x);
	}
	template<typename _type, yimpl(ystdex::enable_if_t<ystdex::and_<
		std::is_integral<_type>, std::is_signed<_type>>::value, int> = 0)>
	YB_ATTR_nodiscard YB_PURE inline result_type
	operator()(const _type& x, const _type& y) const
	{
		return _tPolicy::Int(_tOpPolicy(), x, y);
	}
	using GBDivRemBase<_tOpPolicy>::operator();
};


using DivRemResult = array<ValueObject, 2>;

template<typename _type>
YB_ATTR_nodiscard YB_PURE inline DivRemResult
DividesOverflow(const _type& x)
{
	return {QuotientOverflow(x), _type(0)};
}


struct BDividesPolicy final
{
	using result_type = DivRemResult;

	template<typename _type>
	YB_ATTR_nodiscard YB_PURE static inline result_type
	DoFloat(const _type& q, const _type& x, const _type& y) noexcept
	{
		return {q, x - y * q};
	}

	template<typename _type>
	YB_ATTR_nodiscard YB_PURE static inline result_type
	DoInt(const _type& x, const _type& y) noexcept
	{
		assert(y != 0 && "Invalid divisor found.");
		return {_type(x / y), _type(x % y)};
	}
};


struct BQuotientPolicy final
{
	using result_type = ValueObject;

	template<typename _type>
	YB_ATTR_nodiscard YB_PURE static inline result_type
	DoFloat(const _type& q, const _type&, const _type&) noexcept
	{
		return q;
	}

	template<typename _type>
	YB_ATTR_nodiscard YB_PURE static inline result_type
	DoInt(const _type& x, const _type& y) noexcept
	{
		assert(y != 0 && "Invalid divisor found.");
		return _type(x / y);
	}
};


struct BRemainderPolicy final
{
	using result_type = ValueObject;

	template<typename _type>
	YB_ATTR_nodiscard YB_PURE static inline result_type
	DoFloat(const _type& q, const _type& x, const _type& y) noexcept
	{
		return x - y * q;
	}

	template<typename _type>
	YB_ATTR_nodiscard YB_PURE static inline result_type
	DoInt(const _type& x, const _type& y) noexcept
	{
		assert(y != 0 && "Invalid divisor found.");
		return _type(x % y);
	}
};


struct BFloorPolicy final
{
	template<typename _type>
	YB_ATTR_nodiscard YB_PURE static inline _type
	FloatQuotient(const _type& x, const _type& y) noexcept
	{
		return std::floor(x / y);
	}

	template<typename _type>
	YB_ATTR_nodiscard YB_PURE static inline DivRemResult
	Int(BDividesPolicy, const _type& x, const _type& y) noexcept
	{
		assert(y != 0 && "Invalid divisor found.");
		if(y > 0)
		{
			const _type r(x % y);

			return {_type(x < 0 ? (x + 1) / y - 1 : x / y), r < 0 ? r + y : r};
		}
		if(x != std::numeric_limits<_type>::min())
		{
			const _type r(x % y);

			return {_type((x - 1) / y - 1), r > 0 ? r + y : r};
		}
		if(y != _type(-1))
		{
			const _type r(x % y);

			return {_type((x - y + 1) / y), r > 0 ? r + y : r};
		}
		return DividesOverflow(x);
	}
	template<typename _type>
	YB_ATTR_nodiscard YB_PURE static inline ValueObject
	Int(BQuotientPolicy, const _type& x, const _type& y) noexcept
	{
		assert(y != 0 && "Invalid divisor found.");
		if(y > 0)
			return _type(x < 0 ? (x + 1) / y - 1 : x / y);
		if(x != std::numeric_limits<_type>::min())
			return _type((x - 1) / y - 1);
		if(y != _type(-1))
			return _type((x - y + 1) / y);
		return QuotientOverflow(x);
	}
	template<typename _type>
	YB_ATTR_nodiscard YB_PURE static inline _type
	Int(BRemainderPolicy, const _type& x, const _type& y) noexcept
	{
		assert(y != 0 && "Invalid divisor found.");

		const _type r(x % y);

		return (y > 0 ? r < 0 : r > 0) ? r + y : r;
	}
};


struct BTruncatePolicy final
{
	template<typename _type>
	YB_ATTR_nodiscard YB_PURE static inline _type
	FloatQuotient(const _type& x, const _type& y) noexcept
	{
		return std::trunc(x / y);
	}

	template<typename _type>
	YB_ATTR_nodiscard YB_PURE static inline DivRemResult
	Int(BDividesPolicy, const _type& x, const _type& y) noexcept
	{
		return y != _type(-1) || x != std::numeric_limits<_type>::min()
			? DivRemResult{_type(x / y), _type(x % y)} : DividesOverflow(x);
	}
	template<typename _type>
	YB_ATTR_nodiscard YB_PURE static inline ValueObject
	Int(BQuotientPolicy, const _type& x, const _type& y) noexcept
	{
		assert(y != 0 && "Invalid divisor found.");
		return y != _type(-1) || x != std::numeric_limits<_type>::min()
			? _type(x / y) : QuotientOverflow(x);
	}
	template<typename _type>
	YB_ATTR_nodiscard YB_PURE static inline _type
	Int(BRemainderPolicy, const _type& x, const _type& y) noexcept
	{
		assert(y != 0 && "Invalid divisor found.");
		return x % y;
	}
};


YB_ATTR_nodiscard YB_PURE ValueObject
Promote(NumCode code, const ValueObject& x, NumCode src_code)
{
	return DoNumLeafHinted<ValueObject>(src_code, DynNumCast(code), x);
}

template<class _tBase>
YB_ATTR_nodiscard bool
NumUnaryPredicate(const ValueObject& x) noexcept
{
	return DoNumLeaf<bool>(x, GUOp<_tBase, bool>());
}

template<class _fUnary, typename _tRet = void>
YB_ATTR_nodiscard ValueObject
NumUnaryOp(ResolvedArg<>& x)
{
	auto res(MoveUnary(x));

	DoNumLeaf<void>(res, GUOp<_fUnary, _tRet>(res));
	return res;
}

template<class _fBinary>
bool
NumBinaryComp(const ValueObject& x, const ValueObject& y) noexcept
{
	const auto xcode(MapTypeIdToNumCode(x));
	const auto ycode(MapTypeIdToNumCode(y));
	const auto ret_bin([](ValueObject u, ValueObject v, NumCode code){
		return DoNumLeafHinted<bool>(code, GBOp<_fBinary, bool>(), u, v);
	});

	return size_t(xcode) >= size_t(ycode) ? ret_bin(x, Promote(xcode, y, ycode),
		xcode) : ret_bin(Promote(ycode, x, xcode), y, ycode);
}

template<class _fBinary, typename _tRet = ValueObject>
YB_ATTR_nodiscard _tRet
NumBinaryOp(ResolvedArg<>& x, ResolvedArg<>& y)
{
	const auto xcode(MapTypeIdToNumCode(x));
	const auto ycode(MapTypeIdToNumCode(y));
	const auto ret_bin([](ValueObject u, ValueObject v, NumCode code){
		return DoNumLeafHinted<_tRet>(code, GBOp<_fBinary, _tRet>(), u, v);
	});

	return size_t(xcode) >= size_t(ycode) ?
		ret_bin(MoveUnary(x), Promote(xcode, y.get().Value, ycode), xcode)
		: ret_bin(Promote(ycode, x.get().Value, xcode), MoveUnary(y), ycode);
}


YB_NORETURN YB_NONNULL(1, 2) void
ThrowForInvalidLiteralSuffix(const char* sfx, const char* id)
{
	throw InvalidSyntax(ystdex::sfmt(
		"Literal suffix '%s' is unsupported in identifier '%s'.", sfx, id));
}

template<typename _type>
YB_ATTR_nodiscard YB_STATELESS yconstfn _type
DecimalCarryAddDigit(_type x, char c)
{
	return x * 10 + _type(c - '0');
}

template<typename _tInt>
YB_ATTR_nodiscard YB_FLATTEN inline bool
DecimalAccumulate(_tInt& ans, char c)
{
	static yconstexpr const auto i_thr(std::numeric_limits<_tInt>::max() / 10);
	static yconstexpr const char
		c_thr(char('0' + size_t(std::numeric_limits<_tInt>::max() % 10)));

	if(ans < i_thr || YB_UNLIKELY(ans == i_thr && c <= c_thr))
	{
		ans = DecimalCarryAddDigit(ans, c);
		return true;
	}
	return {};
}

using ReadIntType = int;
using ReadExtIntType = long long;
using ReadCommonType = ystdex::common_type_t<ReadExtIntType, std::uint64_t>;

YB_ATTR_nodiscard YB_STATELESS yconstfn bool
IsDecimalPoint(char c) noexcept
{
	return c == '.';
}

YB_ATTR_nodiscard YB_STATELESS yconstfn
bool IsExponent(char c) noexcept
{
	return c == 'e' || c == 'E';
}

template<typename _type>
YB_ATTR_nodiscard YB_PURE inline _type
ScaleDecimalFlonum(char sign, _type ans, ptrdiff_t scale)
{
	return std::ldexp(sign != '-' ? _type(ans) : -_type(ans), int(scale))
		* std::pow(_type(5), _type(scale));
}

void
SetDecimalFlonum(ValueObject& vo, char e, char sign, ReadCommonType ans,
	ptrdiff_t scale)
{
	switch(e)
	{
	case 'e':
	case 'E':
		vo = ScaleDecimalFlonum(sign, double(ans), scale);
		break;
	}
}

YB_ATTR_nodiscard YB_PURE ptrdiff_t
ReadDecimalExponent(string_view::const_iterator first, string_view id)
{
	bool minus = {};

	switch(*first)
	{
	case '-':
		minus = true;
		YB_ATTR_fallthrough;
	case '+':
		++first;
		YB_ATTR_fallthrough;
	default:
		break;
	}
	if(first != id.end())
	{
		ptrdiff_t res(0);

		ystdex::retry_on_cond(ystdex::id<>(), [&]() -> bool{
			if(ystdex::isdigit(*first))
			{
				if(DecimalAccumulate(res, *first))
					return ++first != id.end();
				res = std::numeric_limits<ptrdiff_t>::max();
				return {};
			}
			throw InvalidSyntax(ystdex::sfmt("Invalid character '%c' found in"
				" the exponent of '%s'.", *first, id.data()));
		});
		return minus ? -res : res;
	}
	throw
		InvalidSyntax(ystdex::sfmt("Empty exponent found in '%s'.", id.data()));
}

void
ReadDecimalInexact(ValueObject& vo, string_view::const_iterator first,
	string_view id, ReadCommonType ans, string_view::const_iterator dpos)
{
	assert(!id.empty() && "Invalid lexeme found.");

	auto cut(id.end());
	ptrdiff_t scale;
	char e('e');

	ystdex::retry_on_cond(ystdex::id<>(), [&]() -> bool{
		if(ystdex::isdigit(*first))
		{
			if(cut == id.end())
			{
				if(!DecimalAccumulate(ans, *first))
				{
					ans += *first >= '5' ? 1 : 0;
					cut = first;
				}
			}
		}
		else if(IsDecimalPoint(*first))
		{
			if(dpos == id.end())
				dpos = first;
			else
				throw InvalidSyntax(ystdex::sfmt(
					"More than one decimal points found in '%s'.", id.data()));
		}
		else if(IsExponent(*first))
		{
			e = *first;
			scale = (cut == id.end() ? 0 : first - cut
				- (dpos != id.end() && dpos > cut ? 1 : 0))
				- (dpos == id.end() ? 0 : first - dpos - 1)
				+ ReadDecimalExponent(first + 1, id);
			return {};
		}
		else
			ThrowForInvalidLiteralSuffix(&*first, id.data());
		if(++first != id.end())
			return true;
		scale = (cut == id.end() ? 0 : id.end() - cut
			- (dpos != id.end() && dpos > cut ? 1 : 0))
			- (dpos == id.end() ? 0 : id.end() - dpos - 1);
		return {};
	});
	SetDecimalFlonum(vo, e, id[0], ans, scale);
}

template<typename _tInt>
YB_ATTR_nodiscard bool
ReadDecimalExact(ValueObject& vo, string_view id,
	string_view::const_iterator& first, _tInt& ans)
{
	assert(!id.empty() &&  "Invalid lexeme found.");
	return ystdex::retry_on_cond([&](ReductionStatus r) noexcept -> bool{
		if(r == ReductionStatus::Retrying)
		{
			if(++first != id.end())
				return true;
			vo = id[0] != '-' ? ans : -ans;
		}
		return {};
	}, [&]() -> ReductionStatus{
		if(ystdex::isdigit(*first))
			return DecimalAccumulate(ans, *first) ? ReductionStatus::Retrying
				: ReductionStatus::Partial;
		if(IsDecimalPoint(*first))
		{
			ReadDecimalInexact(vo, first + 1, id, ReadCommonType(ans),
				first);
			return ReductionStatus::Clean;
		}
		if(IsExponent(*first))
		{
			SetDecimalFlonum(vo, *first, id[0], ReadCommonType(ans),
				ReadDecimalExponent(first + 1, id));
			return ReductionStatus::Clean;
		}
		ThrowForInvalidLiteralSuffix(&*first, id.data());
	}) == ReductionStatus::Partial;
}

} // unnamed namespace;

bool
IsExactValue(const ValueObject& vo) noexcept
{
	return IsTyped<int>(vo) || IsTyped<unsigned>(vo) || IsTyped<long long>(vo)
		|| IsTyped<unsigned long long>(vo) || IsTyped<long>(vo)
		|| IsTyped<unsigned long>(vo) || IsTyped<short>(vo)
		|| IsTyped<unsigned short>(vo) || IsTyped<signed char>(vo)
		|| IsTyped<unsigned char>(vo);
}

bool
IsInexactValue(const ValueObject& vo) noexcept
{
	return
		IsTyped<double>(vo) || IsTyped<float>(vo) || IsTyped<long double>(vo);
}

bool
IsRationalValue(const ValueObject& vo) noexcept
{
	if(const auto p_d = Unilang::TryAccessValue<double>(vo))
		return std::isfinite(*p_d);
	if(const auto p_f = Unilang::TryAccessValue<float>(vo))
		return std::isfinite(*p_f);
	if(const auto p_ld = Unilang::TryAccessValue<long double>(vo))
		return std::isfinite(*p_ld);
	return IsExactValue(vo);
}

bool
IsIntegerValue(const ValueObject& vo) noexcept
{
	if(const auto p_d = Unilang::TryAccessValue<double>(vo))
		return FloatIsInteger(*p_d);
	if(const auto p_f = Unilang::TryAccessValue<float>(vo))
		return FloatIsInteger(*p_f);
	if(const auto p_ld = Unilang::TryAccessValue<long double>(vo))
		return FloatIsInteger(*p_ld);
	return IsExactValue(vo);
}


bool
IsFinite(const ValueObject& x) noexcept
{
	if(const auto p_d = Unilang::TryAccessValue<double>(x))
		return std::isfinite(*p_d);
	if(const auto p_f = Unilang::TryAccessValue<float>(x))
		return std::isfinite(*p_f);
	if(const auto p_ld = Unilang::TryAccessValue<long double>(x))
		return std::isfinite(*p_ld);
	return true;
}

bool
IsInfinite(const ValueObject& x) noexcept
{
	if(const auto p_d = Unilang::TryAccessValue<double>(x))
		return std::isinf(*p_d);
	if(const auto p_f = Unilang::TryAccessValue<float>(x))
		return std::isinf(*p_f);
	if(const auto p_ld = Unilang::TryAccessValue<long double>(x))
		return std::isinf(*p_ld);
	return {};
}

bool
IsNaN(const ValueObject& x) noexcept
{
	if(const auto p_d = Unilang::TryAccessValue<double>(x))
		return std::isnan(*p_d);
	if(const auto p_f = Unilang::TryAccessValue<float>(x))
		return std::isnan(*p_f);
	if(const auto p_ld = Unilang::TryAccessValue<long double>(x))
		return std::isnan(*p_ld);
	return {};
}


bool
Equal(const ValueObject& x, const ValueObject& y) noexcept
{
	return NumBinaryComp<ystdex::equal_to<>>(x, y);
}

bool
Less(const ValueObject& x, const ValueObject& y) noexcept
{
	return NumBinaryComp<ystdex::less<>>(x, y);
}

bool
Greater(const ValueObject& x, const ValueObject& y) noexcept
{
	return NumBinaryComp<ystdex::greater<>>(x, y);
}

bool
LessEqual(const ValueObject& x, const ValueObject& y) noexcept
{
	return NumBinaryComp<ystdex::less_equal<>>(x, y);
}

bool
GreaterEqual(const ValueObject& x, const ValueObject& y) noexcept
{
	return NumBinaryComp<ystdex::greater_equal<>>(x, y);
}


bool
IsZero(const ValueObject& x) noexcept
{
	return NumUnaryPredicate<EqZero>(x);
}

bool
IsPositive(const ValueObject& x) noexcept
{
	return NumUnaryPredicate<Positive>(x);
}

bool
IsNegative(const ValueObject& x) noexcept
{
	return NumUnaryPredicate<Negative>(x);
}

bool
IsOdd(const ValueObject& x) noexcept
{
	return NumUnaryPredicate<Odd>(x);
}

bool
IsEven(const ValueObject& x) noexcept
{
	return NumUnaryPredicate<Even>(x);
}


ValueObject
Max(ResolvedArg<>&& x, ResolvedArg<>&& y)
{
	return NumBinaryOp<BMax>(x, y);
}

ValueObject
Min(ResolvedArg<>&& x, ResolvedArg<>&& y)
{
	return NumBinaryOp<BMin>(x, y);
}

ValueObject
Add1(ResolvedArg<>&& x)
{
	return NumUnaryOp<AddOne>(x);
}

ValueObject
Sub1(ResolvedArg<>&& x)
{
	return NumUnaryOp<SubOne>(x);
}

ValueObject
Plus(ResolvedArg<>&& x, ResolvedArg<>&& y)
{
	return NumBinaryOp<BPlus>(x, y);
}

ValueObject
Minus(ResolvedArg<>&& x, ResolvedArg<>&& y)
{
	return NumBinaryOp<BMinus>(x, y);
}

ValueObject
Multiplies(ResolvedArg<>&& x, ResolvedArg<>&& y)
{
	return NumBinaryOp<BMultiplies>(x, y);
}

ValueObject
Divides(ResolvedArg<>&& x, ResolvedArg<>&& y)
{
	return NumBinaryOp<BDivides>(x, y);
}

ValueObject
Abs(ResolvedArg<>&& x)
{
	return NumUnaryOp<ReplaceAbs>(x);
}

array<ValueObject, 2>
FloorDivides(ResolvedArg<>&& x, ResolvedArg<>&& y)
{
	return NumBinaryOp<GBDivRem<BFloorPolicy, BDividesPolicy>,
		BDividesPolicy::result_type>(x, y);
}

ValueObject
FloorQuotient(ResolvedArg<>&& x, ResolvedArg<>&& y)
{
	return NumBinaryOp<GBDivRem<BFloorPolicy, BQuotientPolicy>>(x, y);
}

ValueObject
FloorRemainder(ResolvedArg<>&& x, ResolvedArg<>&& y)
{
	return NumBinaryOp<GBDivRem<BFloorPolicy, BRemainderPolicy>>(x, y);
}

array<ValueObject, 2>
TruncateDivides(ResolvedArg<>&& x, ResolvedArg<>&& y)
{
	return NumBinaryOp<GBDivRem<BTruncatePolicy, BDividesPolicy>,
		BDividesPolicy::result_type>(x, y);
}


void
ReadDecimal(ValueObject& vo, string_view id, string_view::const_iterator first)
{
	assert(!id.empty() && "Invalid lexeme found.");
	assert(first >= id.begin() && size_t(first - id.begin()) < id.size()
		&& "Invalid first iterator found.");

	while(YB_UNLIKELY(*first == '0'))
		if(first + 1 != id.end())
			++first;
		else
			break;

	ReadIntType ans(0);

	if(YB_UNLIKELY(ReadDecimalExact(vo, id, first, ans)))
	{
		ReadExtIntType lans(ans);

		if(YB_UNLIKELY(ReadDecimalExact(vo, id, first, lans)))
			ReadDecimalInexact(vo, first, id, ReadCommonType(lans),
				id.end());
	}
}

string
FPToString(float x)
{
	switch(std::fpclassify(x))
	{
	default:
		return sfmt<string>(FloatIsInteger(x) ? "%.1f" : "%.6g", double(x));
	case FP_INFINITE:
		return std::signbit(x) ? "-inf.0" : "+inf.0";
	case FP_NAN:
		return std::signbit(x) ? "-nan.0" : "+nan.0";
	}
}
string
FPToString(double x)
{
	switch(std::fpclassify(x))
	{
	default:
		return sfmt<string>(FloatIsInteger(x) ? "%.1f" : "%.14g", x);
	case FP_INFINITE:
		return std::signbit(x) ? "-inf.0" : "+inf.0";
	case FP_NAN:
		return std::signbit(x) ? "-nan.0" : "+nan.0";
	}
}
string
FPToString(long double x)
{
	switch(std::fpclassify(x))
	{
	default:
		return sfmt<string>(FloatIsInteger(x) ? "%.1Lf" : "%.18Lg", x);
	case FP_INFINITE:
		return std::signbit(x) ? "-inf.0" : "+inf.0";
	case FP_NAN:
		return std::signbit(x) ? "-nan.0" : "+nan.0";
	}
}

} // inline namespace Math;

} // namespace Unilang;

