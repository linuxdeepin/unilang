// © 2020-2021 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Parser_h_
#define INC_Unilang_Parser_h_ 1

#include "Unilang.h" // for vector, string;

namespace Unilang
{

class ByteParser final
{
public:
	using ParseResult = vector<string>;

private:
	char Delimiter = {};
	string buffer{};
	mutable ParseResult lexemes{};
	bool update_current = {};

public:
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

