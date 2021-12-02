// © 2021 Uniontech Software Technology Co.,Ltd.

#include "Math.h" // for Unilang::TryAccessValue;
#include <cmath> // for std::isfinite;

namespace Unilang
{

inline namespace Math
{

namespace
{

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


Number::Number(int x)
	: value(x)
{}

Number&
Number::operator+=(const Number& n)
{
	value += n.value;
	return *this;
}

Number&
Number::operator-=(const Number& n)
{
	value -= n.value;
	return *this;
}

Number&
Number::operator*=(const Number& n)
{
	value *= n.value;
	return *this;
}

bool
operator==(const Number& x, const Number& y) noexcept
{
	return x.value == y.value;
}

bool
operator<(const Number& x, const Number& y) noexcept
{
	return x.value < y.value;
}

} // inline namespace Math;

} // namespace Unilang;

