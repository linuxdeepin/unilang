// © 2020 Uniontech Software Technology Co.,Ltd.

#include <ydef.h> // for byte, ystdex::size_t, std::pair, yforward,
//	std::declval, std::swap;
#include <ystdex/utility.hpp> // for ystdex::lref, ystdex::ref,
//	ystdex::exchange, ystdex::as_const;
#include <ystdex/any.h> // for ystdex::any, ystdex::bad_any_cast;
#include <ystdex/typeinfo.h> // for ystdex::type_info;
#include <ystdex/functional.hpp> // for ystdex::function, ystdex::expand_proxy,
//	ystdex::compose_n, ystdex::less, ystdex::expanded_function, ystdex::invoke_nonvoid;
#include <ystdex/memory.hpp> // for ystdex::make_shared, std::shared_ptr,
//	std::weak_ptr, ystdex::share_move, std::allocator_arg_t,
//	ystdex::make_obj_using_allocator;
#include <ystdex/string_view.hpp> // for ystdex::string_view;
#include <ystdex/memory_resource.h> // for ystdex::pmr and
//	complete ystdex::pmr::polymorphic_allocator;
#include <ystdex/string.hpp> // for ystdex::basic_string, std::string,
//	ystdex::sfmt, std::getline, ystdex::begins_with;
#include <forward_list> // for std::forward_list;
#include <list> // for std::list;
#include <ystdex/map.hpp> // for ystdex::map;
#include <cctype> // for std::isgraph;
#include <vector> // for std::vector;
#include <deque> // for std::deque;
#include <stack> // for std::stack;
#include <sstream> // for std::basic_ostringstream, std::ostream,
//	std::streamsize;
#include <cassert> // for assert;
#include <exception> // for std::runtime_error, std::throw_with_nested;
#include <ystdex/type_traits.hpp> // for ystdex::enable_if_t, ystdex::decay_t,
//	std::is_same, std::is_convertible;
#include <ystdex/type_op.hpp> // for ystdex::cond_or_t, ystdex::false_,
//	ystdex::not_;
#include <ystdex/operators.hpp> // for ystdex::equality_comparable;
#include <iterator> // for std::next, std::prev, std::make_move_iterator;
#include <ystdex/container.hpp> // for ystdex::try_emplace,
//	ystdex::try_emplace_hint, ystdex::insert_or_assign;
#include <ystdex/cctype.h> // for ystdex::isdigit;
#include <ystdex/algorithm.hpp> // for ystdex::split;
#include <algorithm> // for std::for_each, std::find_if;
#include <ystdex/typeinfo.h> // for ystdex::type_id;
#include <ystdex/scope_guard.hpp> // for ystdex::guard;
#include <cstdlib> // for std::getenv;
#include <ystdex/examiner.hpp> // for ystdex::examiners;
#include <iostream> // for std::cout, std::cerr, std::endl, std::cin;
#include <YSLib/Core/YModules.h>
#include YFM_YSLib_Core_YObject // for YSLib::ValueObject, YSLib::HoldSame;

namespace Unilang
{

using ystdex::byte;
using ystdex::size_t;

using ystdex::lref;

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
using map = ystdex::map<_tKey, _tMapped, _fComp, _tAlloc>;

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


using YSLib::ValueObject;
namespace any_ops = YSLib::any_ops;
using YSLib::any;
using YSLib::bad_any_cast;
using YSLib::in_place_type;


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

	const auto add = [&](string&& arg){
		lexemes.push_back(yforward(arg));
	};
	bool got_delim(UpdateBack(c));
	const auto len(buffer.length());

