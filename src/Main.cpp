// © 2020-2020 Uniontech Software Technology Co.,Ltd.

#include <utility> // for std::pair, std::reference_wrapper, std::ref,
//	std::exchange;
#include <any> // for std::any, std::bad_any_cast, std:;any_cast;
#include <functional> // for std::function, std::less;
#include <memory> // for std::shared_ptr, std::weak_ptr;
#include <string_view> // for std::string_view;
#include <memory_resource> // for pmr and
//	complete std::pmr::polymorphic_allocator;
#include <forward_list> // for std::pmr::forward_list;
#include <list> // for std::pmr::list;
#include <map> // for std::pmr::map;
#include <string> // for std::pmr::string, std::string, std::getline;
#include <vector> // for std::pmr::vector;
#include <deque> // for std::pmr::deque;
#include <stack> // for std::stack;
#include <sstream> // for std::basic_ostringstream, std::ostream,
//	std::streamsize;
#include <exception> // for std::runtime_error;
#include <cassert> // for assert;
#include <cstdarg> // for std::va_list, va_copy, va_end, va_start;
#include <cstdio> // for std::vsprintf;
#include <cctype> // for std::isgraph;
#include <typeinfo> // for typeid;
#include <iostream> // for std::cout, std::cerr, std::endl, std::cin;
#include <algorithm> // for std::for_each;

namespace Unilang
{

using std::size_t;

template<typename _type>
using lref = std::reference_wrapper<_type>;

using std::any;
using std::bad_any_cast;
using std::function;
using std::pair;
using std::shared_ptr;
using std::string_view;
using std::weak_ptr;

namespace pmr = std::pmr;

using pmr::forward_list;
using pmr::list;
using pmr::map;
using pmr::string;
using pmr::vector;

template<typename _type, class _tSeqCon = std::pmr::deque<_type>>
using stack = std::stack<_type, _tSeqCon>;
using ostringstream = std::basic_ostringstream<char, std::char_traits<char>,
	string::allocator_type>;

using ValueObject = any;


// NOTE: This is locale-independent.
[[nodiscard, gnu::const]] constexpr bool
isdigit(char c) noexcept
{
	return (unsigned(c) - '0') < 10U;
}

[[nodiscard, gnu::nonnull(1), gnu::pure]] size_t
vfmtlen(const char*, std::va_list) noexcept;

size_t
vfmtlen(const char* fmt, std::va_list args) noexcept
{
	assert(fmt);

	const int l(std::vsnprintf({}, 0, fmt, args));

	return size_t(l < 0 ? -1 : l);
}

template<class _tString = string>
[[nodiscard, gnu::nonnull(1)]] _tString
vsfmt(const typename _tString::value_type* fmt,
	std::va_list args)
{
	std::va_list ap;

	va_copy(ap, args);

	const auto l(Unilang::vfmtlen(fmt, ap));

	va_end(ap);
	if(l == size_t(-1))
		throw std::runtime_error("Failed to write formatted string.");

	_tString str(l, typename _tString::value_type());

	if(l != 0)
	{
		assert(str.length() > 0 && str[0] == typename _tString::value_type());
		std::vsprintf(&str[0], fmt, args);
	}
	return str;
}

template<class _tString = string>
[[nodiscard, gnu::nonnull(1)]] _tString
sfmt(const typename _tString::value_type* fmt, ...)
{
	std::va_list args;

	va_start(args, fmt);
	try
	{
		auto str(Unilang::vsfmt<_tString>(fmt, args));

		va_end(args);
		return str;
	}
	catch(...)
	{
		va_end(args);
		throw;
	}
}


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


template<typename _type>
[[nodiscard, gnu::pure]] inline _type*
TryAccessLeaf(TermNode& term)
{
	return std::any_cast<_type>(&term.Value);
}
template<typename _type>
[[nodiscard, gnu::pure]] inline const _type*
TryAccessLeaf(const TermNode& term)
{
	return std::any_cast<_type>(&term.Value);
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


class Environment final
{
public:
	using BindingMap = map<string, TermNode, std::less<>>;
	using NameResolution
		= pair<BindingMap::mapped_type*, shared_ptr<Environment>>;
	using allocator_type = BindingMap::allocator_type;

	mutable BindingMap Bindings;
	ValueObject Parent{};

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

	static void
	CheckParent(const ValueObject&);

	[[nodiscard, gnu::pure]] NameResolution::first_type
	LookupName(string_view) const;
};

void
Environment::CheckParent(const ValueObject&)
{
	// TODO: Check parent type.
}

Environment::NameResolution::first_type
Environment::LookupName(string_view id) const
{
	assert(id.data());

	const auto i(Bindings.find(id));

	return i != Bindings.cend() ? &i->second : nullptr;
}


class Context;

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

	[[nodiscard, gnu::pure]] weak_ptr<Environment>
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
	return {p_env->LookupName(id), std::move(p_env)};
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
	return std::exchange(p_record, p_env);
}

weak_ptr<Environment>
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


namespace Forms
{
} // namespace Forms;


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
				|| Unilang::isdigit(f))
				throw UnilangException(Unilang::sfmt<std::string>(id.front()
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
					term.Value = TermReference(bound);
				res = ReductionStatus::Neutral;
			}
			else
				throw UnilangException(
					"Bad identifier '" + std::string(id) + "' found.");
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
	throw UnilangException("Invalid object found in the combiner position.");
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
			auto term_ref(std::ref(term));

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
			return ReduceBranch(sub, ctx);
		});
		return ReductionStatus::Partial;
	}
	return ReductionStatus::Retained;
}

} // unnamed namespace;

