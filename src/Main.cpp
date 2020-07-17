// © 2020-2020 Uniontech Software Technology Co.,Ltd.

#include <utility> // for std::reference_wrapper, std::ref;
#include <any> // for std::any, std::bad_any_cast, std:;any_cast;
#include <functional> // for std::function;
#include <string_view> // for std::string_view;
#include <memory_resource> // for complete std::pmr::polymorphic_allocator;
#include <forward_list> // for std::pmr::forward_list;
#include <list> // for std::pmr::list;
#include <string> // for std::pmr::string, std::getline;
#include <vector> // for std::pmr::vector;
#include <deque> // for std::pmr::deque;
#include <stack> // for std::stack;
#include <sstream> // for std::basic_ostringstream, std::ostream,
//	std::streamsize;
#include <exception> // for std::runtime_error;
#include <cctype> // for std::isgraph;
#include <cassert> // for assert;
#include <algorithm> // for std::for_each;
#include <iostream>
#include <typeinfo> // for typeid;

namespace Unilang
{

using std::size_t;

template<typename _type>
using lref = std::reference_wrapper<_type>;

using std::any;
using std::bad_any_cast;
using std::function;
using std::string_view;

using std::pmr::forward_list;
using std::pmr::list;
using std::pmr::string;
using std::pmr::vector;

template<typename _type, class _tSeqCon = std::pmr::deque<_type>>
using stack = std::stack<_type, _tSeqCon>;
using ostringstream = std::basic_ostringstream<char, std::char_traits<char>,
	string::allocator_type>;


class UnilangException : public std::runtime_error
{
	using runtime_error::runtime_error;
};


enum class ReductionStatus : size_t
{
	Partial = 0x00,
	Neutral = 0x01,
	Clean = 0x02,
	Retained = 0x03,
	Regular = Retained,
	Retrying = 0x10
};


class Context;

using Reducer = function<ReductionStatus(Context&)>;

using ReducerSequence = forward_list<Reducer>;

using ValueObject = any;


[[nodiscard]] constexpr bool
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

void
ByteParser::operator()(char c)
{
	buffer += c;

	bool got_delim(UpdateBack(c));

	[&](auto add, auto append){
		const auto len(buffer.length());

		assert(!(lexemes.empty() && update_current));
		if(len > 0)
		{
			if(len == 1)
			{
				const auto update_c([&](char c){
					if(update_current)
						append(c);
					else
						add(string({c}, lexemes.get_allocator()));
				});
				const char c(buffer.back());
				const bool unquoted(Delimiter == char());

 				if(got_delim)
				{
					update_c(c);
					update_current = !unquoted;
				}
				else if(unquoted && IsDelimiter(c))
				{
					if(IsGraphicalDelimiter(c))
						add(string({c}, lexemes.get_allocator()));
					update_current = {};
				}
				else
				{
					update_c(c);
					update_current = true;
				}
			}
			else if(update_current)
				append(buffer.substr(0, len));
			else
				add(std::move(buffer));
			buffer.clear();
		}
	}([&](auto&& arg){
		lexemes.push_back(std::forward<decltype(arg)>(arg));
	}, [&](auto&& arg){
		lexemes.back() += std::forward<decltype(arg)>(arg);
	});
}

bool
ByteParser::UpdateBack(char c)
{
	auto& b(buffer.back());

	switch(c)
	{
		case '\'':
		case '"':
			if(Delimiter == char())
			{
				Delimiter = c;
				return true;
			}
			else if(Delimiter == c)
			{
				Delimiter = char();
				return true;
			}
			break;
		case '\f':
		case '\n':
		case '\t':
		case '\v':
			if(Delimiter == char())
			{
				b = ' ';
				break;
			}
			[[fallthrough]];
		default:
			break;
	}
	return {};
}


class TermNode final
{
public:
	using Container = list<TermNode>;
	using allocator_type = Container::allocator_type;
	using iterator = Container::iterator;
	using const_iterator = Container::const_iterator;
	using reverse_iterator = Container::reverse_iterator;
	using const_reverse_iterator = Container::const_reverse_iterator;

	Container Subterms{};
	ValueObject Value{};

	TermNode() = default;
	explicit
	TermNode(allocator_type a)
		: Subterms(a)
	{}
	TermNode(const Container& con)
		: Subterms(con)
	{}
	TermNode(Container&& con)
		: Subterms(std::move(con))
	{}
	TermNode(const TermNode&) = default;
	TermNode(const TermNode& tm, allocator_type a)
		: Subterms(tm.Subterms, a), Value(tm.Value)
	{}
	TermNode(TermNode&&) = default;
	TermNode(TermNode&& tm, allocator_type a)
		: Subterms(std::move(tm.Subterms), a), Value(std::move(tm.Value))
	{}
	
	TermNode&
	operator=(TermNode&&) = default;

	[[nodiscard, gnu::pure]] bool
	operator!() const noexcept
	{
		return !bool(*this);
	}

