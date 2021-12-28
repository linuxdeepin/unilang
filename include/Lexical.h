// © 2020-2021 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Lexical_h_
#define INC_Unilang_Lexical_h_ 1

#include "Unilang.h" // for string_view;
#include <cassert> // for assert;
#include <ystdex/cctype.h> // for ystdex::isspace;

namespace Unilang
{

class UnescapeContext final
{
public:
	size_t Start = size_t(-1);
	size_t Length = 0;

	YB_ATTR_nodiscard bool
	IsHandling() const noexcept
	{
		return Length != 0;
	}

	void
	Clear() noexcept
	{
		Length =  0;
	}

	void
	VerifyBufferLength(size_t len) const
	{
		yunused(len);
		assert((Length == 0 || Start + Length <= len)
			&& "Invalid unescaping state found.");
	}
};


YB_ATTR_nodiscard bool
HandleBackslashPrefix(string_view, UnescapeContext&);

YB_ATTR_nodiscard bool
Unescape(string&, char, UnescapeContext&, char);


class LexicalAnalyzer final
{
private:
	UnescapeContext unescape_context{};
	char ld = {};

public:
	YB_ATTR_nodiscard char
	GetDelimiter() const noexcept
	{
		return ld;
	}
	YB_ATTR_nodiscard const UnescapeContext&
	GetUnescapeContext() const noexcept
	{
		return unescape_context;
	}

	template<class _tCharBuffer,
		typename _fUnescape = decltype(Unescape),
		typename _fPrefixHandler = decltype(HandleBackslashPrefix)>
	YB_ATTR_nodiscard bool
	FilterChar(char c, _tCharBuffer& cbuf, _fUnescape unescape = Unescape,
		_fPrefixHandler handle_prefix = HandleBackslashPrefix)
	{
		if(!unescape_context.IsHandling())
		{
			cbuf += c;
			return !handle_prefix(cbuf, unescape_context);
		}
		unescape(cbuf, c, unescape_context, ld);
		return {};
	}

	bool
	UpdateBack(char&, char);
};


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

YB_ATTR_nodiscard YB_PURE string
Escape(string_view);

YB_ATTR_nodiscard YB_PURE string
EscapeLiteral(string_view);

YB_ATTR_nodiscard YB_STATELESS constexpr bool
IsGraphicalDelimiter(char c) noexcept
{
	return c == '(' || c == ')' || c == ',' || c == ';'
		|| c == '[' || c == ']' || c == '{' || c == '}';
}

YB_ATTR_nodiscard constexpr bool
IsDelimiter(char c) noexcept
{
	return ystdex::isspace(c) || IsGraphicalDelimiter(c);
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

