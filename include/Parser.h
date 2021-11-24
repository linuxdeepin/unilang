// © 2020-2021 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Parser_h_
#define INC_Unilang_Parser_h_ 1

#include "Lexical.h" // for lref, LexicalAnalyzer, string, pmr, vector;

namespace Unilang
{

class BufferedByteParserBase
{
public:
	char Delimiter = {};

private:
	string buffer{};

public:
	BufferedByteParserBase(pmr::polymorphic_allocator<byte> a = {})
		: buffer(a)
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
	YB_ATTR_nodiscard YB_PURE char&
	GetBackRef() noexcept
	{
		return buffer.back();
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
	ByteParser(pmr::polymorphic_allocator<yimpl(byte)> a = {})
		: BufferedByteParserBase(a), lexemes(a)
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

