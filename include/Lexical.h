// © 2020-2021 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Lexical_h_
#define INC_Unilang_Lexical_h_ 1

#include "Unilang.h" // for string_view;
#include <cassert> // for assert;
#include <cctype> // for std::isgraph;

namespace Unilang
{

YB_ATTR_nodiscard YB_PURE char
CheckLiteral(string_view) noexcept;

YB_ATTR_nodiscard YB_PURE inline string_view
DeliteralizeUnchecked(string_view sv) noexcept
{
	assert(sv.data());
	assert(!(sv.size() < 2));
	return sv.substr(1, sv.size() - 2);
}

YB_ATTR_nodiscard YB_PURE inline string_view
Deliteralize(string_view sv) noexcept
{
	return CheckLiteral(sv) != char() ? DeliteralizeUnchecked(sv) : sv;
}

YB_ATTR_nodiscard YB_STATELESS constexpr bool
IsGraphicalDelimiter(char c) noexcept
{
	return c == '(' || c == ')' || c == ',' || c == ';';
}

YB_ATTR_nodiscard constexpr bool
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


YB_ATTR_nodiscard YB_PURE LexemeCategory
CategorizeBasicLexeme(string_view) noexcept;

YB_ATTR_nodiscard YB_PURE inline bool
IsUnilangSymbol(string_view id) noexcept
{
	return CategorizeBasicLexeme(id) == LexemeCategory::Symbol;
}

} // namespace Unilang;

#endif

