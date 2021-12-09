// © 2021 Uniontech Software Technology Co.,Ltd.

#include "Math.h" // for size_t, type_info, Unilang::TryAccessValue;
#include <ystdex/exception.h> // for ystdex::unsupported;
#include <ystdex/meta.hpp> // for ystdex::exclude_self_t, ystdex::enable_if_t,
//	std::is_floating_point;
#include <cmath> // for std::isfinite, std::nearbyint, std::isinf, std::isnan,
//	std::fmod;
#include <string> // for std::to_string;

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

YB_NORETURN void
ThrowTypeErrorForInteger(const std::string& val)
{
	throw TypeError(ystdex::sfmt(
		"Expected a value of type 'integer', got '%s'.", val.c_str()));
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


template<typename _type = void>
struct GUAssertMismatch
{
	YB_NORETURN _type
	operator()(const ValueObject&) const noexcept
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


struct EqZero : GUAssertMismatch<bool>
{
	using GUAssertMismatch<bool>::operator();
	template<typename _type,
		yimpl(typename = ystdex::exclude_self_t<ValueObject, _type>)>
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


struct Positive : GUAssertMismatch<bool>
{
	using GUAssertMismatch<bool>::operator();
	template<typename _type,
		yimpl(typename = ystdex::exclude_self_t<ValueObject, _type>)>
	YB_ATTR_nodiscard YB_PURE yconstfn bool
	operator()(const _type& x) const noexcept
	{
		return x > _type(0);
	}
};


struct Negative : GUAssertMismatch<bool>
{
	using GUAssertMismatch<bool>::operator();
	template<typename _type,
		yimpl(typename = ystdex::exclude_self_t<ValueObject, _type>)>
	YB_ATTR_nodiscard YB_PURE yconstfn bool
	operator()(const _type& x) const noexcept
	{
		return x < _type(0);
	}
};


struct Odd : GUAssertMismatch<bool>
{
	using GUAssertMismatch<bool>::operator();
	template<typename _type>
	inline yimpl(ystdex::exclude_self_t<ValueObject, _type, bool>)
	operator()(const _type& x) const
	{
		return Do(x);
	}

private:
	template<typename _type>
	static inline
		ystdex::enable_if_t<std::is_floating_point<_type>::value, bool>
	Do(const _type& x)
	{
#if YB_IMPL_GNUCPP || YB_IMPL_CLANGPP
	YB_Diag_Push
	YB_Diag_Ignore(float-equal)
#endif
		if(FloatIsInteger(x))
			return std::fmod(x, _type(2)) == _type(1);
#if YB_IMPL_GNUCPP || YB_IMPL_CLANGPP
	YB_Diag_Pop
#endif
		ThrowTypeErrorForInteger(std::to_string(x));
	}
	template<typename _type, yimpl(ystdex::enable_if_t<
		!std::is_floating_point<_type>::value, int> = 0)>
	static inline bool
	Do(const _type& x) noexcept
	{
		return x % _type(2) != _type(0);
	}
};


struct Even : GUAssertMismatch<bool>
{
	using GUAssertMismatch<bool>::operator();
	template<typename _type>
	inline yimpl(ystdex::exclude_self_t<ValueObject, _type, bool>)
	operator()(const _type& x) const
	{
		return Do(x);
	}

private:
	template<typename _type>
	static inline
		ystdex::enable_if_t<std::is_floating_point<_type>::value, bool>
	Do(const _type& x)
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
		ThrowTypeErrorForInteger(std::to_string(x));
	}
	template<typename _type, yimpl(ystdex::enable_if_t<
		!std::is_floating_point<_type>::value, int> = 0)>
	static inline bool
	Do(const _type& x) noexcept
	{
		return x % _type(2) == _type(0);
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


struct BMax : GBAssertMismatch<>
{
	using GBAssertMismatch<>::operator();
	template<typename _type,
		yimpl(typename = ystdex::exclude_self_t<ValueObject, _type>)>
	inline ValueObject
	operator()(const _type& x, const _type& y) const noexcept
	{
		return x > y ? x : y;
	}
};


struct BMin : GBAssertMismatch<>
{
	using GBAssertMismatch<>::operator();
	template<typename _type,
		yimpl(typename = ystdex::exclude_self_t<ValueObject, _type>)>
	inline ValueObject
	operator()(const _type& x, const _type& y) const noexcept
	{
		return x < y ? x : y;
	}
};


struct AddOne : GUAssertMismatch<>
{
	ValueObject& Result;

	AddOne(ValueObject& res)
		: Result(res)
	{}

	using GUAssertMismatch<>::operator();
	template<typename _type>
	inline yimpl(ystdex::exclude_self_t<ValueObject, _type>)
	operator()(_type& x) const noexcept
	{
		Perform(x);
	}

private:
	template<typename _type>
	inline ystdex::enable_if_t<std::is_floating_point<_type>::value>
	Perform(_type& x) const noexcept
	{
		x += _type(1);
	}
	template<typename _type, yimpl(ystdex::enable_if_t<
		!std::is_floating_point<_type>::value, int> = 0)>
	inline void
	Perform(_type& x) const
	{
		if(x != std::numeric_limits<_type>::max())
			++x;
		else
			Result = ValueObject(MakeExtType<_type>(x) + 1);
	}
};

struct SubOne : GUAssertMismatch<>
{
	ValueObject& Result;

	SubOne(ValueObject& res)
		: Result(res)
	{}

	using GUAssertMismatch<>::operator();
	template<typename _type>
	inline yimpl(ystdex::exclude_self_t<ValueObject, _type>)
	operator()(_type& x) const noexcept
	{
		Perform(x);
	}

private:
	template<typename _type>
	inline ystdex::enable_if_t<std::is_floating_point<_type>::value>
	Perform(_type& x) const noexcept
	{
		x -= _type(1);
	}
	template<typename _type, yimpl(ystdex::enable_if_t<
		!std::is_floating_point<_type>::value, int> = 0)>
	inline void
	Perform(_type& x) const
	{
		if(x != std::numeric_limits<_type>::min())
			--x;
		else
			Result = ValueObject(MakeNExtType<_type>(x) - 1);
	}
};


struct BPlus : GBAssertMismatch<>
{
	using GBAssertMismatch<>::operator();
	template<typename _type,
		yimpl(typename = ystdex::exclude_self_t<ValueObject, _type>)>
	inline ValueObject
	operator()(const _type& x, const _type& y) const noexcept
	{
		return Do(x, y);
	}

private:
	template<typename _type>
	static inline
		ystdex::enable_if_t<!std::is_integral<_type>::value, _type>
	Do(const _type& x, const _type& y) noexcept
	{
		return x + y;
	}
	template<typename _type, yimpl(ystdex::enable_if_t<
		ystdex::and_<std::is_integral<_type>,
		std::is_signed<_type>>::value, int> = 0)>
	static inline ValueObject
	Do(const _type& x, const _type& y)
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
	static inline ValueObject
	Do(const _type& x, const _type& y)
	{
#if __has_builtin(__builtin_add_overflow)
		_type r;

		if(!__builtin_add_overflow(x, y, &r))
			return r;
#else
		const _type r(x + y);

		if(r >= x)
			return r;urn x + y;
#endif
		return MakeExtType<_type>(x) + MakeExtType<_type>(y);
	}
};


struct BMinus : GBAssertMismatch<>
{
	using GBAssertMismatch<>::operator();
	template<typename _type,
		yimpl(typename = ystdex::exclude_self_t<ValueObject, _type>)>
	inline ValueObject
	operator()(const _type& x, const _type& y) const noexcept
	{
		return Do(x, y);
	}

private:
	template<typename _type>
	static inline
		ystdex::enable_if_t<!std::is_integral<_type>::value, _type>
	Do(const _type& x, const _type& y) noexcept
	{
		return x - y;
	}
	template<typename _type, yimpl(ystdex::enable_if_t<
		ystdex::and_<std::is_integral<_type>,
		std::is_signed<_type>>::value, int> = 0)>
	static inline ValueObject
	Do(const _type& x, const _type& y)
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
	static inline ValueObject
	Do(const _type& x, const _type& y)
	{
		if(y <= x)
			return x - y;
		return MakeExtType<_type>(x) - MakeExtType<_type>(y);
	}
};


struct BMultiplies : GBAssertMismatch<>
{
	using GBAssertMismatch<>::operator();
	template<typename _type,
		yimpl(typename = ystdex::exclude_self_t<ValueObject, _type>)>
	inline ValueObject
	operator()(const _type& x, const _type& y) const noexcept
	{
		return Do(x, y);
	}

private:
	template<typename _type>
	static inline
		ystdex::enable_if_t<!std::is_integral<_type>::value, _type>
	Do(const _type& x, const _type& y) noexcept
	{
		return x * y;
	}
	template<typename _type, yimpl(ystdex::enable_if_t<
		std::is_integral<_type>::value, int> = 0)>
	static inline ValueObject
	Do(const _type& x, const _type& y)
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


struct BDivides : GBAssertMismatch<>
{
	using GBAssertMismatch<>::operator();
	template<typename _type,
		yimpl(typename = ystdex::exclude_self_t<ValueObject, _type>)>
	inline ValueObject
	operator()(const _type& x, const _type& y) const
	{
		return Do(x, y);
	}

private:
	template<typename _type>
	static inline
		ystdex::enable_if_t<!std::is_integral<_type>::value, _type>
	Do(const _type& x, const _type& y) noexcept
	{
		return x / y;
	}
	template<typename _type, yimpl(ystdex::enable_if_t<
		ystdex::and_<std::is_integral<_type>,
		std::is_signed<_type>>::value, int> = 0)>
	static inline ValueObject
	Do(const _type& x, const _type& y)
	{
		if(y != 0)
		{
			if(y != _type(-1) || x != std::numeric_limits<_type>::min())
				return DoInt(x, y);
			if(std::numeric_limits<_type>::min()
				== std::numeric_limits<long long>::min())
				return -double(x);
			return -MakeExtType<_type>(x);
		}
		ThrowDivideByZero();
	}
	template<typename _type, yimpl(ystdex::enable_if_t<
		std::is_unsigned<_type>::value, long> = 0L)>
	static inline ValueObject
	Do(const _type& x, const _type& y)
	{
		if(y != 0)
			return DoInt(x, y);
		ThrowDivideByZero();
	}

	template<typename _type>
	static inline ValueObject
	DoInt(const _type& x, const _type& y) noexcept
	{
		static_assert(std::is_integral<_type>(), "Invalid type found.");
		YAssert(y != 0, "Invalid value found.");
		_type r(x / y);

		if(r * y == x)
			return r;
		return double(x) / double(y);
	}

	YB_NORETURN static ValueObject
	ThrowDivideByZero()
	{
		throw std::domain_error("Division by zero.");
	}
};


struct ReplaceAbs : GUAssertMismatch<>
{
	ValueObject& Result;

	ReplaceAbs(ValueObject& res)
		: Result(res)
	{}

	using GUAssertMismatch<>::operator();
	template<typename _type>
	inline yimpl(ystdex::exclude_self_t<ValueObject, _type>)
	operator()(_type& x) const
	{
		Perform(x);
	}

private:
	template<typename _type>
	inline ystdex::enable_if_t<!std::is_integral<_type>::value>
	Perform(_type& x) const
	{
		using std::abs;

		x = abs(x);
	}
	template<typename _type, yimpl(ystdex::enable_if_t<
		ystdex::and_<std::is_integral<_type>,
		std::is_signed<_type>>::value, int> = 0)>
	inline void
	Perform(_type& x) const
	{
		if(x != std::numeric_limits<_type>::min())
			x = _type(std::abs(x));
		else
			Result = -MakeExtType<_type>(x);
	}
	template<typename _type, yimpl(ystdex::enable_if_t<
		std::is_unsigned<_type>::value, long> = 0L)>
	inline void
	Perform(_type&) const noexcept
	{}
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

template<class _fBinary>
ValueObject
NumBinaryOp(ResolvedArg<>& x, ResolvedArg<>& y)
{
	const auto xcode(MapTypeIdToNumCode(x));
	const auto ycode(MapTypeIdToNumCode(y));
	const auto ret_bin([](ValueObject u, ValueObject v, NumCode code){
		return DoNumLeafHinted<ValueObject>(code, _fBinary(), u, v);
	});

	return size_t(xcode) >= size_t(ycode) ?
		ret_bin(MoveUnary(x), Promote(xcode, y.get().Value, ycode), xcode)
		: ret_bin(Promote(ycode, x.get().Value, xcode), MoveUnary(y), ycode);
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


bool
IsZero(const ValueObject& x) noexcept
{
	return DoNumLeaf<bool>(x, EqZero());
}

bool
IsPositive(const ValueObject& x) noexcept
{
	return DoNumLeaf<bool>(x, Positive());
}

bool
IsNegative(const ValueObject& x) noexcept
{
	return DoNumLeaf<bool>(x, Negative());
}

bool
IsOdd(const ValueObject& x) noexcept
{
	return DoNumLeaf<bool>(x, Odd());
}

bool
IsEven(const ValueObject& x) noexcept
{
	return DoNumLeaf<bool>(x, Even());
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
	auto res(MoveUnary(x));

	DoNumLeaf<void>(res, AddOne(res));
	return res;
}

ValueObject
Sub1(ResolvedArg<>&& x)
{
	auto res(MoveUnary(x));

	DoNumLeaf<void>(res, SubOne(res));
	return res;
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
	auto res(MoveUnary(x));

	DoNumLeaf<void>(res, ReplaceAbs(res));
	return res;
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

	int ans(0);

	for(auto p(first); p != id.end(); ++p)
		if(ystdex::isdigit(*p))
		{
			if(unsigned((ans << 3) + (ans << 1) + *p - '0')
				<= unsigned(INT_MAX))
				ans = (ans << 3) + (ans << 1) + *p - '0';
			else
				throw InvalidSyntax(ystdex::sfmt<std::string>(
					"Value of identifier '%s' is out of the range of"
					" the supported integer.", id.data()));
		}
		else
			throw InvalidSyntax(ystdex::sfmt<std::string>("Literal"
				" postfix is unsupported in identifier '%s'.",
				id.data()));
	vo = id[0] != '-' ? ans : -ans;
}

} // inline namespace Math;

} // namespace Unilang;

