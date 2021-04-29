// © 2020-2021 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Lexical_h_
#define INC_Unilang_Lexical_h_ 1

#include "Unilang.h" // for string_view;
#include <cassert> // for assert;

namespace Unilang
{

[[nodiscard, gnu::pure]] char
CheckLiteral(string_view) noexcept;

[[nodiscard, gnu::pure]] inline string_view
DeliteralizeUnchecked(string_view sv) noexcept
{
	assert(sv.data());
	assert(!(sv.size() < 2));
	return sv.substr(1, sv.size() - 2);
}

[[nodiscard, gnu::pure]] inline string_view
Deliteralize(string_view sv) noexcept
{
	return CheckLiteral(sv) != char() ? DeliteralizeUnchecked(sv) : sv;
}

[[nodiscard, gnu::const]] constexpr bool
IsGraphicalDelimiter(char c) noexcept
{
	return c == '(' || c == ')' || c == ',' || c == ';';
}

[[nodiscard]] constexpr bool
IsDelimiter(char c) noexcept
{
#if CHAR_MIN < 0
	return c >= 0 && (!std::isgraph(c) || IsGraphicalDelimiter(c));
#else
	return !std::isgraph(c) || IsGraphicalDelimiter(c);
#endif
}


enum class LexemeCategory
{
	Symbol,
	Code,
	Data,
	Extended
};


[[nodiscard, gnu::pure]] LexemeCategory
CategorizeBasicLexeme(string_view) noexcept;

[[nodiscard, gnu::pure]] inline bool
IsUnilangSymbol(string_view id) noexcept
{
	return CategorizeBasicLexeme(id) == LexemeCategory::Symbol;
}

} // namespace Unilang;

#endif

