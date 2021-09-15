// © 2021 Uniontech Software Technology Co.,Ltd.

#include "Arithmetic.h"

namespace Unilang
{

inline namespace Numbers
{

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

} // inline namespace Numbers;

} // namespace Unilang;

