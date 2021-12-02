// © 2021 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Math_h_
#define INC_Unilang_Math_h_ 1

#include "Unilang.h"
#include <ystdex/operators.hpp> // for ystdex::ordered_field_operators;

namespace Unilang
{

inline namespace Math
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

	friend bool
	operator==(const Number&, const Number&) noexcept;

	friend bool
	operator<(const Number&, const Number&) noexcept;

	operator int() const noexcept
	{
		return value;
	}
};

} // inline namespace Math;

} // namespace Unilang;

#endif

