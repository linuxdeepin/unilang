// © 2021 Uniontech Software Technology Co.,Ltd.

#include "Math.h"

namespace Unilang
{

inline namespace Math
{

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