	[[nodiscard, gnu::pure]] explicit
	operator bool() const noexcept
	{
		return Value.has_value() || !empty();
	}

	void
	Add(const TermNode& term)
	{
		Subterms.push_back(term);
	}
	void
	Add(TermNode&& term)
	{
		Subterms.push_back(std::move(term));
	}

	[[nodiscard, gnu::pure]] iterator
	begin() noexcept
	{
		return Subterms.begin();
	}
	[[nodiscard, gnu::pure]] const_iterator
	begin() const noexcept
	{
		return Subterms.begin();
	}

	[[nodiscard, gnu::pure]] bool
	empty() const noexcept
	{
		return Subterms.empty();
	}

	[[nodiscard, gnu::pure]] iterator
	end() noexcept
	{
		return Subterms.end();
	}
	[[nodiscard, gnu::pure]] const_iterator
	end() const noexcept
	{
		return Subterms.end();
	}

	[[nodiscard, gnu::pure]]
	allocator_type
	get_allocator() const noexcept
	{
		return Subterms.get_allocator();
	}
};


template<typename... _tParams>
[[nodiscard, gnu::pure]] inline TermNode
AsTermNode(_tParams&&... args)
{
	TermNode term;

	term.Value = ValueObject(std::forward<_tParams>(args)...);
	return term;
}


struct LexemeTokenizer final
{
	// XXX: Allocator is ignored;
	LexemeTokenizer(TermNode::allocator_type = {})
	{}

	template<class _type>
	[[nodiscard, gnu::pure]] inline TermNode
	operator()(const _type& val) const
	{
		return AsTermNode(val);
	}
};

template<typename _tIn>
[[nodiscard]] inline _tIn
ReduceSyntax(TermNode& term, _tIn first, _tIn last)
{
	return ReduceSyntax(term, first, last,
		LexemeTokenizer{term.get_allocator()});
}
template<typename _tIn, typename _fTokenize>
[[nodiscard]] _tIn
ReduceSyntax(TermNode& term, _tIn first, _tIn last, _fTokenize tokenize)
{
	const auto a(term.get_allocator());
	stack<TermNode> tms(a);
	struct Guard final
	{
		TermNode& term;
		stack<TermNode>& tms;

		~Guard()
		{
			assert(!tms.empty());
			term = std::move(tms.top());
		}
	} gd{term, tms};

	tms.push(std::move(term));
	for(; first != last; ++first)
		if(*first == "(")
			tms.push(AsTermNode());
		else if(*first != ")")
		{
			assert(!tms.empty());
			tms.top().Add(tokenize(*first));
		}
		else if(tms.size() != 1)
		{
			auto tm(std::move(tms.top()));

			tms.pop();
			assert(!tms.empty());
			tms.top().Add(std::move(tm));
		}
		else
			return first;
	if(tms.size() == 1)
		return first;
	throw UnilangException("Redundant '(' found.");
}


// NOTE: The collection of values of unit types.
enum class ValueToken
{
	Unspecified
};


// NOTE: The host type of symbol.
// XXX: The destructor is not virtual.
class TokenValue final : public string
{
public:
	using base = string;

	TokenValue() = default;
	using base::base;
	TokenValue(const base& b)
		: base(b)
	{}
	TokenValue(base&& b)
	: base(std::move(b))
	{}
	TokenValue(const TokenValue&) = default;
	TokenValue(TokenValue&&) = default;

	TokenValue&
	operator=(const TokenValue&) = default;
	TokenValue&
	operator=(TokenValue&&) = default;
};


// NOTE: The host type of reference values.
class TermReference final
{
private:
	lref<TermNode> term_ref;

public:
	inline
	TermReference(TermNode& term) noexcept
		: term_ref(term)
	{}
	TermReference(const TermReference&) = default;

	TermReference&
	operator=(const TermReference&) = default;

	[[nodiscard, gnu::pure]] friend bool
	operator==(const TermReference& x, const TermReference& y) noexcept
	{
		return &x.term_ref.get() == &y.term_ref.get();
	}

	[[nodiscard, gnu::pure]] friend bool
	operator!=(const TermReference& x, const TermReference& y) noexcept
	{
		return &x.term_ref.get() != &y.term_ref.get();
	}

	[[nodiscard, gnu::pure]] explicit
	operator TermNode&() const noexcept
	{
		return term_ref;
	}

	[[nodiscard, gnu::pure]] TermNode&
	get() const noexcept
	{
		return term_ref.get();
	}
};


// NOTE: This is the main entry of the evaluation algorithm.
ReductionStatus
ReduceOnce(TermNode&, Context&);

ReductionStatus
ReduceOnce(TermNode&, Context&)
{
	return ReductionStatus::Neutral;
}


class Context final
{
private:
	TermNode* next_term_ptr = {};
	ReducerSequence current{};

public:
	Reducer TailAction{};
	ReductionStatus LastStatus = ReductionStatus::Neutral;

	bool
	IsAlive() const noexcept
	{
		return !current.empty();
	}

