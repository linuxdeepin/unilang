// © 2020 Uniontech Software Technology Co.,Ltd.

#include <ystdex/utility.hpp> // for ystdex::size_t, std::pair, ystdex::lref,
//	ystdex::ref, std::swap, ystdex::exchange;
#include <ystdex/any.h> // for ystdex::any, ystdex::bad_any_cast,
//	ystdex::any_cast;
#include <ystdex/functional.hpp> // for ystdex::function, ystdex::less,
//	ystdex::expand_proxy;
#include <memory> // for std::shared_ptr, std::weak_ptr;
#include <ystdex/string_view.hpp> // for ystdex::string_view;
#include <ystdex/memory_resource.h> // for ystdex::pmr and
//	complete ystdex::pmr::polymorphic_allocator;
#include <ystdex/string.hpp> // for ystdex::basic_string, std::string,
//	ystdex::sfmt, std::getline, ystdex::begins_with;
#include <forward_list> // for std::forward_list;
#include <list> // for std::list;
#include <map> // for std::map;
#include <cctype> // for std::isgraph;
#include <vector> // for std::vector;
#include <deque> // for std::deque;
#include <stack> // for std::stack;
#include <sstream> // for std::basic_ostringstream, std::ostream,
//	std::streamsize;
#include <exception> // for std::runtime_error;
#include <cassert> // for assert;
#include <iterator> // for std::next, std::prev, std::make_move_iterator;
#include <ystdex/cctype.h> // for ystdex::isdigit;
#include <ystdex/algorithm.hpp> // for ystdex::split;
#include <algorithm> // for std::find_if, std::for_each;
#include <type_traits> // for std::is_same, std::enable_if_t;
#include <typeinfo> // for typeid;
#include <cstdlib> // for std::getenv;
#include <iostream> // for std::cout, std::cerr, std::endl, std::cin;

namespace Unilang
{

using ystdex::size_t;

using ystdex::lref;

using ystdex::any;
using ystdex::bad_any_cast;
using ystdex::function;
using ystdex::make_shared;
using std::pair;
using std::shared_ptr;
using ystdex::string_view;
using std::weak_ptr;

namespace pmr = ystdex::pmr;

template<typename _tChar, typename _tTraits = std::char_traits<_tChar>,
	class _tAlloc = pmr::polymorphic_allocator<_tChar>>
using basic_string = ystdex::basic_string<_tChar, _tTraits, _tAlloc>;

using string = basic_string<char>;

template<typename _type, class _tAlloc = pmr::polymorphic_allocator<_type>>
using forward_list = std::forward_list<_type, _tAlloc>;

template<typename _type, class _tAlloc = pmr::polymorphic_allocator<_type>>
using list = ystdex::list<_type, _tAlloc>;

template<typename _tKey, typename _tMapped, typename _fComp
	= ystdex::less<_tKey>, class _tAlloc
	= pmr::polymorphic_allocator<std::pair<const _tKey, _tMapped>>>
using map = std::map<_tKey, _tMapped, _fComp, _tAlloc>;

template<typename _type, class _tAlloc = pmr::polymorphic_allocator<_type>>
using vector = std::vector<_type, _tAlloc>;

template<typename _type, class _tAlloc = pmr::polymorphic_allocator<_type>>
using deque = std::deque<_type, _tAlloc>;

template<typename _type, class _tSeqCon = deque<_type>>
using stack = std::stack<_type, _tSeqCon>;

using ostringstream = std::basic_ostringstream<char, std::char_traits<char>,
	string::allocator_type>;

// NOTE: Only use the unqualified call for unqualified 'string' type.
using ystdex::sfmt;


using ValueObject = any;


[[nodiscard, gnu::pure]] char
CheckLiteral(string_view) noexcept;

char
CheckLiteral(string_view sv) noexcept
{
	assert(sv.data());
	if(sv.length() > 1)
		if(const char c = sv.front() == sv.back() ? sv.front() : char())
		{
			if(c == '\'' || c == '"')
				return c;
		}
	return {};
}

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


class UnilangException : public std::runtime_error
{
	using runtime_error::runtime_error;
};


class TypeError : public UnilangException
{
	using UnilangException::UnilangException;
};


class ListReductionFailure : public TypeError
{
	using TypeError::TypeError;
};


class InvalidSyntax : public UnilangException
{
	using UnilangException::UnilangException;
};


class ParameterMismatch : public InvalidSyntax
{
	using InvalidSyntax::InvalidSyntax;
};


class ArityMismatch : public ParameterMismatch
{
private:
	size_t expected;
	size_t received;

public:
	ArityMismatch(size_t, size_t);

	[[nodiscard, gnu::pure]] size_t
	GetExpected() const noexcept
	{
		return expected;
	}

	[[nodiscard, gnu::pure]] size_t
	GetReceived() const noexcept
	{
		return received;
	}
};

ArityMismatch::ArityMismatch(size_t e, size_t r)
	: ParameterMismatch(
	ystdex::sfmt("Arity mismatch: expected %zu, received %zu.", e, r)),
	expected(e), received(r)
{}


class BadIdentifier : public InvalidSyntax
{
private:
	shared_ptr<string> p_identifier;

public:
	[[gnu::nonnull(2)]]
	BadIdentifier(const char*, size_t = 0);
	BadIdentifier(string_view, size_t = 0);

