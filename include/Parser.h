// © 2020-2021 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Parser_h_
#define INC_Unilang_Parser_h_ 1

#include "Lexical.h" // for lref, LexicalAnalyzer, string, pmr, vector;

namespace Unilang
{

class BufferedByteParserBase
{
private:
	lref<LexicalAnalyzer> lexer_ref;
	string buffer{};

public:
	BufferedByteParserBase(LexicalAnalyzer& lexer,
		pmr::polymorphic_allocator<byte> a = {})
		: lexer_ref(lexer), buffer(a)
	{}

	YB_ATTR_nodiscard YB_PURE const string&
	GetBuffer() const noexcept
	{
		return buffer;
	}
	YB_ATTR_nodiscard YB_PURE string&
	GetBufferRef() noexcept
	{
		return buffer;
	}
	YB_ATTR_nodiscard YB_PURE LexicalAnalyzer&
	GetLexerRef() const noexcept
	{
		return lexer_ref;
	}
	YB_ATTR_nodiscard YB_PURE char&
	GetBackRef() noexcept
	{
		return buffer.back();
	}

	size_t
	QueryLastDelimited(char ld) const noexcept
	{
		return buffer.length() - (ld != char() ? 1 : 0);
	}

	void
	reserve(size_t n)
	{
		buffer.reserve(n);
	}

	friend void
	swap(BufferedByteParserBase& x, BufferedByteParserBase& y) noexcept
	{
		std::swap(x.lexer_ref, y.lexer_ref);
		std::swap(x.buffer, y.buffer);
	}
};


class ByteParser final : BufferedByteParserBase
{
public:
	using ParseResult = vector<string>;

private:
	mutable ParseResult lexemes{};
	bool update_current = {};

public:
	ByteParser(LexicalAnalyzer& lexer,
		pmr::polymorphic_allocator<yimpl(byte)> a = {})
		: BufferedByteParserBase(lexer, a), lexemes(a)
	{}

	void
	operator()(char);

	const ParseResult&
	GetResult() const noexcept
	{
		return lexemes;
	}

private:
	bool
	UpdateBack(char);
};

} // namespace Unilang;

#endif