	[[nodiscard, gnu::pure]] TermNode&
	GetNextTermRef() const;

	ReductionStatus
	ApplyTail();

	ReductionStatus
	Evaluate(TermNode&);

	ReductionStatus
	Rewrite(Reducer);

	template<typename... _tParams>
	inline void
	SetupCurrent(_tParams&&... args)
	{
		assert(!IsAlive());
		return SetupFront(std::forward<_tParams>(args)...);
	}

	template<typename... _tParams>
	inline void
	SetupFront(_tParams&&... args)
	{
		current.emplace_front(std::forward<_tParams>(args)...);
	}
};

TermNode&
Context::GetNextTermRef() const
{
	if(const auto p = next_term_ptr)
		return *p;
	throw UnilangException("No next term found to evaluation.");
}

ReductionStatus
Context::ApplyTail()
{
	assert(IsAlive());
	TailAction = std::move(current.front());
	current.pop_front();
	LastStatus = TailAction(*this);
	return LastStatus;
}

ReductionStatus
Context::Evaluate(TermNode& term)
{
	next_term_ptr = &term;
	return Rewrite([this](Context& ctx){
		return ReduceOnce(ctx.GetNextTermRef(), ctx);
	});
}

ReductionStatus
Context::Rewrite(Reducer reduce)
{
	SetupCurrent(std::move(reduce));
	// NOTE: Rewrite until no actions remain.
	do
	{
		ApplyTail();
	}while(IsAlive());
	return LastStatus;
}


// NOTE: This is the host type for combiners.
using ContextHandler = function<ReductionStatus(TermNode&, Context&)>;


namespace
{

void
PrintTermNode(std::ostream& os, const TermNode& term, size_t depth = 0)
{
	const auto print_indent([&](size_t n){
		if(n != 0)
		{
			const auto& s(string(n, '\t'));

			os.write(s.data(), std::streamsize(s.size()));
		}
	});

	print_indent(depth);

	try
	{
		const auto& vo(term.Value);

		os << [&]() -> string{
			if(const auto p = std::any_cast<string>(&vo))
				return *p;
			if(const auto p = std::any_cast<bool>(&vo))
				return *p ? "#t" : "#f";

			const auto& t(vo.type());

			if(t != typeid(void))
				return "#[" + string(t.name()) + ']';
			throw bad_any_cast();
		}() << '\n';
	}
	catch(bad_any_cast&)
	{
		os << '\n';
		if(term)
			for(const auto& nd : term)
			{
				print_indent(depth);
				os << '(' << '\n';
				try
				{
					PrintTermNode(os, nd, depth + 1);
				}
				catch(std::out_of_range&)
				{}
				print_indent(depth);
				os << ')' << '\n';
			}
	}
}


class Interpreter final
{
private:
	string line{};

public:
	Context Root{};

	Interpreter();
	Interpreter(const Interpreter&) = delete;

	void
	Evaluate(TermNode&);

	static void
	Print(const TermNode&);

	bool
	Process();

	[[nodiscard]] TermNode
	Read(string_view);

	void
	Run();

	std::istream&
	WaitForLine();
};

Interpreter::Interpreter()
{}

void
Interpreter::Evaluate(TermNode& term)
{
	Root.Evaluate(term);
}

void
Interpreter::Print(const TermNode& term)
{
	ostringstream oss(string(term.get_allocator()));

	PrintTermNode(std::cout, term);
}

bool
Interpreter::Process()
{
	if(line == "exit")
		return {};
	else if(!line.empty())
		try
		{
			auto term(Read(line));

			Evaluate(term);
			Print(term);
		}
		catch(UnilangException& e)
		{
			using namespace std;

			cerr << "UnilangException[" << typeid(e).name() << "]: "
				<< e.what() << endl;
		}
	return true;
}

TermNode
Interpreter::Read(string_view unit)
{
	ByteParser parse{};

	std::for_each(unit.begin(), unit.end(), std::ref(parse));

	const auto& parse_result(parse.GetResult());
	TermNode term{};

	if(ReduceSyntax(term, parse_result.cbegin(), parse_result.cend())
		!= parse_result.cend())
		throw UnilangException("Redundant ')' found.");
	return term;
}

void
Interpreter::Run()
{
	while(WaitForLine() && Process())
		;
}

std::istream&
Interpreter::WaitForLine()
{
	using namespace std;

	cout << "> ";
	return getline(cin, line);
}

#define APP_NAME "Unilang demo"
#define APP_VER "0.0.10"
#define APP_PLATFORM "[C++17]"
constexpr auto
	title(APP_NAME " " APP_VER " @ (" __DATE__ ", " __TIME__ ") " APP_PLATFORM);

} // unnamed namespace;

} // namespace Unilang;

int
main()
{
	using namespace Unilang;
	using namespace std;
	Interpreter intp{};

	cout << title << endl << "Initializing...";
	cout << "Initialization finished." << endl;
	cout << "Type \"exit\" to exit, \"cls\" to clear screen." << endl << endl;
	intp.Run();
}