	[[nodiscard, gnu::pure]] const string&
	GetIdentifier() const noexcept
	{
		assert(p_identifier);
		return *p_identifier;
	}

private:
	std::string
	InitBadIdentifierExceptionString(std::string&& id, size_t n)
	{
		return (n != 0 ? (n == 1 ? "Bad identifier: '" 
			: "Duplicate identifier: '")
			: "Unknown identifier: '") + std::move(id) + "'.";
	}
};

BadIdentifier::BadIdentifier(const char* id, size_t n)
	: InvalidSyntax(InitBadIdentifierExceptionString(id, n)),
	p_identifier(make_shared<string>(id))
{}
BadIdentifier::BadIdentifier(string_view id, size_t n)
	: InvalidSyntax(InitBadIdentifierExceptionString(std::string(id.data(),
	id.data() + id.size()), n)), p_identifier(make_shared<string>(id))
{}


enum class LexemeCategory
{
	Symbol,
	Code,
	Data,
	Extended
};


[[nodiscard, gnu::pure]] LexemeCategory
CategorizeBasicLexeme(string_view) noexcept;

LexemeCategory
CategorizeBasicLexeme(string_view id) noexcept
{
	assert(id.data());

	const auto c(CheckLiteral(id));

	if(c == '\'')
		return LexemeCategory::Code;
	if(c != char())
		return LexemeCategory::Data;
	return LexemeCategory::Symbol;
}


class TermNode final
{
private:
	template<typename... _tParams>
	using enable_value_constructible_t = std::enable_if_t<
		std::is_constructible<ValueObject, _tParams...>::value>;

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
	template<typename... _tParams,
		typename = enable_value_constructible_t<_tParams...>>
	TermNode(const Container& con, _tParams&&... args)
		: Subterms(con), Value(yforward(args)...)
	{}
	template<typename... _tParams,
		typename = enable_value_constructible_t<_tParams...>>
	TermNode(Container&& con, _tParams&&... args)
		: Subterms(std::move(con)), Value(yforward(args)...)
	{}
	template<typename... _tParams,
		typename = enable_value_constructible_t<_tParams...>>
	inline
	TermNode(std::allocator_arg_t, allocator_type a, const Container& con,
		_tParams&&... args)
		: Subterms(con, a), Value(yforward(args)...)
	{}
	template<typename... _tParams,
		typename = enable_value_constructible_t<_tParams...>>
	inline
	TermNode(std::allocator_arg_t, allocator_type a, Container&& con,
		_tParams&&... args)
		: Subterms(std::move(con), a), Value(yforward(args)...)
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
	operator=(const TermNode&) = default;
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

	iterator
	erase(const_iterator i)
	{
		return Subterms.erase(i);
	}

	[[nodiscard, gnu::pure]]
	allocator_type
	get_allocator() const noexcept
	{
		return Subterms.get_allocator();
	}

