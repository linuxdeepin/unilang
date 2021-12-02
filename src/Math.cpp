// © 2021 Uniontech Software Technology Co.,Ltd.

#include "Math.h" // for size_t, type_info, Unilang::TryAccessValue;
#include <ystdex/exception.h> // for ystdex::unsupported;
#include <ystdex/meta.hpp> // for ystdex::exclude_self_t;
#include <cmath> // for std::isfinite, std::isinf, std::isnan;

namespace Unilang
{

inline namespace Math
{

namespace
{

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
	// TODO: Use bigint.
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

template<typename _type, typename _func, class... _tValue>
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


YB_NORETURN void
AssertMismatch()
{
	assert(false && "Invalid number type found for number leaf conversion.");
	YB_ASSUME(false);
}

template<typename _type>
YB_ATTR_nodiscard YB_PURE bool
FloatIsInteger(const _type& x) noexcept
{
#if __GNUC__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
	return std::isfinite(x) && std::nearbyint(x) == x;
#if __GNUC__
#	pragma GCC diagnostic pop
#endif
}


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


template<typename _type = ValueObject>
struct GBAssertMismatch
{
	YB_NORETURN _type
	operator()(const ValueObject&, const ValueObject&) const noexcept
	{
		AssertMismatch();
	}
};


template<class _tBase>
struct GBCompare : GBAssertMismatch<bool>, _tBase
{
	using GBAssertMismatch<bool>::operator();
	template<typename _type,
		yimpl(typename = ystdex::exclude_self_t<ValueObject, _type>)>
	inline bool
	operator()(const _type& x, const _type& y) const noexcept
	{
		return _tBase::operator()(x, y);
	}
};


YB_ATTR_nodiscard YB_PURE ValueObject
Promote(NumCode code, const ValueObject& x, NumCode src_code)
{
	return DoNumLeafHinted<ValueObject>(src_code, DynNumCast(code), x);
}

template<class _fBinary>
bool
NumBinaryComp(const ValueObject& x, const ValueObject& y) noexcept
{
	const auto xcode(MapTypeIdToNumCode(x));
	const auto ycode(MapTypeIdToNumCode(y));
	const auto ret_bin([](ValueObject u, ValueObject v, NumCode code){
		return DoNumLeafHinted<bool>(code, _fBinary(), u, v);
	});

	return size_t(xcode) >= size_t(ycode) ? ret_bin(x, Promote(xcode, y, ycode),
		xcode) : ret_bin(Promote(ycode, x, xcode), y, ycode);
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
	return NumBinaryComp<GBCompare<ystdex::equal_to<>>>(x, y);
}

bool
Less(const ValueObject& x, const ValueObject& y) noexcept
{
	return NumBinaryComp<GBCompare<ystdex::less<>>>(x, y);
}

bool
Greater(const ValueObject& x, const ValueObject& y) noexcept
{
	return NumBinaryComp<GBCompare<ystdex::greater<>>>(x, y);
}

bool
LessEqual(const ValueObject& x, const ValueObject& y) noexcept
{
	return NumBinaryComp<GBCompare<ystdex::less_equal<>>>(x, y);
}

bool
GreaterEqual(const ValueObject& x, const ValueObject& y) noexcept
{
	return NumBinaryComp<GBCompare<ystdex::greater_equal<>>>(x, y);
}

} // inline namespace Math;

} // namespace Unilang;

