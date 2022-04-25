// © 2020-2022 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_TermNode_h_
#define INC_Unilang_TermNode_h_ 1

#include "Unilang.h" // for ValueObject, list, Unilang::Deref, yforward,
//	std::allocator_arg_t, YSLib::IsTyped;
#include <YModules.h>
#include YFM_YBaseMacro // for DefBitmaskEnum;
#include <ystdex/type_traits.hpp> // for std::is_constructible,
//	ystdex::enable_if_t, std::is_assignable, std::is_nothrow_assignable,
//	std::is_convertible, ystdex::decay_t, ystdex::false_, ystdex::not_;
#include <cassert> // for assert;
#include <ystdex/type_op.hpp> // for ystdex::cond_or_t;

namespace Unilang
{

enum TermTagIndices : size_t
{
	UnqualifiedIndex = 0,
	UniqueIndex,
	NonmodifyingIndex,
	TemporaryIndex
};

enum class TermTags
{
	Unqualified = 1 << UnqualifiedIndex,
	Unique = 1 << UniqueIndex,
	Nonmodifying = 1 << NonmodifyingIndex,
	Temporary = 1 << TemporaryIndex
};

DefBitmaskEnum(TermTags)

YB_ATTR_nodiscard YB_STATELESS constexpr TermTags
GetLValueTagsOf(const TermTags& tags) noexcept
{
	return tags & ~TermTags::Temporary;
}

YB_ATTR_nodiscard YB_STATELESS constexpr TermTags
PropagateTo(TermTags dst, TermTags tags) noexcept
{
	return dst | (tags & TermTags::Nonmodifying);
}

inline void
EnsureValueTags(TermTags& tags) noexcept
{
	tags &= ~TermTags::Temporary;
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

private:
	Container container{};

public:
	ValueObject Value{};
	TermTags Tags = TermTags::Unqualified;

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
	TermNode(const TermNode& nd, allocator_type a)
		: container(nd.container, a), Value(nd.Value), Tags(nd.Tags)
	{}
	TermNode(TermNode&&) = default;
	TermNode(TermNode&& nd, allocator_type a)
		: container(std::move(nd.container), a), Value(std::move(nd.Value)),
		Tags(nd.Tags)
	{}
	~TermNode()
	{
		Clear();
	}

	TermNode&
	operator=(const TermNode&) = default;
	TermNode&
	operator=(TermNode&&) = default;

	YB_ATTR_nodiscard YB_PURE bool
	operator!() const noexcept
	{
		return !bool(*this);
	}

	YB_ATTR_nodiscard YB_PURE explicit
	operator bool() const noexcept
	{
		return Value || !empty();
	}

	YB_ATTR_nodiscard YB_PURE const Container&
	GetContainer() const noexcept
	{
		return container;
	}

	YB_ATTR_nodiscard YB_PURE Container&
	GetContainerRef() noexcept
	{
		return container;
	}

	template<class _tCon, class _type>
	ystdex::enable_if_t<
		ystdex::and_<std::is_assignable<Container, _tCon&&>,
		std::is_assignable<ValueObject, _type&&>>::value>
	SetContent(_tCon&& con, _type&& val) noexcept(ystdex::and_<
		std::is_nothrow_assignable<Container, _tCon&&>,
		std::is_nothrow_assignable<ValueObject, _type&&>>())
	{
		container = yforward(con);
		Value = yforward(val);
	}
	void
	SetContent(const TermNode& nd)
	{
		SetContent(nd.container, nd.Value);
		Tags = nd.Tags;
	}
	void
	SetContent(TermNode&& nd)
	{
		SetContent(std::move(nd.container), std::move(nd.Value));
		Tags = nd.Tags;
	}

	template<typename _tParam>
	inline yimpl(ystdex::enable_if_same_param_t<ValueObject, _tParam>)
	SetValue(_tParam&& arg) noexcept(noexcept(yforward(arg)))
	{
		Value = yforward(arg);
	}
	template<typename _tParam, typename... _tParams,
		yimpl(ystdex::enable_if_t<sizeof...(_tParams) != 0
		|| !ystdex::is_same_param<ValueObject, _tParam>::value, int> = 0,
		ystdex::exclude_self_t<std::allocator_arg_t, _tParam, int> = 0)>
	inline yimpl(ystdex::enable_if_inconvertible_t)<ystdex::decay_t<_tParam>,
		TermNode::allocator_type, void>
	SetValue(_tParam&& arg, _tParams&&... args) noexcept(noexcept(Value.assign(
		std::allocator_arg, std::declval<TermNode::allocator_type&>(),
		yforward(arg), yforward(args)...)))
	{
		SetValue(get_allocator(), yforward(arg), yforward(args)...);
	}
	template<typename... _tParams>
	inline void
	SetValue(TermNode::allocator_type a, _tParams&&... args) noexcept(
		noexcept(Value.assign(std::allocator_arg, a, yforward(args)...)))
	{
		Value.assign(std::allocator_arg, a, yforward(args)...);
	}

	void
	Add(const TermNode& nd)
	{
		container.push_back(nd);
	}
	void
	Add(TermNode&& nd)
	{
		container.push_back(std::move(nd));
	}

	template<typename... _tParams>
	static inline void
	AddValueTo(Container& con, _tParams&&... args)
	{
		con.emplace_back(NoContainer, yforward(args)...);
	}
	template<typename... _tParams>
	static inline void
	AddValueTo(const_iterator position, Container& con, _tParams&&... args)
	{
		con.emplace(position, NoContainer, yforward(args)...);
	}

	void
	Clear() noexcept
	{
		Value.Clear();
		ClearContainer();
	}

	void
	ClearContainer() noexcept;

	void
	CopyContainer(const TermNode& nd)
	{
		GetContainerRef() = Container(nd.GetContainer());
	}

	void
	CopyContent(const TermNode& nd)
	{
		SetContent(TermNode(nd));
	}

	void
	CopyValue(const TermNode& nd)
	{
		Value = ValueObject(nd.Value);
	}

	YB_ATTR_nodiscard YB_PURE iterator
	begin() noexcept
	{
		return container.begin();
	}
	YB_ATTR_nodiscard YB_PURE const_iterator
	begin() const noexcept
	{
		return container.begin();
	}

	YB_ATTR_nodiscard YB_PURE bool
	empty() const noexcept
	{
		return container.empty();
	}

	YB_ATTR_nodiscard YB_PURE iterator
	end() noexcept
	{
		return container.end();
	}
	YB_ATTR_nodiscard YB_PURE const_iterator
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

	YB_ATTR_nodiscard YB_PURE
	allocator_type
	get_allocator() const noexcept
	{
		return container.get_allocator();
	}

	YB_ATTR_nodiscard YB_PURE reverse_iterator
	rbegin() noexcept
	{
		return container.rbegin();
	}
	YB_ATTR_nodiscard YB_PURE const_reverse_iterator
	rbegin() const noexcept
	{
		return container.rbegin();
	}

	YB_ATTR_nodiscard YB_PURE reverse_iterator
	rend() noexcept
	{
		return container.rend();
	}
	YB_ATTR_nodiscard YB_PURE const_reverse_iterator
	rend() const noexcept
	{
		return container.rend();
	}

	YB_ATTR_nodiscard YB_PURE size_t
	size() const noexcept
	{
		return container.size();
	}
};

using TNIter = TermNode::iterator;
using TNCIter = TermNode::const_iterator;

YB_ATTR_nodiscard YB_PURE inline bool
IsBranch(const TermNode& nd) noexcept
{
	return !nd.empty();
}

YB_ATTR_nodiscard YB_PURE inline bool
IsBranchedList(const TermNode& nd) noexcept
{
	return !(nd.empty() || nd.Value);
}

YB_ATTR_nodiscard YB_PURE inline bool
IsEmpty(const TermNode& nd) noexcept
{
	return !nd;
}

YB_ATTR_nodiscard YB_PURE inline bool
IsExtendedList(const TermNode& nd) noexcept
{
	return !(nd.empty() && nd.Value);
}

YB_ATTR_nodiscard YB_PURE inline bool
IsLeaf(const TermNode& nd) noexcept
{
	return nd.empty();
}

YB_ATTR_nodiscard YB_PURE inline bool
IsList(const TermNode& nd) noexcept
{
	return !nd.Value;
}

YB_ATTR_nodiscard YB_PURE inline bool
IsRegular(const TermNode& nd) noexcept
{
	return IsLeaf(nd) || IsList(nd);
}

using YSLib::IsTyped;
template<typename _type>
YB_ATTR_nodiscard YB_PURE inline bool
IsTyped(const TermNode& nd)
{
	return IsTyped<_type>(nd.Value);
}

template<typename _type>
YB_ATTR_nodiscard YB_PURE inline bool
IsTypedRegular(const TermNode& nd)
{
	return IsLeaf(nd) && IsTyped<_type>(nd);
}

template<typename _type>
YB_ATTR_nodiscard YB_PURE inline bool
HasValue(const TermNode& nd, const _type& x)
{
	return nd.Value == x;
}

template<typename _type>
YB_ATTR_nodiscard YB_PURE inline _type&
Access(TermNode& nd)
{
	return nd.Value.Access<_type&>();
}
template<typename _type>
YB_ATTR_nodiscard YB_PURE inline const _type&
Access(const TermNode& nd)
{
	return nd.Value.Access<const _type&>();
}

inline void
AssertBranch(const TermNode& nd) noexcept
{
	yunused(nd);
	assert(IsBranch(nd) && "Invalid term found.");
}

inline void
AssertBranchedList(const TermNode& nd) noexcept
{
	yunused(nd);
	assert(IsBranchedList(nd) && "Invalid term found.");
}

inline void
AssertValueTags(const TermNode& nd) noexcept
{
	yunused(nd);
	assert(nd.Tags == TermTags && "Invalid term of first-class value found.");
}

YB_ATTR_nodiscard YB_PURE inline TermNode&
AccessFirstSubterm(TermNode& nd) noexcept
{
	AssertBranch(nd);
	return Unilang::Deref(nd.begin());
}
YB_ATTR_nodiscard YB_PURE inline const TermNode&
AccessFirstSubterm(const TermNode& nd) noexcept
{
	AssertBranch(nd);
	return Unilang::Deref(nd.begin());
}

YB_ATTR_nodiscard YB_PURE inline TermNode&&
MoveFirstSubterm(TermNode& nd)
{
	return std::move(AccessFirstSubterm(nd));
}

YB_ATTR_nodiscard inline shared_ptr<TermNode>
ShareMoveTerm(TermNode& nd)
{
	return ystdex::share_move(nd.get_allocator(), nd);
}
YB_ATTR_nodiscard inline shared_ptr<TermNode>
ShareMoveTerm(TermNode&& nd)
{
	return ystdex::share_move(nd.get_allocator(), nd);
}

inline void
RemoveHead(TermNode& nd) noexcept
{
	assert(!nd.empty());
	nd.erase(nd.begin());
}

template<typename... _tParam, typename... _tParams>
YB_ATTR_nodiscard YB_PURE inline
ystdex::enable_if_t<ystdex::not_<ystdex::cond_or_t<ystdex::bool_<
	(sizeof...(_tParams) >= 1)>, ystdex::false_, std::is_convertible,
	ystdex::decay_t<_tParams>..., TermNode::allocator_type>>::value, TermNode>
AsTermNode(_tParams&&... args)
{
	return TermNode(NoContainer, yforward(args)...);
}
template<typename... _tParams>
YB_ATTR_nodiscard YB_PURE inline TermNode
AsTermNode(TermNode::allocator_type a, _tParams&&... args)
{
	return TermNode(std::allocator_arg, a, NoContainer, yforward(args)...);
}

} // namespace Unilang;

#endif