	assert(!(lexemes.empty() && update_current));
	if(len > 0)
	{
		if(len == 1)
		{
			const auto update_c([&](char c){
				if(update_current)
					lexemes.back() += c;
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
			lexemes.back() += buffer.substr(0, len);
		else
			add(std::move(buffer));
		buffer.clear();
	}
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


class ListTypeError : public TypeError
{
public:
	using TypeError::TypeError;
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


constexpr const struct NoContainerTag{} NoContainer{};


class TermNode final
{
private:
	template<typename... _tParams>
	using enable_value_constructible_t = ystdex::enable_if_t<
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
	inline
	TermNode(NoContainerTag, _tParams&&... args)
		: Value(yforward(args)...)
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
	TermNode(std::allocator_arg_t, allocator_type a, NoContainerTag,
		_tParams&&... args)
		: Subterms(a), Value(yforward(args)...)
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
		return Value || !empty();
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
	iterator
	erase(const_iterator first, const_iterator last)
	{
		return Subterms.erase(first, last);
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
	return !(term.empty() || term.Value);
}

[[nodiscard, gnu::pure]] inline bool
IsEmpty(const TermNode& term) noexcept
{
	return !term;
}

[[nodiscard, gnu::pure]] inline bool
IsExtendedList(const TermNode& term) noexcept
{
	return !(term.empty() && term.Value);
}

[[nodiscard, gnu::pure]] inline bool
IsLeaf(const TermNode& term) noexcept
{
	return term.empty();
}

[[nodiscard, gnu::pure]] inline bool
IsList(const TermNode& term) noexcept
{
	return !term.Value;
}

template<typename _type>
[[nodiscard, gnu::pure]] inline _type&
Access(TermNode& term)
{
	return term.Value.Access<_type&>();
}
template<typename _type>
[[nodiscard, gnu::pure]] inline const _type&
Access(const TermNode& term)
{
	return term.Value.Access<const _type&>();
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

[[nodiscard]] inline shared_ptr<TermNode>
ShareMoveTerm(TermNode& term)
{
	return ystdex::share_move(term.get_allocator(), term);
}
[[nodiscard]] inline shared_ptr<TermNode>
ShareMoveTerm(TermNode&& term)
{
	return ystdex::share_move(term.get_allocator(), term);
}

inline void
RemoveHead(TermNode& term) noexcept
{
	assert(!term.empty());
	term.erase(term.begin());
}

template<typename... _tParam, typename... _tParams>
[[nodiscard, gnu::pure]] inline
ystdex::enable_if_t<ystdex::not_<ystdex::cond_or_t<ystdex::bool_<
	(sizeof...(_tParams) >= 1)>, ystdex::false_, std::is_convertible,
	ystdex::decay_t<_tParams>..., TermNode::allocator_type>>::value, TermNode>
AsTermNode(_tParams&&... args)
{
	return TermNode(NoContainer, yforward(args)...);
}
template<typename... _tParams>
[[nodiscard, gnu::pure]] YB_PURE inline TermNode
AsTermNode(TermNode::allocator_type a, _tParams&&... args)
{
	return TermNode(std::allocator_arg, a, NoContainer, yforward(args)...);
}


template<typename _type>
[[nodiscard, gnu::pure]] inline bool
HasValue(const TermNode& term, const _type& x)
{
	return term.Value == x;
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

[[noreturn]] void
ThrowListTypeErrorForInvalidType(const ystdex::type_info&, const TermNode&,
	bool);

template<typename _type>
[[nodiscard, gnu::pure]] inline _type*
TryAccessLeaf(TermNode& term)
{
	return term.Value.type() == ystdex::type_id<_type>()
		? std::addressof(term.Value.GetObject<_type>()) : nullptr; 
}
template<typename _type>
[[nodiscard, gnu::pure]] inline const _type*
TryAccessLeaf(const TermNode& term)
{
	return term.Value.type() == ystdex::type_id<_type>()
		? std::addressof(term.Value.GetObject<_type>()) : nullptr; 
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

void
ThrowListTypeErrorForInvalidType(const ystdex::type_info& tp,
	const TermNode& term, bool is_ref)
{
	throw ListTypeError(ystdex::sfmt("Expected a value of type '%s', got a list"
		" '%s'.", tp.name(),
		TermToStringWithReferenceMark(term, is_ref).c_str()));
}


class Environment;

using AnchorPtr = shared_ptr<const void>;


class EnvironmentReference final
	: private ystdex::equality_comparable<EnvironmentReference>
{
private:
	weak_ptr<Environment> p_weak{};
	AnchorPtr p_anchor{};

public:
	EnvironmentReference() = default;
	EnvironmentReference(const shared_ptr<Environment>&) noexcept;
	template<typename _tParam1, typename _tParam2>
	EnvironmentReference(_tParam1&& arg1, _tParam2&& arg2) noexcept
		: p_weak(yforward(arg1)), p_anchor(yforward(arg2))
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
class TermReference final : private ystdex::equality_comparable<TermReference>
{
private:
	lref<TermNode> term_ref;
	EnvironmentReference r_env;

public:
	template<typename _tParam, typename... _tParams>
	inline
	TermReference(TermNode& term, _tParam&& arg, _tParams&&... args) noexcept
		: term_ref(term), r_env(yforward(arg), yforward(args)...)
	{}
	TermReference(const TermReference&) = default;

	TermReference&
	operator=(const TermReference&) = default;

	[[nodiscard, gnu::pure]] friend bool
	operator==(const TermReference& x, const TermReference& y) noexcept
	{
		return &x.term_ref.get() == &y.term_ref.get();
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
	-> decltype(ystdex::expand_proxy<void(_tTerm&&,
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

template<typename _type, class _tTerm>
void
CheckRegular(_tTerm& term, bool has_ref)
{
	if(YB_UNLIKELY(IsList(term)))
		ThrowListTypeErrorForInvalidType(ystdex::type_id<_type>(), term,
			has_ref);
}

template<typename _type, class _tTerm>
[[nodiscard, gnu::pure]] inline auto
AccessRegular(_tTerm& term, bool has_ref)
	-> decltype(Unilang::Access<_type>(term))
{
	Unilang::CheckRegular<_type>(term, has_ref);
	return Unilang::Access<_type>(term);
}

template<typename _type, class _tTerm>
[[nodiscard, gnu::pure]] inline auto
ResolveRegular(_tTerm& term) -> decltype(Unilang::Access<_type>(term))
{
	return Unilang::ResolveTerm([&](_tTerm& nd, bool has_ref)
		-> yimpl(decltype(Unilang::Access<_type>(term))){
		return Unilang::AccessRegular<_type>(nd, has_ref);
	}, term);
}


struct ReferenceTermOp
{
	template<typename _type>
	auto
	operator()(_type&& term) const
		ynoexcept_spec(Unilang::ReferenceTerm(yforward(term)))
		-> decltype(Unilang::ReferenceTerm(yforward(term)))
	{
		return Unilang::ReferenceTerm(yforward(term));
	}
};

template<typename _func>
[[gnu::const]] auto
ComposeReferencedTermOp(_func f)
	-> decltype(ystdex::compose_n(f, ReferenceTermOp()))
{
	return ystdex::compose_n(f, ReferenceTermOp());
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


ReductionStatus
RegularizeTerm(TermNode&, ReductionStatus) noexcept;

ReductionStatus
RegularizeTerm(TermNode& term, ReductionStatus status) noexcept
{
	if(status == ReductionStatus::Clean)
		term.Subterms.clear();
	return status;
}


void
LiftOther(TermNode&, TermNode&);

void
LiftOtherOrCopy(TermNode&, TermNode&, bool);

void
LiftOther(TermNode& term, TermNode& tm)
{
	assert(&term != &tm);

	const auto t(std::move(term.Subterms));

	term.Subterms = std::move(tm.Subterms);
	term.Value = std::move(tm.Value);
}

void
LiftOtherOrCopy(TermNode& term, TermNode& tm, bool move)
{
	if(move)
		LiftOther(term, tm);
	else
	{
		term.Subterms = tm.Subterms;
		term.Value = tm.Value;
	}
}


void
LiftToReturn(TermNode&);

inline void
LiftSubtermsToReturn(TermNode& term)
{
	std::for_each(term.begin(), term.end(), LiftToReturn);
}

ReductionStatus
ReduceBranchToList(TermNode&) noexcept;

ReductionStatus
ReduceBranchToListValue(TermNode& term) noexcept;

inline ReductionStatus
ReduceForLiftedResult(TermNode& term)
{
	LiftToReturn(term);
	return ReductionStatus::Retained;
}

void
LiftToReturn(TermNode& term)
{
	if(const auto p = Unilang::TryAccessLeaf<const TermReference>(term))
		term = p->get();
}

ReductionStatus
ReduceBranchToList(TermNode& term) noexcept
{
	RemoveHead(term);
	return ReductionStatus::Retained;
}

ReductionStatus
ReduceBranchToListValue(TermNode& term) noexcept
{
	RemoveHead(term);
	LiftSubtermsToReturn(term);
	return ReductionStatus::Retained;
}


using EnvironmentList = vector<ValueObject>;


class Environment final : private ystdex::equality_comparable<Environment>
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

	[[nodiscard, gnu::pure]] friend bool
	operator==(const Environment& x, const Environment& y) noexcept
	{
		return &x == &y;
	}

	[[nodiscard, gnu::pure]] const AnchorPtr&
	GetAnchorPtr() const noexcept
	{
		return p_anchor;
	}

	template<typename _tKey, typename... _tParams>
	inline ystdex::enable_if_inconvertible_t<_tKey&&,
		BindingMap::const_iterator, bool>
	AddValue(_tKey&& k, _tParams&&... args)
	{
		return ystdex::try_emplace(Bindings, yforward(k), NoContainer,
			yforward(args)...).second;
	}
	template<typename _tKey, typename... _tParams>
	inline bool
	AddValue(BindingMap::const_iterator hint, _tKey&& k, _tParams&&... args)
	{
		return ystdex::try_emplace_hint(Bindings, hint, yforward(k),
			NoContainer, yforward(args)...).second;
	}

	template<typename _tKey, class _tNode>
	TermNode&
	Bind(_tKey&& k, _tNode&& tm)
	{
		return ystdex::insert_or_assign(Bindings, yforward(k),
			yforward(tm)).first->second;
	}

	static void
	CheckParent(const ValueObject&);

	static Environment&
	EnsureValid(const shared_ptr<Environment>&);

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

Environment&
Environment::EnsureValid(const shared_ptr<Environment>& p_env)
{
	if(p_env)
		return *p_env;
	throw std::invalid_argument("Invalid environment found.");
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


class Context;

using ReducerFunctionType = ReductionStatus(Context&);

using Reducer = ystdex::expanded_function<ReducerFunctionType>;

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
		return SetupFront(yforward(args)...);
	}

	template<typename... _tParams>
	inline void
	SetupFront(_tParams&&... args)
	{
		current.emplace_front(yforward(args)...);
	}

	[[nodiscard, gnu::pure]] shared_ptr<Environment>
	ShareRecord() const noexcept;

	shared_ptr<Environment>
	SwitchEnvironment(const shared_ptr<Environment>&);

	shared_ptr<Environment>
	SwitchEnvironmentUnchecked(const shared_ptr<Environment>&) noexcept;

	void
	UnwindCurrent() noexcept;

	[[nodiscard, gnu::pure]] EnvironmentReference
	WeakenRecord() const noexcept;

	[[nodiscard, gnu::pure]] pmr::polymorphic_allocator<byte>
	get_allocator() const noexcept
	{
		return pmr::polymorphic_allocator<byte>(&memory_rsrc.get());
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
	try
	{
		LastStatus = TailAction(*this);
	}
	catch(bad_any_cast&)
	{
		std::throw_with_nested(TypeError("Mismatched type found."));
	}
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
					p_redirected
						= parent.GetObject<EnvironmentReference>().Lock();
					assert(bool(p_redirected));
					p_env.swap(p_redirected);
				}
				else if(tp == ystdex::type_id<shared_ptr<Environment>>())
				{
					p_redirected = parent.GetObject<shared_ptr<Environment>>();
					assert(bool(p_redirected));
					p_env.swap(p_redirected);
				}
				else
				{
					const ValueObject* p_next = {};

					if(tp == ystdex::type_id<EnvironmentList>())
					{
						auto& envs(parent.GetObject<EnvironmentList>());

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
Context::SwitchEnvironment(const shared_ptr<Environment>& p_env)
{
	if(p_env)
		return SwitchEnvironmentUnchecked(p_env);
	throw std::invalid_argument("Invalid environment record pointer found.");
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
using ContextHandler
	= ystdex::expanded_function<ReductionStatus(TermNode&, Context&)>;


template<typename... _tParams>
inline shared_ptr<Environment>
AllocateEnvironment(const Environment::allocator_type& a, _tParams&&... args)
{
	return std::allocate_shared<Environment>(a, yforward(args)...);
}
template<typename... _tParams>
inline shared_ptr<Environment>
AllocateEnvironment(Context& ctx, _tParams&&... args)
{
	return Unilang::AllocateEnvironment(ctx.GetBindingsRef().get_allocator(),
		yforward(args)...);
}
template<typename... _tParams>
inline shared_ptr<Environment>
AllocateEnvironment(TermNode& term, Context& ctx, _tParams&&... args)
{
	const auto a(ctx.GetBindingsRef().get_allocator());

	static_cast<void>(term);
	assert(a == term.get_allocator());
	return Unilang::AllocateEnvironment(a, yforward(args)...);
}

template<typename... _tParams>
inline shared_ptr<Environment>
SwitchToFreshEnvironment(Context& ctx, _tParams&&... args)
{
	return ctx.SwitchEnvironmentUnchecked(Unilang::AllocateEnvironment(ctx,
		yforward(args)...));
}


template<typename _type, typename... _tParams>
inline bool
EmplaceLeaf(Environment::BindingMap& m, string_view name, _tParams&&... args)
{
	assert(name.data());

	const auto i(m.find(name));

	if(i == m.end())
	{
		m[string(name)].Value = _type(yforward(args)...);
		return true;
	}

	auto& nd(i->second);

	nd.Value = _type(yforward(args)...);
	nd.Subterms.clear();
	return {};
}
template<typename _type, typename... _tParams>
inline bool
EmplaceLeaf(Environment& env, string_view name, _tParams&&... args)
{
	return Unilang::EmplaceLeaf<_type>(env.Bindings, name, yforward(args)...);
}
template<typename _type, typename... _tParams>
inline bool
EmplaceLeaf(Context& ctx, string_view name, _tParams&&... args)
{
	return Unilang::EmplaceLeaf<_type>(ctx.GetRecordRef(), name, yforward(args)...);
}


[[nodiscard]] pair<shared_ptr<Environment>, bool>
ResolveEnvironment(const ValueObject&);
[[nodiscard]] pair<shared_ptr<Environment>, bool>
ResolveEnvironment(const TermNode&);

pair<shared_ptr<Environment>, bool>
ResolveEnvironment(const ValueObject& vo)
{
	if(const auto p = vo.AccessPtr<const EnvironmentReference>())
		return {p->Lock(), {}};
	if(const auto p = vo.AccessPtr<const shared_ptr<Environment>>())
		return {*p, true};
	throw TypeError(
		ystdex::sfmt("Invalid environment type '%s' found.", vo.type().name()));
}
pair<shared_ptr<Environment>, bool>
ResolveEnvironment(const TermNode& term)
{
	return ResolveTerm([&](const TermNode& nd, bool has_ref){
		if(!IsExtendedList(nd))
			return ResolveEnvironment(nd.Value);
		throw ListTypeError(
			ystdex::sfmt("Invalid environment formed from list '%s' found.",
			TermToStringWithReferenceMark(nd, has_ref).c_str()));
	}, term);
}


struct EnvironmentSwitcher
{
	lref<Context> ContextRef;
	mutable shared_ptr<Environment> SavedPtr;

	EnvironmentSwitcher(Context& ctx,
		shared_ptr<Environment>&& p_saved = {})
		: ContextRef(ctx), SavedPtr(std::move(p_saved))
	{}
	EnvironmentSwitcher(EnvironmentSwitcher&&) = default;

	EnvironmentSwitcher&
	operator=(EnvironmentSwitcher&&) = default;

	void
	operator()() const ynothrowv
	{
		if(SavedPtr)
			ContextRef.get().SwitchEnvironmentUnchecked(std::move(SavedPtr));
	}
};


// NOTE: The collection of values of unit types.
enum class ValueToken
{
	Unspecified
};


class Continuation
{
public:
	using allocator_type
		= decltype(std::declval<const Context&>().get_allocator());

	ContextHandler Handler;

	template<typename _func, typename
		= ystdex::exclude_self_t<Continuation, _func>>
	inline
	Continuation(_func&& handler, allocator_type a)
		: Handler(ystdex::make_obj_using_allocator<ContextHandler>(a,
		yforward(handler)))
	{}
	template<typename _func, typename
		= ystdex::exclude_self_t<Continuation, _func>>
	inline
	Continuation(_func&& handler, const Context& ctx)
		: Continuation(yforward(handler), ctx.get_allocator())
	{}
	Continuation(const Continuation& cont, allocator_type a)
		: Handler(ystdex::make_obj_using_allocator<ContextHandler>(a,
		cont.Handler))
	{}
	Continuation(Continuation&& cont, allocator_type a)
		: Handler(ystdex::make_obj_using_allocator<ContextHandler>(a,
		std::move(cont.Handler)))
	{}
	Continuation(const Continuation&) = default;
	Continuation(Continuation&&) = default;

	Continuation&
	operator=(const Continuation&) = default;
	Continuation&
	operator=(Continuation&&) = default;

	[[nodiscard, gnu::const]] friend bool
	operator==(const Continuation& x, const Continuation& y) noexcept
	{
		return ystdex::ref_eq<>()(x, y);
	}

	ReductionStatus
	operator()(Context& ctx) const
	{
		return Handler(ctx.GetNextTermRef(), ctx);
	}
};


[[noreturn]] void
ThrowInsufficientTermsError();


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
		}, yforward(term), pfx, filter);
	}

	template<typename _func, class _tTerm, class _fPred>
	[[nodiscard]] TermNode
	AddRange(_func add_range, _tTerm&& term, const ValueObject& pfx,
		_fPred filter) const
	{
		using it_t = decltype(std::make_move_iterator(term.begin()));
		const auto a(term.get_allocator());
		auto res(Unilang::AsTermNode(yforward(term).Value));

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
			return yforward(tm);
		}, yforward(term), pfx, filter);
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


class FormContextHandler
{
public:
	ContextHandler Handler;
	size_t Wrapping;

	template<typename _func, typename = ystdex::enable_if_t<
		!std::is_same<FormContextHandler&, _func&>::value>>
	FormContextHandler(_func&& f, size_t n = 0)
		: Handler(yforward(f)), Wrapping(n)
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
		FormContextHandler(yforward(args)..., _vWrapping));
}

template<class _tTarget, typename... _tParams>
inline void
RegisterForm(_tTarget& target, string_view name, _tParams&&... args)
{
	Unilang::RegisterHandler<Form>(target, name,
		yforward(args)...);
}

template<class _tTarget, typename... _tParams>
inline void
RegisterStrict(_tTarget& target, string_view name, _tParams&&... args)
{
	Unilang::RegisterHandler<>(target, name, yforward(args)...);
}


using EnvironmentGuard = ystdex::guard<EnvironmentSwitcher>;


inline ReductionStatus
ReduceOnceLifted(TermNode& term, Context& ctx, TermNode& tm)
{
	LiftOther(term, tm);
	return ReduceOnce(term, ctx);
}

ReductionStatus
ReduceOrdered(TermNode&, Context&);

inline ReductionStatus
ReduceReturnUnspecified(TermNode& term) noexcept
{
	term.Value = ValueToken::Unspecified;
	return ReductionStatus::Clean;
}

void
BindParameter(const shared_ptr<Environment>&, const TermNode&, TermNode&);

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

template<typename... _tParams>
ReductionStatus
CombinerReturnThunk(const ContextHandler& h, TermNode& term, Context& ctx,
	_tParams&&... args)
{
	static_assert(sizeof...(args) < 2, "Unsupported owner arguments found.");

	// TODO: Implement TCO.
	ctx.SetNextTermRef(term);
	ctx.SetupFront(std::bind([&](const _tParams&...){
		return RegularizeTerm(term, ctx.LastStatus);
	}, std::move(args)...));
	ctx.SetupFront(Continuation(std::ref(h), ctx));
	return ReductionStatus::Partial;
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
			return CombinerReturnThunk(*p_handler, term, ctx);
	}
	if(const auto p_handler = Unilang::TryAccessTerm<ContextHandler>(fm))
	{
		// TODO: Implement TCO.
		auto p(ystdex::share_move(ctx.get_allocator(), *p_handler));

		return CombinerReturnThunk(*p, term, ctx, std::move(p));
	}
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
			return ReduceOnceLifted(term, ctx, term_ref);
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
		ctx.SetupFront([&]{
			return ReduceOnce(sub, ctx);
		});
		return ReductionStatus::Partial;
	}
	return ReductionStatus::Retained;
}


template<typename _fNext>
inline ReductionStatus
ReduceSubsequent(TermNode& term, Context& ctx, _fNext&& next)
{
	ctx.SetupFront(yforward(next));
	return ReduceOnce(term, ctx);
}


inline ReductionStatus
ReduceChildrenOrderedAsync(TNIter, TNIter, Context&);

ReductionStatus
ReduceChildrenOrderedAsyncUnchecked(TNIter first, TNIter last, Context& ctx)
{
	assert(first != last);

	auto& term(*first++);

	return ReduceSubsequent(term, ctx, [=](Context& c){
		return ReduceChildrenOrderedAsync(first, last, c);
	});
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
		return ReduceOnceLifted(term, ctx, *i);
	ctx.SetupFront([&, i]{
		return ReduceSequenceOrderedAsync(term, ctx, term.erase(i));
	});
	return ReduceOnce(*i, ctx);
}


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
					if(const auto p = vo.AccessPtr<string>())
						return ystdex::quote(*p);
					if(const auto p = vo.AccessPtr<TokenValue>())
						return *p;
					if(const auto p = vo.AccessPtr<bool>())
						return *p ? "#t" : "#f";
					if(const auto p = vo.AccessPtr<int>())
						return ystdex::sfmt<string>("%d", *p);
					if(const auto p = vo.AccessPtr<ValueToken>())
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


template<typename _func>
auto
CheckSymbol(string_view n, _func f) -> decltype(f())
{
	if(CategorizeBasicLexeme(n) == LexemeCategory::Symbol)
		return f();
	throw ParameterMismatch(ystdex::sfmt(
		"Invalid token '%s' found for symbol parameter.", n.data()));
}

template<typename _func>
auto
CheckParameterLeafToken(string_view n, _func f) -> decltype(f())
{
	if(n != "#ignore")
		CheckSymbol(n, f);
}


using Action = function<void()>;

struct BindParameterObject
{
	lref<const EnvironmentReference> Referenced;

	BindParameterObject(const EnvironmentReference& r_env)
		: Referenced(r_env)
	{}

	template<typename _fCopy>
	void
	operator()(TermNode& o, _fCopy cp)
		const
	{
		if(const auto p = Unilang::TryAccessLeaf<TermReference>(o))
			cp(p->get());
		else
			cp(o);
	}
};


template<typename _fBindTrailing, typename _fBindValue>
class GParameterMatcher
{
public:
	_fBindTrailing BindTrailing;
	_fBindValue BindValue;

private:
	mutable Action act{};

public:
	template<class _type, class _type2>
	GParameterMatcher(_type&& arg, _type2&& arg2)
		: BindTrailing(yforward(arg)),
		BindValue(yforward(arg2))
	{}

	void
	operator()(const TermNode& t, TermNode& o,
		const EnvironmentReference& r_env) const
	{
		Match(t, o, r_env);
		while(act)
		{
			const auto a(std::move(act));

			a();
		}
	}

private:
	void
	Match(const TermNode& t, TermNode& o, const EnvironmentReference& r_env)
		const
	{
		if(IsList(t))
		{
			if(IsBranch(t))
			{
				const auto n_p(t.size());
				auto last(t.end());

				if(n_p > 0)
				{
					const auto& back(*std::prev(last));

					if(IsLeaf(back))
					{
						if(const auto p
							= Unilang::TryAccessLeaf<TokenValue>(back))
						{
							if(!p->empty() && p->front() == '.')
								--last;
						}
						else if(!IsList(back))
							throw ParameterMismatch(ystdex::sfmt(
								"Invalid term '%s' found for symbol parameter.",
								TermToString(back).c_str()));
					}
				}
				ResolveTerm([&, n_p](TermNode& nd,
					ResolvedTermReferencePtr p_ref){
					const bool ellipsis(last != t.end());
					const auto n_o(nd.size());

					if(n_p == n_o || (ellipsis && n_o >= n_p - 1))
						MatchSubterms(t.begin(), last, nd.begin(), nd.end(),
							p_ref ? p_ref->GetEnvironmentReference() : r_env,
							ellipsis);
					else if(!ellipsis)
						throw ArityMismatch(n_p, n_o);
					else
						ThrowInsufficientTermsError();
				}, o);
			}
			else
				ResolveTerm([&](const TermNode& nd, bool has_ref){
					if(nd)
						throw ParameterMismatch(ystdex::sfmt("Invalid nonempty"
							" operand value '%s' found for empty list"
							" parameter.", TermToStringWithReferenceMark(nd,
							has_ref).c_str()));
				}, o);
		}
		else
		{
			const auto& tp(t.Value.type());
		
			if(tp == ystdex::type_id<TermReference>())
				ystdex::update_thunk(act, [&]{
					Match(t.Value.GetObject<TermReference>().get(), o, r_env);
				});
			else if(tp == ystdex::type_id<TokenValue>())
				BindValue(t.Value.GetObject<TokenValue>(), o, r_env);
			else
				throw ParameterMismatch(ystdex::sfmt("Invalid parameter value"
					" '%s' found.", TermToString(t).c_str()));
		}
	}

	void
	MatchSubterms(TNCIter i, TNCIter last, TNIter j, TNIter o_last,
		const EnvironmentReference& r_env, bool ellipsis) const
	{
		if(i != last)
		{
			ystdex::update_thunk(act, [=, &r_env]{
				return MatchSubterms(std::next(i), last, std::next(j), o_last,
					r_env, ellipsis);
			});
			assert(j != o_last);
			Match(*i, *j, r_env);
		}
		else if(ellipsis)
		{
			const auto& lastv(last->Value);

			assert(lastv.type() == ystdex::type_id<TokenValue>());
			BindTrailing(j, o_last, lastv.GetObject<TokenValue>(), r_env);
		}
	}
};

template<typename _fBindTrailing, typename _fBindValue>
[[nodiscard]] inline GParameterMatcher<_fBindTrailing, _fBindValue>
MakeParameterMatcher(_fBindTrailing bind_trailing_seq, _fBindValue bind_value)
{
	return GParameterMatcher<_fBindTrailing, _fBindValue>(
		std::move(bind_trailing_seq), std::move(bind_value));
}

} // unnamed namespace;

void
ThrowInsufficientTermsError()
{
	throw ParameterMismatch("Insufficient terms found for list parameter.");
}


ReductionStatus
ReduceOnce(TermNode& term, Context& ctx)
{
	return term.Value ? ReduceLeaf(term, ctx)
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


void
BindParameter(const shared_ptr<Environment>& p_env, const TermNode& t,
	TermNode& o)
{
	auto& env(*p_env);

	MakeParameterMatcher([&](TNIter first, TNIter last, string_view id,
		const EnvironmentReference& r_env){
		assert(ystdex::begins_with(id, "."));
		id.remove_prefix(1);
		if(!id.empty())
		{
			if(!id.empty())
			{
				TermNode::Container con(t.get_allocator());

				for(; first != last; ++first)
					BindParameterObject{r_env}(*first,
						[&](const TermNode& tm){
						con.emplace_back(tm.Subterms, tm.Value);
					});
				env.Bind(id, TermNode(std::move(con)));
			}
		}
	}, [&](const TokenValue& n, TermNode& b, const EnvironmentReference& r_env){
		CheckParameterLeafToken(n, [&]{
			if(!n.empty())
			{
				string_view id(n);

				if(!id.empty())
					BindParameterObject{r_env}(b,
						[&](const TermNode& tm){
						env.Bind(id, tm);
					});
			}
		});
	})(t, o, p_env);
}


namespace
{

[[nodiscard, gnu::pure]] bool
ExtractBool(const TermNode& term)
{
	if(const auto p = Unilang::TryAccessReferencedTerm<bool>(term))
		return *p;
	return true;
}


inline ReductionStatus
MoveGuard(EnvironmentGuard& gd, Context& ctx) ynothrow
{
	const auto egd(std::move(gd));

	return ctx.LastStatus;
}


[[noreturn]] inline void
ThrowInsufficientTermsErrorFor(InvalidSyntax&& e)
{
	try
	{
		ThrowInsufficientTermsError();
	}
	catch(...)
	{
		std::throw_with_nested(std::move(e));
	}
}


[[nodiscard, gnu::pure]] inline bool
IsIgnore(const TokenValue& s) noexcept
{
	return s == "#ignore";
}

[[gnu::nonnull(2)]] void
CheckVauSymbol(const TokenValue& s, const char* target, bool is_valid)
{
	if(!is_valid)
		throw InvalidSyntax(ystdex::sfmt("Token '%s' is not a symbol or"
			" '#ignore' expected for %s.", s.data(), target));
}

[[noreturn, gnu::nonnull(2)]] void
ThrowInvalidSymbolType(const TermNode& term, const char* n)
{
	throw InvalidSyntax(ystdex::sfmt("Invalid %s type '%s' found.", n,
		term.Value.type().name()));
}

[[nodiscard, gnu::pure]] string
CheckEnvFormal(const TermNode& eterm)
{
	const auto& term(ReferenceTerm(eterm));

	if(const auto p = TermToNamePtr(term))
	{
		if(!IsIgnore(*p))
		{
			CheckVauSymbol(*p, "environment formal parameter",
				CategorizeBasicLexeme(*p) == LexemeCategory::Symbol);
			return *p;
		}
	}
	else
		ThrowInvalidSymbolType(term, "environment formal parameter");
	return {};
}


template<typename _fNext>
ReductionStatus
RelayForEvalOrDirect(Context& ctx, TermNode& term, EnvironmentGuard&& gd,
	bool, _fNext&& next)
{
	// TODO: Implement TCO.
	auto act(std::bind(MoveGuard, std::move(gd), std::placeholders::_1));

	Continuation cont([&]{
		return ReduceForLiftedResult(term);
	}, ctx);

	ctx.SetupFront(std::move(act));
	ctx.SetNextTermRef(term);
	ctx.SetupFront(std::move(cont));
	return next(ctx);
}

ReductionStatus
RelayForCall(Context& ctx, TermNode& term, EnvironmentGuard&& gd,
	bool no_lift)
{
	return RelayForEvalOrDirect(ctx, term, std::move(gd), no_lift,
		Continuation(ReduceOnce, ctx));
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

} // namespace Forms;

namespace
{

[[nodiscard, gnu::pure]] inline InvalidSyntax
MakeFunctionAbstractionError() noexcept
{
	return InvalidSyntax("Invalid syntax found in function abstraction.");
}


class VauHandler final : private ystdex::equality_comparable<VauHandler>
{
private:
	string eformal{};
	shared_ptr<TermNode> p_formals;
	EnvironmentReference parent;
	mutable shared_ptr<Environment> p_static;
	mutable shared_ptr<TermNode> p_eval_struct;
	ReductionStatus(VauHandler::*p_call)(TermNode&, Context&) const;

public:
	bool NoLifting = {};

	VauHandler(string&& ename, shared_ptr<TermNode>&& p_fm,
		shared_ptr<Environment>&& p_env, bool owning, TermNode& term, bool nl)
		: eformal(std::move(ename)), p_formals((CheckParameterTree(*p_fm),
		std::move(p_fm))), parent((Environment::EnsureValid(p_env), p_env)),
		p_static(owning ? std::move(p_env) : nullptr),
		p_eval_struct(ShareMoveTerm(ystdex::exchange(term,
		Unilang::AsTermNode(term.get_allocator())))), p_call(eformal.empty()
		? &VauHandler::CallStatic : &VauHandler::CallDynamic), NoLifting(nl)
	{}

	friend bool
	operator==(const VauHandler& x, const VauHandler& y)
	{
		return x.eformal == y.eformal && x.p_formals == y.p_formals
			&& x.parent == y.parent && x.p_static == y.p_static
			&& x.NoLifting == y.NoLifting;
	}

	ReductionStatus
	operator()(TermNode& term, Context& ctx) const
	{
		if(IsBranchedList(term))
		{
			if(p_eval_struct)
				return (this->*p_call)(term, ctx);
			throw UnilangException("Invalid handler of call found.");
		}
		throw UnilangException("Invalid composition found.");
	}

	static void
	CheckParameterTree(const TermNode& term)
	{
		for(const auto& child : term)
			CheckParameterTree(child);
		if(term.Value)
		{
			if(const auto p = TermToNamePtr(term))
				CheckVauSymbol(*p, "parameter in a parameter tree",
					IsIgnore(*p)
						|| CategorizeBasicLexeme(*p) == LexemeCategory::Symbol);
			else
				ThrowInvalidSymbolType(term, "parameter tree node");
		}
	}

private:
	void
	BindEnvironment(Context& ctx, ValueObject&& vo) const
	{
		assert(!eformal.empty());
		ctx.GetRecordRef().AddValue(eformal, std::move(vo));
	}

	ReductionStatus
	CallDynamic(TermNode& term, Context& ctx) const
	{
		auto wenv(ctx.WeakenRecord());
		EnvironmentGuard gd(ctx, Unilang::SwitchToFreshEnvironment(ctx));

		BindEnvironment(ctx, std::move(wenv));
		return DoCall(term, ctx, gd);
	}

	ReductionStatus
	CallStatic(TermNode& term, Context& ctx) const
	{
		EnvironmentGuard gd(ctx, Unilang::SwitchToFreshEnvironment(ctx));

		return DoCall(term, ctx, gd);
	}

	ReductionStatus
	DoCall(TermNode& term, Context& ctx, EnvironmentGuard& gd) const
	{
		assert(p_eval_struct);

		RemoveHead(term);
		BindParameter(ctx.GetRecordPtr(), *p_formals, term);
		ctx.GetRecordRef().Parent = parent;
		// TODO: Implement TCO.
		LiftOtherOrCopy(term, *p_eval_struct, {});
		return RelayForCall(ctx, term, std::move(gd), NoLifting);
	}
};


template<typename _func>
ReductionStatus
CreateFunction(TermNode& term, _func f, size_t n)
{
	Forms::Retain(term);
	if(term.size() > n)
		return f();
	ThrowInsufficientTermsErrorFor(MakeFunctionAbstractionError());
}

template<typename _func>
inline auto
CheckFunctionCreation(_func f) -> decltype(f())
{
	try
	{
		return f();
	}
	catch(...)
	{
		std::throw_with_nested(MakeFunctionAbstractionError());
	}
}

ContextHandler
CreateVau(TermNode& term, bool no_lift, TNIter i,
	shared_ptr<Environment>&& p_env, bool owning)
{
	auto formals(ShareMoveTerm(*++i));
	auto eformal(CheckEnvFormal(*++i));

	term.erase(term.begin(), ++i);
	return FormContextHandler(VauHandler(std::move(eformal), std::move(formals),
		std::move(p_env), owning, term, no_lift));
}

[[noreturn]] ReductionStatus
ThrowForUnwrappingFailure(const ContextHandler& h)
{
	throw TypeError(ystdex::sfmt("Unwrapping failed with type '%s'.",
		h.target_type().name()));
}


template<typename _fComp, typename _func>
void
EqualTerm(TermNode& term, _fComp f, _func g)
{
	Forms::RetainN(term, 2);

	auto i(term.begin());
	const auto& x(*++i);

	term.Value = f(g(x), g(ystdex::as_const(*++i)));
}

template<typename _func>
void
EqualTermReference(TermNode& term, _func f)
{
	EqualTerm(term, [f](const TermNode& x, const TermNode& y){
		return IsLeaf(x) && IsLeaf(y) ? f(x.Value, y.Value)
			: ystdex::ref_eq<>()(x, y);
	}, static_cast<const TermNode&(&)(const TermNode&)>(ReferenceTerm));
}

} // unnamed namespace;


template<typename _type, typename... _tParams>
void
EmplaceCallResult(ValueObject&, _type&&, ystdex::false_, _tParams&&...) ynothrow
{}
template<typename _type>
inline void
EmplaceCallResult(ValueObject& vo, _type&& res, ystdex::true_, ystdex::true_)
	ynoexcept_spec(vo = yforward(res))
{
	vo = yforward(res);
}
template<typename _type>
inline void
EmplaceCallResult(ValueObject& vo, _type&& res, ystdex::true_, ystdex::false_)
{
	vo = ystdex::decay_t<_type>(yforward(res));
}
template<typename _type>
inline void
EmplaceCallResult(ValueObject& vo, _type&& res, ystdex::true_)
{
	Unilang::EmplaceCallResult(vo, yforward(res), ystdex::true_(),
		std::is_same<ystdex::decay_t<_type>, ValueObject>());
}
template<typename _type>
inline void
EmplaceCallResult(ValueObject& vo, _type&& res)
{
	Unilang::EmplaceCallResult(vo, yforward(res), ystdex::not_<
		std::is_same<ystdex::decay_t<_type>, ystdex::pseudo_output>>());
}


namespace Forms
{

template<typename _func>
struct UnaryExpansion
	: private ystdex::equality_comparable<UnaryExpansion<_func>>
{
	_func Function;

	UnaryExpansion(_func f)
		: Function(std::move(f))
	{}

	[[nodiscard, gnu::pure]] friend bool
	operator==(const UnaryExpansion& x, const UnaryExpansion& y)
	{
		return ystdex::examiners::equal_examiner::are_equal(x.Function,
			y.Function);
	}

	template<typename... _tParams>
	inline ReductionStatus
	operator()(TermNode& term, _tParams&&... args) const
	{
		RetainN(term);
		Unilang::EmplaceCallResult(term.Value, ystdex::invoke_nonvoid(
			ystdex::make_expanded<void(TermNode&, _tParams&&...)>(
			std::ref(Function)), *std::next(term.begin()), yforward(args)...));
		return ReductionStatus::Clean;
	}
};


template<typename _type, typename _func>
struct UnaryAsExpansion
	: private ystdex::equality_comparable<UnaryAsExpansion<_type, _func>>
{
	_func Function;

	UnaryAsExpansion(_func f)
		: Function(std::move(f))
	{}

	[[nodiscard, gnu::pure]] friend bool
	operator==(const UnaryAsExpansion& x, const UnaryAsExpansion& y)
	{
		return ystdex::examiners::equal_examiner::are_equal(x.Function,
			y.Function);
	}

	template<typename... _tParams>
	inline ReductionStatus
	operator()(TermNode& term, _tParams&&... args) const
	{
		RetainN(term);
		Unilang::EmplaceCallResult(term.Value, ystdex::invoke_nonvoid(
			ystdex::make_expanded<void(_type&, _tParams&&...)>(
			std::ref(Function)), Unilang::ResolveRegular<_type>(
			*std::next(term.begin())), yforward(args)...));
		return ReductionStatus::Clean;
	}
};


template<size_t _vWrapping = Strict, typename _func, class _tTarget>
inline void
RegisterUnary(_tTarget& target, string_view name, _func f)
{
	Unilang::RegisterHandler<_vWrapping>(target, name,
		UnaryExpansion<_func>(std::move(f)));
}
template<size_t _vWrapping = Strict, typename _type, typename _func,
	class _tTarget>
inline void
RegisterUnary(_tTarget& target, string_view name, _func f)
{
	Unilang::RegisterHandler<_vWrapping>(target, name,
		UnaryAsExpansion<_type, _func>(std::move(f)));
}


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
Equal(TermNode&);

ReductionStatus
EqualValue(TermNode&);


ReductionStatus
If(TermNode&, Context&);


ReductionStatus
Cons(TermNode&);


ReductionStatus
Eval(TermNode&, Context&);


ReductionStatus
MakeEnvironment(TermNode&);

ReductionStatus
GetCurrentEnvironment(TermNode&, Context&);


ReductionStatus
Define(TermNode&, Context&);


ReductionStatus
VauWithEnvironment(TermNode&, Context&);


ReductionStatus
Wrap(TermNode&);

ReductionStatus
Unwrap(TermNode&);


ReductionStatus
Sequence(TermNode&, Context&);


ReductionStatus
Equal(TermNode& term)
{
	EqualTermReference(term, YSLib::HoldSame);
	return ReductionStatus::Clean;
}

ReductionStatus
EqualValue(TermNode& term)
{
	EqualTermReference(term, ystdex::equal_to<>());
	return ReductionStatus::Clean;
}


ReductionStatus
If(TermNode& term, Context& ctx)
{
	Retain(term);

	const auto size(term.size());

	if(size == 3 || size == 4)
	{
		auto i(std::next(term.begin()));

		return ReduceSubsequent(*i, ctx, [&, i]() -> ReductionStatus{
			auto j(i);

			if(!ExtractBool(*j))
				++j;
			return ++j != term.end() ? ReduceOnceLifted(term, ctx, *j)
				: ReduceReturnUnspecified(term);
		});
	}
	else
		throw InvalidSyntax("Syntax error in conditional form.");
}


ReductionStatus
Cons(TermNode& term)
{
	RetainN(term, 2);

	const auto i(std::next(term.begin(), 2));

	ResolveTerm([&](TermNode& nd_y, ResolvedTermReferencePtr p_ref){
		if(IsList(nd_y))
			term.Subterms.splice(term.end(), std::move(nd_y.Subterms));
		else
			throw ListTypeError(ystdex::sfmt(
				"Expected a list for the 2nd argument, got '%s'.",
				TermToStringWithReferenceMark(nd_y, p_ref).c_str()));
	}, *i);
	term.erase(i);
	RemoveHead(term);
	LiftSubtermsToReturn(term);
	return ReductionStatus::Retained;
}


ReductionStatus
Eval(TermNode& term, Context& ctx)
{
	RetainN(term, 2);

	const auto i(std::next(term.begin()));
	auto p_env(ResolveEnvironment(*std::next(i)).first);

	ResolveTerm([&](TermNode& nd){
		auto t(std::move(term.Subterms));

		term.Subterms = nd.Subterms;
		term.Value = nd.Value;
	}, *i);
	return RelayForEvalOrDirect(ctx, term,
		EnvironmentGuard(ctx, ctx.SwitchEnvironment(std::move(p_env))), {},
		Continuation(ReduceOnce, ctx));
}


ReductionStatus
MakeEnvironment(TermNode& term)
{
	Retain(term);

	const auto a(term.get_allocator());

	if(term.size() > 1)
	{
		ValueObject parent;
		const auto tr([&](TNCIter iter){
			return ystdex::make_transform(iter, [&](TNCIter i){
				return ReferenceTerm(*i).Value;
			});
		});

		parent = ValueObject(EnvironmentList(tr(std::next(term.begin())),
			tr(term.end()), a));
		term.Value = Unilang::AllocateEnvironment(a, std::move(parent));
	}
	else
		term.Value = Unilang::AllocateEnvironment(a);
	return ReductionStatus::Clean;
}

ReductionStatus
GetCurrentEnvironment(TermNode& term, Context& ctx)
{
	RetainN(term, 0);
	term.Value = ValueObject(ctx.WeakenRecord());
	return ReductionStatus::Clean;
}


ReductionStatus
Define(TermNode& term, Context& ctx)
{
	Retain(term);
	if(term.size() > 2)
	{
		RemoveHead(term);

		auto formals(MoveFirstSubterm(term));

		RemoveHead(term);
		return ReduceSubsequent(term, ctx,
			std::bind([&](Context&, const TermNode& saved,
			const shared_ptr<Environment>& p_e){
			BindParameter(p_e, saved, term);
			term.Value = ValueToken::Unspecified;
			return ReductionStatus::Clean;
		}, std::placeholders::_1, std::move(formals), ctx.GetRecordPtr()));
	}
	throw InvalidSyntax("Invalid syntax found in definition.");
}


ReductionStatus
VauWithEnvironment(TermNode& term, Context& ctx)
{
	return CreateFunction(term, [&]{
		auto i(term.begin());
		auto& tm(*++i);

		return ReduceSubsequent(tm, ctx, [&, i]{
			term.Value = CheckFunctionCreation([&]{
				auto p_env_pr(ResolveEnvironment(tm));

				return CreateVau(term, {}, i, std::move(p_env_pr.first),
					p_env_pr.second);
			});
			return ReductionStatus::Clean;
		});
	}, 3);
}


ReductionStatus
Wrap(TermNode& term)
{
	RetainN(term);

	auto& tm(*std::next(term.begin()));

	ResolveTerm([&](TermNode& nd, bool has_ref){
		auto& h(nd.Value.Access<ContextHandler>());

		if(const auto p = h.target<FormContextHandler>())
		{
			auto& fch(*p);

			if(has_ref)
				term.Value = ContextHandler(FormContextHandler(fch.Handler,
					fch.Wrapping + 1));
			else
			{
				++fch.Wrapping;
				LiftOther(term, nd);
			}
		}
		else
		{
			if(has_ref)
				term.Value = ContextHandler(FormContextHandler(h, 1));
			else
				term.Value
					= ContextHandler(FormContextHandler(std::move(h), 1));
		}
	}, tm);
	return ReductionStatus::Clean;
}

ReductionStatus
Unwrap(TermNode& term)
{
	RetainN(term);

	auto& tm(*std::next(term.begin()));

	ResolveTerm([&](TermNode& nd, bool has_ref){
		auto& h(nd.Value.Access<ContextHandler>());

		if(const auto p = h.target<FormContextHandler>())
		{
			auto& fch(*p);

			if(has_ref)
				term.Value = ContextHandler(FormContextHandler(fch.Handler,
					fch.Wrapping - 1));
			else if(fch.Wrapping != 0)
			{
				--fch.Wrapping;
				LiftOther(term, nd);
			}
			else
				throw TypeError("Unwrapping failed on an operative argument.");
		}
		else
			ThrowForUnwrappingFailure(h);
	}, tm);
	return ReductionStatus::Clean;
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

	TermNode
	Perform(string_view);

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

TermNode
Interpreter::Perform(string_view unit)
{
	auto term(Read(unit));

	Evaluate(term);
	return term;
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

	ctx.GetRecordRef().Bindings["ignore"].Value = TokenValue("#ignore");
	RegisterStrict(ctx, "eq?", Equal);
	RegisterStrict(ctx, "eqv?", EqualValue);
	RegisterForm(ctx, "$if", If);
	RegisterUnary<>(ctx, "null?", ComposeReferencedTermOp(IsEmpty));
	RegisterStrict(ctx, "cons", Cons);
	RegisterStrict(ctx, "eval", Eval);
	RegisterUnary<Strict, const EnvironmentReference>(ctx, "lock-environment",
		[](const EnvironmentReference& wenv) noexcept{
		return wenv.Lock();
	});
	RegisterStrict(ctx, "make-environment", MakeEnvironment);
	RegisterForm(ctx, "$def!", Define);
	RegisterForm(ctx, "$vau/e", VauWithEnvironment);
	RegisterStrict(ctx, "wrap", Wrap);
	RegisterStrict(ctx, "unwrap", Unwrap);
	RegisterUnary<Strict, const string>(ctx, "raise-invalid-syntax-error",
		[](const string& str){
		throw InvalidSyntax(str.c_str());
	});
	RegisterStrict(ctx, "get-current-environment", GetCurrentEnvironment);
	intp.Perform(R"Unilang(
		$def! $vau $vau/e (() get-current-environment) (formals ef .body) d
			eval (cons $vau/e (cons d (cons formals (cons ef body)))) d;
		$def! lock-current-environment (wrap ($vau () d lock-environment d));
		$def! $lambda $vau (formals .body) d wrap
			(eval (cons $vau (cons formals (cons ignore body))) d);
	)Unilang");
	RegisterStrict(ctx, "list", ReduceBranchToListValue);
	intp.Perform(R"Unilang(
		$def! $set! $vau (e formals .expr) d
			eval (list $def! formals (unwrap eval) expr d) (eval e d);
		$def! $defv! $vau ($f formals ef .body) d
			eval (list $set! d $f $vau formals ef body) d;
		$defv! $defl! (f formals .body) d
			eval (list $set! d f $lambda formals body) d;
		$def! make-standard-environment
			$lambda () () lock-current-environment;
	)Unilang");
	RegisterForm(ctx, "$sequence", Sequence);
	intp.Perform(R"Unilang(
		$defl! first ((x .)) x;
		$defl! rest ((#ignore .x)) x;
		$defl! apply (appv arg .opt)
			eval (cons () (cons (unwrap appv) arg))
				($if (null? opt) (() make-environment)
					(($lambda ((e .eopt))
						$if (null? eopt) e
							(raise-invalid-syntax-error
								"Syntax error in applying form.")) opt));
		$defl! list* (head .tail)
			$if (null? tail) head (cons head (apply list* tail));
		$defv! $defw! (f formals ef .body) d
			eval (list $set! d f wrap (list* $vau formals ef body)) d;
		$defv! $cond clauses d
			$if (null? clauses) #inert
				(apply ($lambda ((test .body) .clauses)
					$if (eval test d) (eval body d)
						(apply (wrap $cond) clauses d)) clauses);
		$defv! $when (test .exprseq) d
			$if (eval test d) (eval (list* () $sequence exprseq) d);
		$defv! $unless (test .exprseq) d
			$if (eval test d) #inert (eval (list* () $sequence exprseq) d);
		$defl! not? (x) eqv? x #f;
		$defv! $and? x d $cond
			((null? x) #t)
			((null? (rest x)) eval (first x) d)
			((eval (first x) d) apply (wrap $and?) (rest x) d)
			(#t #f);
		$defv! $or? x d $cond
			((null? x) #f)
			((null? (rest x)) eval (first x) d)
			(#t ($lambda (r) $if r r
				(apply (wrap $or?) (rest x) d)) (eval (first x) d));
		$defw! accr (l pred? base head tail sum) d
			$if (apply pred? (list l) d) base
				(apply sum (list (apply head (list l) d)
					(apply accr (list (apply tail (list l) d)
					pred? base head tail sum) d)) d);
		$defw! foldr1 (kons knil l) d
			apply accr (list l null? knil first rest kons) d;
		$defw! map1 (appv l) d
			foldr1 ($lambda (x xs) cons (apply appv (list x) d) xs) () l;
		$defl! list-concat (x y) foldr1 cons y x;
		$defl! append (.ls) foldr1 list-concat () ls;
		$defv! $let (bindings .body) d
			eval (list* () (list* $lambda (map1 first bindings)
				(list body)) (map1 ($lambda (x) list (rest x)) bindings)) d;
		$defv! $let/d (bindings ef .body) d
			eval (list* () (list wrap (list* $vau (map1 first bindings)
				ef (list body))) (map1 ($lambda (x) list (rest x)) bindings)) d;
		$defv! $let* (bindings .body) d
			eval ($if (null? bindings) (list* $let bindings body)
				(list $let (list (first bindings))
				(list* $let* (rest bindings) body))) d;
		$defv! $letrec (bindings .body) d
			eval (list $let () $sequence (list $def! (map1 first bindings)
				(list* () list (map1 rest bindings))) body) d;
		$defv! $bindings/p->environment (parents .bindings) d $sequence
			($def! res apply make-environment (map1 ($lambda (x) eval x d)
				parents))
			(eval (list $set! res (map1 first bindings)
				(list* () list (map1 rest bindings))) d)
			res;
		$defv! $bindings->environment (.bindings) d
			eval (list* $bindings/p->environment
				(list (() make-standard-environment)) bindings) d;
		$defv! $provide! (symbols .body) d
			$sequence (eval (list $def! symbols (list $let () $sequence
				(list ($vau (e) d $set! e res (lock-environment d))
				(() get-current-environment)) body
				(list* () list symbols))) d) res;
		$defv! $provide/d! (symbols ef .body) d
			$sequence (eval (list $def! symbols (list $let/d () ef $sequence
				(list ($vau (e) d $set! e res (lock-environment d))
				(() get-current-environment)) body
				(list* () list symbols))) d) res;
		$defv! $import! (e .symbols) d
			eval (list $set! d symbols (list* () list symbols)) (eval e d);
	)Unilang");
	RegisterStrict(ctx, "display", [&](TermNode& term){
		RetainN(term);
		LiftOther(term, *std::next(term.begin()));
		PrintTermNode(std::cout, term);
		return ReduceReturnUnspecified(term);
	});
	RegisterStrict(ctx, "newline", [&](TermNode& term){
		RetainN(term, 0);
		std::cout << std::endl;
		return ReduceReturnUnspecified(term);
	});
	intp.SaveGround();
}

#define APP_NAME "Unilang demo"
#define APP_VER "0.4.7"
#define APP_PLATFORM "[C++11] + YSLib"
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