ReductionStatus
ReduceOnce(TermNode& term, Context& ctx)
{
	return term.Value.has_value() ? ReduceLeaf(term, ctx)
		: ReduceBranch(term, ctx);
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

		if(Unilang::isdigit(f))
		{
		    int ans(0);

			for(auto p(id.begin()); p != id.end(); ++p)
				if(Unilang::isdigit(*p))
				{
					if(unsigned((ans << 3) + (ans << 1) + *p - '0')
						<= unsigned(INT_MAX))
						ans = (ans << 3) + (ans << 1) + *p - '0';
					else
						throw UnilangException(Unilang::sfmt<std::string>(
							"Value of identifier '%s' is out of the range of"
							" the supported integer.", id.data()));
				}
				else
					throw UnilangException(Unilang::sfmt<std::string>("Literal"
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
PrintTermNode(std::ostream& os, const TermNode& term, size_t depth = 0)
{
	try
	{
		const auto& vo(term.Value);

		os << [&]() -> string{
			if(const auto p = std::any_cast<string>(&vo))
				return *p;
			if(const auto p = std::any_cast<bool>(&vo))
				return *p ? "#t" : "#f";
			if(const auto p = std::any_cast<int>(&vo))
				return sfmt<string>("%d", *p);

			const auto& t(vo.type());

			if(t != typeid(void))
				return "#[" + string(t.name()) + ']';
			throw bad_any_cast();
		}();
	}
	catch(bad_any_cast&)
	{
		if(term)
			for(const auto& nd : term)
			{
				os << '(';
				try
				{
					PrintTermNode(os, nd, depth + 1);
				}
				catch(std::out_of_range&)
				{}
				os << ')';
			}
	}
}


class Interpreter final
{
private:
	string line{};
	shared_ptr<Environment> p_ground{};

public:
	Context Root{*pmr::new_delete_resource()};

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
	ostringstream oss(string(term.get_allocator()));

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

	if(ReduceSyntax(term, parse_result.cbegin(), parse_result.cend(), ParseLeaf)
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

	// TODO: Re-enable the protection after the parent environment resolution is
	//	implemented.
//	intp.SaveGround();
}

#define APP_NAME "Unilang demo"
#define APP_VER "0.0.30"
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
	LoadFunctions(intp);
	cout << "Initialization finished." << endl;
	cout << "Type \"exit\" to exit, \"cls\" to clear screen." << endl << endl;
	intp.Run();
}