	[[nodiscard, gnu::pure]] size_t
	size() const noexcept
	{
		return Subterms.size();
	}
};

using TNIter = TermNode::iterator;
using TNCIter = TermNode::const_iterator;

[[nodiscard, gnu::pure]] inline bool
IsBranch(const TermNode& term) noexcept
{
	return !term.empty();
}

[[nodiscard, gnu::pure]] inline bool
IsBranchedList(const TermNode& term) noexcept
{
	return !(term.empty() || term.Value.has_value());
}

[[nodiscard, gnu::pure]] inline bool
IsEmpty(const TermNode& term) noexcept
{
	return !term;
}

[[nodiscard, gnu::pure]] inline bool
IsLeaf(const TermNode& term) noexcept
{
	return term.empty();
}

[[nodiscard, gnu::pure]] inline bool
IsList(const TermNode& term) noexcept
{
	return !term.Value.has_value();
}

[[nodiscard, gnu::pure]] inline TermNode&
AccessFirstSubterm(TermNode& term) noexcept
{
	assert(IsBranch(term));
	return *term.begin();
}
[[nodiscard, gnu::pure]] inline const TermNode&
AccessFirstSubterm(const TermNode& term) noexcept
{
	assert(IsBranch(term));
	return *term.begin();
}

[[nodiscard, gnu::pure]] inline TermNode&&
MoveFirstSubterm(TermNode& term)
{
	return std::move(AccessFirstSubterm(term));
}

inline void
RemoveHead(TermNode& term) noexcept
{
	assert(!term.empty());
	term.erase(term.begin());
}

template<typename... _tParams>
[[nodiscard, gnu::pure]] inline TermNode
AsTermNode(_tParams&&... args)
{
	TermNode term;

	term.Value = ValueObject(std::forward<_tParams>(args)...);
	return term;
}


template<typename _type>
[[nodiscard, gnu::pure]] inline bool
HasValue(const TermNode& term, const _type& x)
{
	if(const auto p = ystdex::any_cast<_type>(&term.Value))
		return *p == x;
	return {};
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

[[nodiscard, gnu::pure]] string
TermToString(const TermNode&);

[[nodiscard, gnu::pure]] string
TermToStringWithReferenceMark(const TermNode&, bool);

template<typename _type>
[[nodiscard, gnu::pure]] inline _type*
TryAccessLeaf(TermNode& term)
{
	return ystdex::any_cast<_type>(&term.Value);
}
template<typename _type>
[[nodiscard, gnu::pure]] inline const _type*
TryAccessLeaf(const TermNode& term)
{
	return ystdex::any_cast<_type>(&term.Value);
}

template<typename _type>
[[nodiscard, gnu::pure]] inline _type*
TryAccessTerm(TermNode& term)
{
	return IsLeaf(term) ? Unilang::TryAccessLeaf<_type>(term) : nullptr;
}
template<typename _type>
[[nodiscard, gnu::pure]] inline const _type*
TryAccessTerm(const TermNode& term)
{
	return IsLeaf(term) ? Unilang::TryAccessLeaf<_type>(term) : nullptr;
}

[[nodiscard, gnu::pure]] inline const TokenValue*
TermToNamePtr(const TermNode& term)
{
	return Unilang::TryAccessTerm<TokenValue>(term);
}

string
TermToString(const TermNode& term)
{
	if(const auto p = TermToNamePtr(term))
		return *p;
	return sfmt<string>("#<unknown{%zu}:%s>", term.size(),
		term.Value.type().name());
}

string
TermToStringWithReferenceMark(const TermNode& term, bool has_ref)
{
	auto term_str(TermToString(term));

	return has_ref ? "[*] " + std::move(term_str) : std::move(term_str);
}


class Environment;

using AnchorPtr = shared_ptr<const void>;


class EnvironmentReference
{
private:
	weak_ptr<Environment> p_weak{};
	AnchorPtr p_anchor{};

public:
	EnvironmentReference() = default;
	EnvironmentReference(const shared_ptr<Environment>&) noexcept;
	template<typename _tParam1, typename _tParam2>
	EnvironmentReference(_tParam1&& arg1, _tParam2&& arg2) noexcept
		: p_weak(std::forward<_tParam1>(arg1)),
		p_anchor(std::forward<_tParam2>(arg2))
	{}
	EnvironmentReference(const EnvironmentReference&) = default;
	EnvironmentReference(EnvironmentReference&&) = default;

	EnvironmentReference&
	operator=(const EnvironmentReference&) = default;
	EnvironmentReference&
	operator=(EnvironmentReference&&) = default;

	[[nodiscard, gnu::pure]] friend bool
	operator==(const EnvironmentReference& x, const EnvironmentReference& y)
		noexcept
	{
		return x.p_weak.lock() == y.p_weak.lock();
	}
	[[nodiscard, gnu::pure]] friend bool
	operator!=(const EnvironmentReference& x, const EnvironmentReference& y)
		noexcept
	{
		return x.p_weak.lock() != y.p_weak.lock();
	}

	[[nodiscard, gnu::pure]] const AnchorPtr&
	GetAnchorPtr() const noexcept
	{
		return p_anchor;
	}

	[[nodiscard, gnu::pure]] const weak_ptr<Environment>&
	GetPtr() const noexcept
	{
		return p_weak;
	}

	shared_ptr<Environment>
	Lock() const noexcept
	{
		return p_weak.lock();
	}
};


// NOTE: The host type of reference values.
class TermReference final
{
private:
	lref<TermNode> term_ref;
	EnvironmentReference r_env;

public:
	template<typename _tParam, typename... _tParams>
	inline
	TermReference(TermNode& term, _tParam&& arg, _tParams&&... args) noexcept
		: term_ref(term),
		r_env(std::forward<_tParam>(arg), std::forward<_tParams>(args)...)
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

	[[nodiscard, gnu::pure]] const EnvironmentReference&
	GetEnvironmentReference() const noexcept
	{
		return r_env;
	}

	[[nodiscard, gnu::pure]] TermNode&
	get() const noexcept
	{
		return term_ref.get();
	}
};


[[nodiscard, gnu::pure]] inline TermNode&
ReferenceTerm(TermNode& term)
{
	if(const auto p = Unilang::TryAccessLeaf<const TermReference>(term))
		return p->get();
	return term;
}
[[nodiscard, gnu::pure]] inline const TermNode&
ReferenceTerm(const TermNode& term)
{
	if(const auto p = Unilang::TryAccessLeaf<const TermReference>(term))
		return p->get();
	return term;
}


using ResolvedTermReferencePtr = const TermReference*;

[[nodiscard, gnu::pure]] constexpr ResolvedTermReferencePtr
ResolveToTermReferencePtr(const TermReference* p) noexcept
{
	return p;
}


template<typename _type>
[[nodiscard, gnu::pure]] inline _type*
TryAccessReferencedTerm(TermNode& term)
{
	return Unilang::TryAccessTerm<_type>(ReferenceTerm(term));
}
template<typename _type>
[[nodiscard, gnu::pure]] inline const _type*
TryAccessReferencedTerm(const TermNode& term)
{
	return Unilang::TryAccessTerm<_type>(ReferenceTerm(term));
}

template<typename _func, class _tTerm>
auto
ResolveTerm(_func do_resolve, _tTerm&& term)
	-> decltype(ystdex::expand_proxy<yimpl(void)(_tTerm&&,
	ResolvedTermReferencePtr)>::call(do_resolve, yforward(term),
	ResolvedTermReferencePtr()))
{
	using handler_t = void(_tTerm&&, ResolvedTermReferencePtr);

	if(const auto p = Unilang::TryAccessLeaf<const TermReference>(term))
		return ystdex::expand_proxy<handler_t>::call(do_resolve, p->get(),
			Unilang::ResolveToTermReferencePtr(p));
	return ystdex::expand_proxy<handler_t>::call(do_resolve,
		yforward(term), ResolvedTermReferencePtr());
}


enum class ReductionStatus : size_t
{
	Partial = 0x00,
	Neutral = 0x01,
	Clean = 0x02,
	Retained = 0x03,
	Regular = Retained,
	Retrying = 0x10
};

[[nodiscard, gnu::const]] constexpr bool
CheckReducible(ReductionStatus status) noexcept
{
	return status == ReductionStatus::Partial
		|| status == ReductionStatus::Retrying;
}


void
LiftOther(TermNode&, TermNode&);

void
LiftOther(TermNode& term, TermNode& tm)
{
	assert(&term != &tm);

	const auto t(std::move(term.Subterms));

	term.Subterms = std::move(tm.Subterms);
	term.Value = std::move(tm.Value);
}

class Context;

ReductionStatus
ReduceBranchToList(TermNode&, Context&) noexcept;

ReductionStatus
ReduceBranchToList(TermNode& term, Context&) noexcept
{
	RemoveHead(term);
	return ReductionStatus::Retained;
}


using EnvironmentList = vector<ValueObject>;


class Environment final
{
public:
	using BindingMap = map<string, TermNode, ystdex::less<>>;
	using NameResolution
		= pair<BindingMap::mapped_type*, shared_ptr<Environment>>;
	using allocator_type = BindingMap::allocator_type;

	mutable BindingMap Bindings;
	ValueObject Parent{};

private:
	AnchorPtr p_anchor{InitAnchor()};

public:
	Environment(allocator_type a)
		: Bindings(a)
	{}
	Environment(pmr::memory_resource& rsrc)
		: Environment(allocator_type(&rsrc))
	{}
	explicit
	Environment(const BindingMap& m)
		: Bindings(m)
	{}
	explicit
	Environment(BindingMap&& m)
		: Bindings(std::move(m))
	{}
	Environment(const ValueObject& vo, allocator_type a)
		: Bindings(a), Parent((CheckParent(vo), vo))
	{}
	Environment(ValueObject&& vo, allocator_type a)
		: Bindings(a), Parent((CheckParent(vo), std::move(vo)))
	{}
	Environment(pmr::memory_resource& rsrc, const ValueObject& vo)
		: Environment(vo, allocator_type(&rsrc))
	{}
	Environment(pmr::memory_resource& rsrc, ValueObject&& vo)
		: Environment(std::move(vo), allocator_type(&rsrc))
	{}
	Environment(const Environment& e)
		: Bindings(e.Bindings), Parent(e.Parent)
	{}
	Environment(Environment&&) = default;

	Environment&
	operator=(Environment&&) = default;

	[[nodiscard, gnu::pure]] friend
	operator==(const Environment& x, const Environment& y) noexcept
	{
		return &x == &y;
	}
	[[nodiscard, gnu::pure]] friend
	operator!=(const Environment& x, const Environment& y) noexcept
	{
		return &x != &y;
	}

	[[nodiscard, gnu::pure]] const AnchorPtr&
	GetAnchorPtr() const noexcept
	{
		return p_anchor;
	}

	static void
	CheckParent(const ValueObject&);

private:
	[[nodiscard, gnu::pure]] AnchorPtr
	InitAnchor() const;

public:
	[[nodiscard, gnu::pure]] NameResolution::first_type
	LookupName(string_view) const;
};

EnvironmentReference::EnvironmentReference(const shared_ptr<Environment>& p_env)
	noexcept
	: EnvironmentReference(p_env, p_env ? p_env->GetAnchorPtr() : nullptr)
{}

namespace
{

struct AnchorData final
{
public:
	AnchorData() = default;
	AnchorData(AnchorData&&) = default;

	AnchorData&
	operator=(AnchorData&&) = default;
};

using Redirector = function<const ValueObject*()>;

const ValueObject*
RedirectEnvironmentList(EnvironmentList::const_iterator first,
	EnvironmentList::const_iterator last, string_view id, Redirector& cont)
{
	if(first != last)
	{
		cont = std::bind(
			[=, &cont](EnvironmentList::const_iterator i, Redirector& c){
			cont = std::move(c);
			return RedirectEnvironmentList(i, last, id, cont);
		}, std::next(first), std::move(cont));
		return &*first;
	}
	return {};
}

} // unnamed namespace;

void
Environment::CheckParent(const ValueObject&)
{
	// TODO: Check parent type.
}

AnchorPtr
Environment::InitAnchor() const
{
	return std::allocate_shared<AnchorData>(Bindings.get_allocator());
}

Environment::NameResolution::first_type
Environment::LookupName(string_view id) const
{
	assert(id.data());

	const auto i(Bindings.find(id));

	return i != Bindings.cend() ? &i->second : nullptr;
}


using Reducer = function<ReductionStatus(Context&)>;

using ReducerSequence = forward_list<Reducer>;

// NOTE: This is the main entry of the evaluation algorithm.
ReductionStatus
ReduceOnce(TermNode&, Context&);


class Context final
{
private:
	lref<pmr::memory_resource> memory_rsrc;
	shared_ptr<Environment> p_record{std::allocate_shared<Environment>(
		Environment::allocator_type(&memory_rsrc.get()))};
	TermNode* next_term_ptr = {};
	ReducerSequence current{};

public:
	Reducer TailAction{};
	ReductionStatus LastStatus = ReductionStatus::Neutral;

	Context(pmr::memory_resource& rsrc)
		: memory_rsrc(rsrc)
	{}

	bool
	IsAlive() const noexcept
	{
		return !current.empty();
	}

	[[nodiscard]] Environment::BindingMap&
	GetBindingsRef() const noexcept
	{
		return p_record->Bindings;
	}
	[[nodiscard, gnu::pure]] TermNode&
	GetNextTermRef() const;
	[[nodiscard, gnu::pure]] const shared_ptr<Environment>&
	GetRecordPtr() const noexcept
	{
		return p_record;
	}
	[[nodiscard]] Environment&
	GetRecordRef() const noexcept
	{
		return *p_record;
	}

	void
	SetNextTermRef(TermNode& term) noexcept
	{
		next_term_ptr = &term;
	}

	ReductionStatus
	ApplyTail();

	ReductionStatus
	Evaluate(TermNode&);

	ReductionStatus
	Rewrite(Reducer);

	Environment::NameResolution
	Resolve(shared_ptr<Environment>, string_view);

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

	[[nodiscard, gnu::pure]] shared_ptr<Environment>
	ShareRecord() const noexcept;

	shared_ptr<Environment>
	SwitchEnvironmentUnchecked(const shared_ptr<Environment>&) noexcept;

	void
	UnwindCurrent() noexcept;

	[[nodiscard, gnu::pure]] EnvironmentReference
	WeakenRecord() const noexcept;
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

Environment::NameResolution
Context::Resolve(shared_ptr<Environment> p_env, string_view id)
{
	assert(bool(p_env));
	Redirector cont;
	// NOTE: Blocked. Use ISO C++14 deduced lambda return type (cf. CWG 975)
	//	compatible to G++ attribute.
	Environment::NameResolution::first_type p_obj;

	do
	{
		p_obj = p_env->LookupName(id);
	}while([&]() -> bool{
		if(!p_obj)
		{
			lref<const ValueObject> cur(p_env->Parent);
			shared_ptr<Environment> p_redirected{};
			bool search_next;

			do
			{
				const ValueObject& parent(cur);
				const auto& tp(parent.type());

				if(tp == ystdex::type_id<EnvironmentReference>())
				{
					p_redirected = ystdex::any_cast<
						EnvironmentReference>(&parent)->Lock();
					assert(bool(p_redirected));
					p_env.swap(p_redirected);
				}
				else if(tp == ystdex::type_id<shared_ptr<Environment>>())
				{
					p_redirected = *ystdex::any_cast<
						shared_ptr<Environment>>(&parent);
					assert(bool(p_redirected));
					p_env.swap(p_redirected);
				}
				else
				{
					const ValueObject* p_next = {};

					if(tp == ystdex::type_id<EnvironmentList>())
					{
						auto& envs(*ystdex::any_cast<EnvironmentList>(&parent));

						p_next = RedirectEnvironmentList(envs.cbegin(),
							envs.cend(), id, cont);
					}
					while(!p_next && bool(cont))
						p_next = ystdex::exchange(cont, Redirector())();
					if(p_next)
					{
						// XXX: Cyclic parent found is not allowed.
						assert(&cur.get() != p_next);
						cur = *p_next;
						search_next = true;
					}
				}
				search_next = false;
			}while(search_next);
			return bool(p_redirected);
		}
		return false;
	}());
	return {p_obj, std::move(p_env)};
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

shared_ptr<Environment>
Context::ShareRecord() const noexcept
{
	return p_record;
}

shared_ptr<Environment>
Context::SwitchEnvironmentUnchecked(const shared_ptr<Environment>& p_env)
	noexcept
{
	assert(p_env);
	return ystdex::exchange(p_record, p_env);
}

void
Context::UnwindCurrent() noexcept
{
	// XXX: The order is significant.
	while(!current.empty())
		current.pop_front();
}

EnvironmentReference
Context::WeakenRecord() const noexcept
{
	return p_record;
}


// NOTE: This is the host type for combiners.
using ContextHandler = function<ReductionStatus(TermNode&, Context&)>;


template<typename... _tParams>
inline shared_ptr<Environment>
AllocateEnvironment(const Environment::allocator_type& a, _tParams&&... args)
{
	return std::allocate_shared<Environment>(a,
		std::forward<_tParams>(args)...);
}
template<typename... _tParams>
inline shared_ptr<Environment>
AllocateEnvironment(Context& ctx, _tParams&&... args)
{
	return Unilang::AllocateEnvironment(ctx.GetBindingsRef().get_allocator(),
		std::forward<_tParams>(args)...);
}
template<typename... _tParams>
inline shared_ptr<Environment>
AllocateEnvironment(TermNode& term, Context& ctx, _tParams&&... args)
{
	const auto a(ctx.GetBindingsRef().get_allocator());

	static_cast<void>(term);
	assert(a == term.get_allocator());
	return Unilang::AllocateEnvironment(a, std::forward<_tParams>(args)...);
}

template<typename... _tParams>
inline shared_ptr<Environment>
SwitchToFreshEnvironment(Context& ctx, _tParams&&... args)
{
	return ctx.SwitchEnvironmentUnchecked(Unilang::AllocateEnvironment(ctx,
		std::forward<_tParams>(args)...));
}


template<typename _type, typename... _tParams>
inline bool
EmplaceLeaf(Environment::BindingMap& m, string_view name, _tParams&&... args)
{
	assert(name.data());

	const auto i(m.find(name));

	if(i == m.end())
	{
		m[string(name)].Value = _type(std::forward<_tParams>(args)...);
		return true;
	}

	auto& nd(i->second);

	nd.Value = _type(std::forward<_tParams>(args)...);
	nd.Subterms.clear();
	return {};
}
template<typename _type, typename... _tParams>
inline bool
EmplaceLeaf(Environment& env, string_view name, _tParams&&... args)
{
	return Unilang::EmplaceLeaf<_type>(env.Bindings, name,
		std::forward<_tParams>(args)...);
}
template<typename _type, typename... _tParams>
inline bool
EmplaceLeaf(Context& ctx, string_view name, _tParams&&... args)
{
	return Unilang::EmplaceLeaf<_type>(ctx.GetRecordRef(), name,
		std::forward<_tParams>(args)...);
}


namespace
{

ReductionStatus
ReduceLeaf(TermNode& term, Context& ctx)
{
	if(const auto p = TermToNamePtr(term))
	{
		auto res(ReductionStatus::Retained);
		const auto id(*p);

		assert(id.data());

		if(!id.empty())
		{
			const char f(id.front());

			if((id.size() > 1 && (f == '#'|| f == '+' || f == '-')
				&& id.find_first_not_of("+-") != string_view::npos)
				|| ystdex::isdigit(f))
				throw InvalidSyntax(ystdex::sfmt<std::string>(id.front()
					!= '#' ? "Unsupported literal prefix found in literal '%s'."
					: "Invalid literal '%s' found.", id.data()));

			auto pr(ctx.Resolve(ctx.GetRecordPtr(), id));

			if(pr.first)
			{
				auto& bound(*pr.first);

				if(const auto p
					= Unilang::TryAccessLeaf<const TermReference>(bound))
				{
					term.Subterms = bound.Subterms;
					term.Value = TermReference(*p);
				}
				else
					term.Value = TermReference(bound, std::move(pr.second));
				res = ReductionStatus::Neutral;
			}
			else
				throw BadIdentifier(id);
		}
		return CheckReducible(res) ? ReduceOnce(term, ctx) : res;
	}
	return ReductionStatus::Retained;
}

ReductionStatus
ReduceCombinedBranch(TermNode& term, Context& ctx)
{
	assert(IsBranchedList(term));

	auto& fm(AccessFirstSubterm(term));
	const auto p_ref_fm(Unilang::TryAccessLeaf<const TermReference>(fm));

	if(p_ref_fm)
	{
		if(const auto p_handler
			= Unilang::TryAccessLeaf<const ContextHandler>(p_ref_fm->get()))
			return (*p_handler)(term, ctx);
	}
	if(const auto p_handler = Unilang::TryAccessTerm<ContextHandler>(fm))
		return (*p_handler)(term, ctx);
	throw
		ListReductionFailure("Invalid object found in the combiner position.");
}

ReductionStatus
ReduceBranch(TermNode& term, Context& ctx)
{
	if(IsBranch(term))
	{
		assert(term.size() != 0);
		if(term.size() == 1)
		{
			// NOTE: The following is necessary to prevent unbounded overflow in
			//	handling recursive subterms.
			auto term_ref(ystdex::ref(term));

			do
			{
				term_ref = AccessFirstSubterm(term_ref);
			}
			while(term_ref.get().size() == 1);
			LiftOther(term, term_ref);
			return ReduceOnce(term, ctx);
		}
		if(IsEmpty(AccessFirstSubterm(term)))
			RemoveHead(term);
		assert(IsBranchedList(term));
		ctx.SetNextTermRef(term);
		ctx.LastStatus = ReductionStatus::Neutral;

		auto& sub(AccessFirstSubterm(term));

		ctx.SetupFront([&](Context& c){
			c.SetNextTermRef(term);
			return ReduceCombinedBranch(term, ctx);
		});
		ctx.SetupFront([&](Context&){
			return ReduceOnce(sub, ctx);
		});
		return ReductionStatus::Partial;
	}
	return ReductionStatus::Retained;
}


inline ReductionStatus
ReduceChildrenOrderedAsync(TNIter, TNIter, Context&);

ReductionStatus
ReduceChildrenOrderedAsyncUnchecked(TNIter first, TNIter last, Context& ctx)
{
	assert(first != last);

	auto& term(*first++);

	ctx.SetupFront([=](Context& c){
		return ReduceChildrenOrderedAsync(first, last, c);
	});
	return ReduceOnce(term, ctx);
}

inline ReductionStatus
ReduceChildrenOrderedAsync(TNIter first, TNIter last, Context& ctx)
{
	return first != last ? ReduceChildrenOrderedAsyncUnchecked(first, last, ctx)
		: ReductionStatus::Neutral;
}

ReductionStatus
ReduceSequenceOrderedAsync(TermNode& term, Context& ctx, TNIter i)
{
	assert(i != term.end());
	if(std::next(i) == term.end())
	{
		LiftOther(term, *i);
		return ReduceOnce(term, ctx);
	}
	ctx.SetupFront([&, i](Context&){
		return ReduceSequenceOrderedAsync(term, ctx, term.erase(i));
	});
	return ReduceOnce(*i, ctx);
}

} // unnamed namespace;

ReductionStatus
ReduceOrdered(TermNode&, Context&);

struct SeparatorTransformer
{
	template<typename _func, class _tTerm, class _fPred>
	[[nodiscard]] TermNode
	operator()(_func trans, _tTerm&& term, const ValueObject& pfx,
		_fPred filter) const
	{
		using it_t = decltype(std::make_move_iterator(term.begin()));

		return AddRange([&](TermNode& res, it_t b, it_t e){
			const auto add([&](TermNode& node, it_t i){
				node.Add(trans(*i));
			});

			if(b != e)
			{
				if(std::next(b) == e)
					add(res, b);
				else
				{
					auto child(Unilang::AsTermNode());

					do
					{
						add(child, b++);
					}
					while(b != e);
					res.Add(std::move(child));
				}
			}
		}, std::forward<_tTerm>(term), pfx, filter);
	}

	template<typename _func, class _tTerm, class _fPred>
	[[nodiscard]] TermNode
	AddRange(_func add_range, _tTerm&& term, const ValueObject& pfx,
		_fPred filter) const
	{
		using it_t = decltype(std::make_move_iterator(term.begin()));
		const auto a(term.get_allocator());
		auto res(Unilang::AsTermNode(std::forward<_tTerm>(term).Value));

		if(IsBranch(term))
		{
			res.Add(Unilang::AsTermNode(pfx));
			ystdex::split(std::make_move_iterator(term.begin()),
				std::make_move_iterator(term.end()), filter,
				[&](it_t b, it_t e){
				add_range(res, b, e);
			});
		}
		return res;
	}

	template<class _tTerm, class _fPred>
	[[nodiscard]] static TermNode
	Process(_tTerm&& term, const ValueObject& pfx, _fPred filter)
	{
		return SeparatorTransformer()([&](_tTerm&& tm) noexcept{
			return std::forward<_tTerm>(tm);
		}, std::forward<_tTerm>(term), pfx, filter);
	}

	template<class _fPred>
	static void
	ReplaceChildren(TermNode& term, const ValueObject& pfx,
		_fPred filter)
	{
		if(std::find_if(term.begin(), term.end(), filter) != term.end())
			term = Process(std::move(term), pfx, filter);
	}
};

ReductionStatus
ReduceOnce(TermNode& term, Context& ctx)
{
	return term.Value.has_value() ? ReduceLeaf(term, ctx)
		: ReduceBranch(term, ctx);
}

ReductionStatus
ReduceOrdered(TermNode& term, Context& ctx)
{
	if(IsBranch(term))
		return ReduceSequenceOrderedAsync(term, ctx, term.begin());
	term.Value = ValueToken::Unspecified;
	return ReductionStatus::Retained;
}


class FormContextHandler
{
public:
	ContextHandler Handler;
	size_t Wrapping;

	template<typename _func, typename = std::enable_if_t<
		!std::is_same<FormContextHandler&, _func&>::value>>
	FormContextHandler(_func&& f, size_t n = 0)
		: Handler(std::forward<_func>(f)), Wrapping(n)
	{}
	FormContextHandler(const FormContextHandler&) = default;
	FormContextHandler(FormContextHandler&&) = default;

	FormContextHandler&
	operator=(const FormContextHandler&) = default;
	FormContextHandler&
	operator=(FormContextHandler&&) = default;

	ReductionStatus
	operator()(TermNode& term, Context& ctx) const
	{
		return CallN(Wrapping, term, ctx);
	}

private:
	ReductionStatus
	CallN(size_t, TermNode&, Context&) const;
};

ReductionStatus
FormContextHandler::CallN(size_t n, TermNode& term, Context& ctx) const
{
	if(n == 0 || term.size() <= 1)
		return Handler(ctx.GetNextTermRef(), ctx);
	ctx.SetupFront([&, n](Context& c){
		c.SetNextTermRef( term);
		return CallN(n - 1, term, c);
	});
	ctx.SetNextTermRef(term);
	assert(!term.empty());
	ReduceChildrenOrderedAsyncUnchecked(std::next(term.begin()), term.end(),
		ctx);
	return ReductionStatus::Partial;
}


enum WrappingKind : decltype(FormContextHandler::Wrapping)
{
	Form = 0,
	Strict = 1
};


template<size_t _vWrapping = Strict, class _tTarget, typename... _tParams>
inline void
RegisterHandler(_tTarget& target, string_view name, _tParams&&... args)
{
	Unilang::EmplaceLeaf<ContextHandler>(target, name,
		FormContextHandler(std::forward<_tParams>(args)..., _vWrapping));
}

template<class _tTarget, typename... _tParams>
inline void
RegisterForm(_tTarget& target, string_view name, _tParams&&... args)
{
	Unilang::RegisterHandler<Form>(target, name,
		std::forward<_tParams>(args)...);
}

template<class _tTarget, typename... _tParams>
inline void
RegisterStrict(_tTarget& target, string_view name, _tParams&&... args)
{
	Unilang::RegisterHandler<>(target, name, std::forward<_tParams>(args)...);
}


namespace
{

ReductionStatus
DefaultEvaluateLeaf(TermNode& term, string_view id)
{
	assert(id.data());
	if(!id.empty())
	{
		const char f(id.front());

		if(ystdex::isdigit(f))
		{
		    int ans(0);

			for(auto p(id.begin()); p != id.end(); ++p)
				if(ystdex::isdigit(*p))
				{
					if(unsigned((ans << 3) + (ans << 1) + *p - '0')
						<= unsigned(INT_MAX))
						ans = (ans << 3) + (ans << 1) + *p - '0';
					else
						throw InvalidSyntax(ystdex::sfmt<std::string>(
							"Value of identifier '%s' is out of the range of"
							" the supported integer.", id.data()));
				}
				else
					throw InvalidSyntax(ystdex::sfmt<std::string>("Literal"
						" postfix is unsupported in identifier '%s'.",
						id.data()));
			term.Value = ans;
		}
		else if(id == "#t")
			term.Value = true;
		else if(id == "#f")
			term.Value = false;
		else if(id == "#inert")
			term.Value = ValueToken::Unspecified;
		else
			return ReductionStatus::Retrying;
		return ReductionStatus::Clean;
	}
	return ReductionStatus::Retrying;
}

TermNode
ParseLeaf(string_view id)
{
	assert(id.data());

	auto term(Unilang::AsTermNode());

	if(!id.empty())
		switch(CategorizeBasicLexeme(id))
		{
		case LexemeCategory::Code:
			id = DeliteralizeUnchecked(id);
			[[fallthrough]];
		case LexemeCategory::Symbol:
			if(CheckReducible(DefaultEvaluateLeaf(term, id)))
				term.Value = TokenValue(id);
			break;
		case LexemeCategory::Data:
			term.Value = string(Deliteralize(id));
			[[fallthrough]];
		default:
			break;
		}
	return term;
}

void
PrintTermNode(std::ostream& os, const TermNode& term, size_t depth = 0,
	size_t n = 0)
{
	const auto print_node_str([&](const TermNode& subterm){
		return ResolveTerm(
			[&](const TermNode& tm) -> pair<lref<const TermNode>, bool>{
			try
			{
				const auto& vo(tm.Value);

				os << [&]() -> string{
					if(const auto p = ystdex::any_cast<string>(&vo))
						return *p;
					if(const auto p = ystdex::any_cast<bool>(&vo))
						return *p ? "#t" : "#f";
					if(const auto p = ystdex::any_cast<int>(&vo))
						return ystdex::sfmt<string>("%d", *p);
					if(const auto p = ystdex::any_cast<ValueToken>(&vo))
						if(*p == ValueToken::Unspecified)
							return "#inert";

					const auto& t(vo.type());

					if(t != typeid(void))
						return "#[" + string(t.name()) + ']';
					throw bad_any_cast();
				}();
				return {tm, true};
			}
			catch(bad_any_cast&)
			{}
			return {tm, false};
		}, subterm);
	});

	if(depth != 0 && n != 0)
		os << ' ';

	const auto pr(print_node_str(term));

	if(!pr.second)
	{
		size_t n(0);

		os << '(';
		for(const auto& nd : pr.first.get())
		{
			try
			{
				PrintTermNode(os, nd, depth + 1, n);
			}
			catch(std::out_of_range&)
			{}
			++n;
		}
		os << ')';
	}
}

} // unnamed namespace;

namespace
{

[[nodiscard, gnu::pure]] bool
ExtractBool(const TermNode& term)
{
	if(const auto p = Unilang::TryAccessReferencedTerm<bool>(term))
		return *p;
	return {};
}

} // unnamed namespace;

namespace Forms
{

inline ReductionStatus
Retain(const TermNode& term) noexcept
{
	assert(IsBranch(term));
	return ReductionStatus::Retained;
}

size_t
RetainN(const TermNode&, size_t = 1);

size_t
RetainN(const TermNode& term, size_t m)
{
	assert(IsBranch(term));

	const auto n(term.size() - 1);

	if(n == m)
		return n;
	throw ArityMismatch(m, n);
}


ReductionStatus
If(TermNode&, Context&);

ReductionStatus
Sequence(TermNode&, Context&);

ReductionStatus
If(TermNode& term, Context& ctx)
{
	Retain(term);

	const auto size(term.size());

	if(size == 3 || size == 4)
	{
		auto i(std::next(term.begin()));

		ctx.SetupFront([&, i](Context&) -> ReductionStatus{
			auto j(i);

			if(!ExtractBool(*j))
				++j;
			if(++j != term.end())
			{
				LiftOther(term, *j);
				return ReduceOnce(term, ctx);
			}
			term.Value = ValueToken::Unspecified;
			return ReductionStatus::Clean;
		});
		return ReduceOnce(*i, ctx);
	}
	else
		throw InvalidSyntax("Syntax error in conditional form.");
}

ReductionStatus
Sequence(TermNode& term, Context& ctx)
{
	Retain(term);
	RemoveHead(term);
	return ReduceOrdered(term, ctx);
}

} // namespace Forms;

namespace
{

class SeparatorPass
{
private:
	using Filter = decltype(std::bind(HasValue<TokenValue>,
		std::placeholders::_1, std::declval<TokenValue&>()));
	using TermStack = stack<lref<TermNode>, vector<lref<TermNode>>>;

	TermNode::allocator_type alloc;
	TokenValue delim{";"};
	TokenValue delim2{","};
	Filter
		filter{std::bind(HasValue<TokenValue>, std::placeholders::_1, delim)};
	Filter
		filter2{std::bind(HasValue<TokenValue>, std::placeholders::_1, delim2)};
	ValueObject pfx{ContextHandler(Forms::Sequence)};
	ValueObject pfx2{ContextHandler(FormContextHandler(ReduceBranchToList, 1))};
	mutable TermStack remained{alloc};

public:
	SeparatorPass(TermNode::allocator_type a)
		: alloc(a)
	{}

	ReductionStatus
	operator()(TermNode& term) const
	{
		assert(remained.empty());
		Transform(term, remained);
		while(!remained.empty())
		{
			const auto term_ref(std::move(remained.top()));

			remained.pop();
			for(auto& tm : term_ref.get())
				Transform(tm, remained);
		}
		return ReductionStatus::Clean;
	}

private:
	void
	Transform(TermNode& term, TermStack& terms) const
	{
		terms.push(term);
		if(std::find_if(term.begin(), term.end(), filter) != term.end())
			term = SeparatorTransformer::Process(std::move(term), pfx, filter);
		if(std::find_if(term.begin(), term.end(), filter2) != term.end())
			term = SeparatorTransformer::Process(std::move(term), pfx2,
				filter2);
	}
};


class Interpreter final
{
private:
	string line{};
	shared_ptr<Environment> p_ground{};

public:
	bool Echo = std::getenv("ECHO");
	Context Root{*pmr::new_delete_resource()};
	SeparatorPass Preprocess{pmr::new_delete_resource()};

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

	bool
	SaveGround();

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
	PrintTermNode(std::cout, term);
	std::cout << std::endl;
}

bool
Interpreter::Process()
{
	if(line == "exit")
		return {};
	else if(!line.empty())
		try
		{
			struct Guard final
			{
				Context& C;

				~Guard()
				{
					C.UnwindCurrent();
				}
			} gd{Root};
			auto term(Read(line));

			Evaluate(term);
			if(Echo)
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

	std::for_each(unit.begin(), unit.end(), ystdex::ref(parse));

	const auto& parse_result(parse.GetResult());
	TermNode term{};

	if(ReduceSyntax(term, parse_result.cbegin(), parse_result.cend(), ParseLeaf)
		!= parse_result.cend())
		throw UnilangException("Redundant ')' found.");
	Preprocess(term);
	return term;
}

void
Interpreter::Run()
{
	while(WaitForLine() && Process())
		;
}

bool
Interpreter::SaveGround()
{
	if(!p_ground)
	{
		auto& cs(Root);

		p_ground = Unilang::SwitchToFreshEnvironment(cs,
			ValueObject(cs.WeakenRecord()));
		return true;
	}
	return {};
}

std::istream&
Interpreter::WaitForLine()
{
	using namespace std;

	cout << "> ";
	return getline(cin, line);
}


void
LoadFunctions(Interpreter& intp)
{
	auto& ctx(intp.Root);
	using namespace Forms;

	RegisterForm(ctx, "$if", If);
	RegisterForm(ctx, "$sequence", Sequence);
	RegisterStrict(ctx, "display", [&](TermNode& term, Context&){
		RetainN(term);
		LiftOther(term, *std::next(term.begin()));
		PrintTermNode(std::cout, term);
		term.Value = ValueToken::Unspecified;
		return ReductionStatus::Clean;
	});
	RegisterStrict(ctx, "newline", [&](TermNode& term, Context&){
		RetainN(term, 0);
		std::cout << std::endl;
		term.Value = ValueToken::Unspecified;
		return ReductionStatus::Clean;
	});
	intp.SaveGround();
}

#define APP_NAME "Unilang demo"
#define APP_VER "0.1.6"
#define APP_PLATFORM "[C++11] + YBase"
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
	LoadFunctions(intp);
	cout << "Initialization finished." << endl;
	cout << "Type \"exit\" to exit." << endl << endl;
	intp.Run();
}

