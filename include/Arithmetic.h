// © 2021 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Arithmetic_h_
#define INC_Unilang_Arithmetic_h_ 1

#include "Unilang.h"
#include <ystdex/operators.hpp> // for ystdex::ordered_field_operators;

namespace Unilang
{

inline namespace Numbers
{

class Number : private ystdex::ordered_field_operators<Number>
{
private:
	int value;

public:
	Number(int);

	Number&
	operator+=(const Number&);

	Number&
	operator-=(const Number&);

	Number&
	operator*=(const Number&);

	[[nodiscard, gnu::pure]] friend bool
	operator==(const Number& x, const Number& y) noexcept;

	[[nodiscard, gnu::pure]] friend bool
	operator<(const Number& x, const Number& y) noexcept;

	// XXX: Temporary workaround to avoid ambiguity
	[[nodiscard, gnu::pure]] friend Number
	operator-(const Number& x, const Number& y);
};

} // inline namespace Numbers;

} // namespace Unilang;

#endif

