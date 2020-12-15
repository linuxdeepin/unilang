// © 2020 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_TermNode_h_
#define INC_Unilang_TermNode_h_ 1

#include "Unilang.h" // for ValueObject, list, yforward;
#include <ystdex/type_traits.hpp> // for std::is_constructible,
//	ystdex::enable_if_t, std::is_convertible, ystdex::decay_t, ystdex::false_,
//	ystdex::not_;
#include <cassert> // for assert;
#include <ystdex/type_op.hpp> // for ystdex::cond_or_t;

namespace Unilang
{

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

private:
	Container container{};

public:
	ValueObject Value{};

	TermNode() = default;
	explicit
	TermNode(allocator_type a)
		: container(a)
	{}
	TermNode(const Container& con)
		: container(con)
	{}
	TermNode(Container&& con)
		: container(std::move(con))
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
		: container(con), Value(yforward(args)...)
	{}
	template<typename... _tParams,
		typename = enable_value_constructible_t<_tParams...>>
	TermNode(Container&& con, _tParams&&... args)
		: container(std::move(con)), Value(yforward(args)...)
	{}
	template<typename... _tParams,
		typename = enable_value_constructible_t<_tParams...>>
	inline
	TermNode(std::allocator_arg_t, allocator_type a, NoContainerTag,
		_tParams&&... args)
		: container(a), Value(yforward(args)...)
	{}
	template<typename... _tParams,
		typename = enable_value_constructible_t<_tParams...>>
	inline
	TermNode(std::allocator_arg_t, allocator_type a, const Container& con,
		_tParams&&... args)
		: container(con, a), Value(yforward(args)...)
	{}
	template<typename... _tParams,
		typename = enable_value_constructible_t<_tParams...>>
	inline
	TermNode(std::allocator_arg_t, allocator_type a, Container&& con,
		_tParams&&... args)
		: container(std::move(con), a), Value(yforward(args)...)
	{}
	TermNode(const TermNode&) = default;
	TermNode(const TermNode& tm, allocator_type a)
		: container(tm.container, a), Value(tm.Value)
	{}
	TermNode(TermNode&&) = default;
	TermNode(TermNode&& tm, allocator_type a)
		: container(std::move(tm.container), a), Value(std::move(tm.Value))
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

	[[nodiscard, gnu::pure]] const Container&
	GetContainer() const noexcept
	{
		return container;
	}

	[[nodiscard, gnu::pure]] Container&
	GetContainerRef() noexcept
	{
		return container;
	}

	void
	Add(const TermNode& term)
	{
		container.push_back(term);
	}
	void
	Add(TermNode&& term)
	{
		container.push_back(std::move(term));
	}

	[[nodiscard, gnu::pure]] iterator
	begin() noexcept
	{
		return container.begin();
	}
	[[nodiscard, gnu::pure]] const_iterator
	begin() const noexcept
	{
		return container.begin();
	}

	[[nodiscard, gnu::pure]] bool
	empty() const noexcept
	{
		return container.empty();
	}

	[[nodiscard, gnu::pure]] iterator
	end() noexcept
	{
		return container.end();
	}
	[[nodiscard, gnu::pure]] const_iterator
	end() const noexcept
	{
		return container.end();
	}

	iterator
	erase(const_iterator i)
	{
		return container.erase(i);
	}
	iterator
	erase(const_iterator first, const_iterator last)
	{
		return container.erase(first, last);
	}

	[[nodiscard, gnu::pure]]
	allocator_type
	get_allocator() const noexcept
	{
		return container.get_allocator();
	}

	[[nodiscard, gnu::pure]] size_t
	size() const noexcept
	{
		return container.size();
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
[[nodiscard, gnu::pure]] inline TermNode
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

} // namespace Unilang;

#endif

