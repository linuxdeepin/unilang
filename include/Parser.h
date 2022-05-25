// © 2020-2022 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Parser_h_
#define INC_Unilang_Parser_h_ 1

#include "Lexical.h" // for lref, LexicalAnalyzer, pmr::polymorphic_allocator,
//	string, pmr, std::swap, vector;
#include <ystdex/type_traits.hpp> // for ystdex::remove_reference_t;
#include <ystdex/ref.hpp> // for ystdex::unwrap_ref_decay_t;
#include <ystdex/meta.hpp> // for ystdex::detected_or_t;

namespace Unilang
{

template<class _tParser>
using MemberParseResult = typename _tParser::ParseResult;

template<typename _fParse>
using ParserClassOf
	= ystdex::remove_reference_t<ystdex::unwrap_ref_decay_t<_fParse>>;

template<typename _fParse>
using ParseResultOf = ystdex::detected_or_t<vector<string>, MemberParseResult,
	ParserClassOf<_fParse>>;

template<typename _fParse>
using GParserResult = const ParseResultOf<_fParse>&;


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


class ByteParser : private BufferedByteParserBase
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
	ByteParser(const ByteParser& parse)
		: BufferedByteParserBase(parse),
		lexemes(parse.lexemes, parse.lexemes.get_allocator()),
		update_current(parse.update_current)
	{}
	ByteParser(ByteParser&&) = default;

	ByteParser&
	operator=(const ByteParser& parse)
	{
		return ystdex::copy_and_swap(*this, parse);
	}
	ByteParser&
	operator=(ByteParser&&) = default;

	template<typename... _tParams>
	inline void
	operator()(char c, _tParams&&... args)
	{
		auto& lexer(GetLexerRef());

		Update(lexer.FilterChar(c, GetBufferRef(), yforward(args)...)
			&& lexer.UpdateBack(GetBackRef(), c));
	}

	bool
	IsUpdating() const noexcept
	{
		return update_current;
	}

	using BufferedByteParserBase::GetBuffer;
	using BufferedByteParserBase::GetBufferRef;
	using BufferedByteParserBase::GetLexerRef;

	const ParseResult&
	GetResult() const noexcept
	{
		return lexemes;
	}

private:
	void
	Update(bool);

public:
	using BufferedByteParserBase::reserve;

	friend void
	swap(ByteParser& x, ByteParser& y)
	{
		swap(static_cast<BufferedByteParserBase&>(x),
			static_cast<BufferedByteParserBase&>(y));
		swap(x.lexemes, y.lexemes);
		std::swap(x.update_current, y.update_current);
	}
};


class SourcedByteParser : private BufferedByteParserBase
{
public:
	using ParseResult = vector<pair<SourceLocation, string>>;

private:
	mutable ParseResult lexemes{};
	bool update_current = {};
	SourceLocation source_location{0, 0};

public:
	SourcedByteParser(LexicalAnalyzer& lexer,
		pmr::polymorphic_allocator<yimpl(byte)> a = {})
		: BufferedByteParserBase(lexer, a), lexemes(a)
	{}
	SourcedByteParser(const SourcedByteParser& parse)
		: BufferedByteParserBase(parse),
		lexemes(parse.lexemes, parse.lexemes.get_allocator()),
		update_current(parse.update_current),
		source_location(parse.source_location)
	{}
	SourcedByteParser(SourcedByteParser&&) = default;

	SourcedByteParser&
	operator=(const SourcedByteParser& parse)
	{
		return ystdex::copy_and_swap(*this, parse);
	}
	SourcedByteParser&
	operator=(SourcedByteParser&&) = default;

	template<typename... _tParams>
	inline void
	operator()(char c, _tParams&&... args)
	{
		auto& lexer(GetLexerRef());

		Update(lexer.FilterChar(c, GetBufferRef(), yforward(args)...)
			&& lexer.UpdateBack(GetBackRef(), c));
		if(c != '\n')
			source_location.Step();
		else
			source_location.Newline();
	}

	bool
	IsUpdating() const noexcept
	{
		return update_current;
	}

	using BufferedByteParserBase::GetBuffer;
	using BufferedByteParserBase::GetBufferRef;
	using BufferedByteParserBase::GetLexerRef;
	const ParseResult&
	GetResult() const noexcept
	{
		return lexemes;
	}
	const SourceLocation&
	GetSourceLocation() const noexcept
	{
		return source_location;
	}

private:
	void
	Update(bool);

public:
	using BufferedByteParserBase::reserve;

	friend void
	swap(SourcedByteParser& x, SourcedByteParser& y)
	{
		swap(static_cast<BufferedByteParserBase&>(x),
			static_cast<BufferedByteParserBase&>(y));
		swap(x.lexemes, y.lexemes);
		std::swap(x.update_current, y.update_current);
		std::swap(x.source_location, y.source_location);
	}
};

} // namespace Unilang;

#endif

