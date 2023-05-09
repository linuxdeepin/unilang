// SPDX-FileCopyrightText: 2020-2023 UnionTech Software Technology Co.,Ltd.

#include "Evaluation.h" // for Unilang::allocate_shared,
//	Unilang::AsTermNodeTagged, TermTags, ystdex::equality_comparable, AnchorPtr,
//	lref, ContextHandler, EnvironmentReference, string_view, ValueToken,
//	TermReference, byte, pmr::polymorphic_allocator, lref, SourceInformation,
//	YSLib::AllocatorHolder, YSLib::IValueHolder::Creation,
//	YSLib::AllocatedHolderOperations, YSLib::forward_as_tuple, Continuation,
//	TermToStringWithReferenceMark, IsSingleElementList, AccessFirstSubterm,
//	GetLValueTagsOf, ThrowTypeErrorForInvalidType, TermToNamePtr, IsList,
//	in_place_type, stack, IsPair, yunseq, ResolveTerm, IsAtom, ResolveSuffix,
//	IsTyped, ThrowInsufficientTermsError, ThrowListTypeErrorForAtom,
//	YSLib::lock_guard, YSLib::mutex, unordered_map, type_index, std::allocator,
//	pair, AssertValueTags;
#include "TermAccess.h" // for ClearCombiningTags, TryAccessLeafAtom,
//	TokenValue, AssertCombiningTerm, IsCombiningTerm, TryAccessTerm;
#include <cassert> // for assert;
#include "Math.h" // for ReadDecimal;
#include <limits> // for std::numeric_limits;
#include <ystdex/string.hpp> // for ystdex::sfmt, std::string,
//	ystdex::begins_with;
#include "Exception.h" // for BadIdentifier, InvalidReference, InvalidSyntax,
//	std::throw_with_nested, ParameterMismatch, ListReductionFailure,
//	ThrowListTypeErrorForNonList;
#include <utility> // for std::declval;
#include <ystdex/functional.hpp> // for std::ref, ystdex::retry_on_cond,
//	ystdex::update_thunk;
#include "TCO.h" // for Action, RelayDirect, Unilang::RelayCurrentOrDirect,
//	AssertNextTerm, EnsureTCOAction, TCOAction, ActiveCombiner;
#include <ystdex/type_traits.hpp> // for ystdex::false_, ystdex::true_;
#include <tuple> // for std::tuple, std::get;
#include <iterator> // for std::prev;
#include <ystdex/functor.hpp> // for std::hash, ystdex::equal_to,
//	ystdex::ref_eq;
#include <ystdex/utility.hpp> // for ystdex::parameterize_static_object,
//	std::piecewise_construct;
#include "Lexical.h" // for CategorizeBasicLexeme, LexemeCategory,
//	DeliteralizeUnchecked;
#include <ystdex/deref_op.hpp> // for ystdex::call_value_or;
#include YFM_YSLib_Core_YException // for YSLib::FilterExceptions,
//	YSLib::Notice;

namespace Unilang
{

namespace
{

template<class _tAlloc, typename... _tParams>
YB_ATTR_nodiscard inline shared_ptr<TermNode>
AllocateSharedTerm(const _tAlloc& a, _tParams&&... args)
{
	return Unilang::allocate_shared<TermNode>(a, yforward(args)...);
}

YB_ATTR_nodiscard inline TermNode
MakeSubobjectReferent(TermNode::allocator_type a, shared_ptr<TermNode> p_sub)
{
	return Unilang::AsTermNodeTagged(a, TermTags::Sticky, std::move(p_sub));
}


class RefContextHandler final
	: private ystdex::equality_comparable<RefContextHandler>
{
private:
	AnchorPtr anchor_ptr;

public:
	lref<const ContextHandler> HandlerRef;

	RefContextHandler(const ContextHandler& h,
		const EnvironmentReference& env_ref) noexcept
		: anchor_ptr(env_ref.GetAnchorPtr()), HandlerRef(h)
	{}
	RefContextHandler(const RefContextHandler&) = default;
	RefContextHandler(RefContextHandler&&) = default;

	RefContextHandler&
	operator=(const RefContextHandler&) = default;
	RefContextHandler&
	operator=(RefContextHandler&&) = default;

	YB_ATTR_nodiscard YB_PURE friend bool
	operator==(const RefContextHandler& x, const RefContextHandler& y)
	{
		return x.HandlerRef.get() == y.HandlerRef.get();
	}

	ReductionStatus
	operator()(TermNode& term, Context& ctx) const
	{
		ClearCombiningTags(term);
		return HandlerRef.get()(term, ctx);
	}
};


ReductionStatus
DefaultEvaluateLeaf(TermNode& term, string_view id)
{
	assert(bool(id.data()));
	assert(!id.empty() && "Invalid leaf token found.");
	switch(id.front())
	{
	case '#':
		id.remove_prefix(1);
		if(!id.empty())
			switch(id.front())
			{
			case 't':
				if(id.size() == 1 || id.substr(1) == "rue")
				{
					term.Value = true;
					return ReductionStatus::Clean;
				}
				break;
			case 'f':
				if(id.size() == 1 || id.substr(1) == "alse")
				{
					term.Value = false;
					return ReductionStatus::Clean;
				}
				break;
			case 'i':
				if(id.substr(1) == "nert")
				{
					term.Value = ValueToken::Unspecified;
					return ReductionStatus::Clean;
				}
				else if(id.substr(1) == "gnore")
				{
					term.Value = ValueToken::Ignore;
					return ReductionStatus::Clean;
				}
				break;
			}
	default:
		break;
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		ReadDecimal(term.Value, id, id.begin());
		return ReductionStatus::Clean;
	case '-':
		if(YB_UNLIKELY(id.find_first_not_of("+-") == string_view::npos))
			break;
		if(id.size() == 6 && id[4] == '.' && id[5] == '0')
		{
			if(id[1] == 'i' && id[2] == 'n' && id[3] == 'f')
			{
				term.Value = -std::numeric_limits<double>::infinity();
				return ReductionStatus::Clean;
			}
			if(id[1] == 'n' && id[2] == 'a' && id[3] == 'n')
			{
				term.Value = -std::numeric_limits<double>::quiet_NaN();
				return ReductionStatus::Clean;
			}
		}
		if(id.size() > 1)
			ReadDecimal(term.Value, id, std::next(id.begin()));
		else
			term.Value = 0;
		return ReductionStatus::Clean;
	case '+':
		if(YB_UNLIKELY(id.find_first_not_of("+-") == string_view::npos))
			break;
		if(id.size() == 6 && id[4] == '.' && id[5] == '0')
		{
			if(id[1] == 'i' && id[2] == 'n' && id[3] == 'f')
			{
				term.Value = std::numeric_limits<double>::infinity();
				return ReductionStatus::Clean;
			}
			if(id[1] == 'n' && id[2] == 'a' && id[3] == 'n')
			{
				term.Value = std::numeric_limits<double>::quiet_NaN();
				return ReductionStatus::Clean;
			}
		}
		YB_ATTR_fallthrough;
	case '0':
		if(id.size() > 1)
			ReadDecimal(term.Value, id, std::next(id.begin()));
		else
			term.Value = 0;
		return ReductionStatus::Clean;
	}
	return ReductionStatus::Retrying;
}

YB_ATTR_nodiscard YB_PURE bool
ParseSymbol(TermNode& term, string_view id)
{
	assert(id.data());
	assert(!id.empty() && "Invalid lexeme found.");
	if(CheckReducible(DefaultEvaluateLeaf(term, id)))
	{
		const char f(id.front());

		if((id.size() > 1 && (f == '#'|| f == '+' || f == '-')
			&& id.find_first_not_of("+-") != string_view::npos))
			throw InvalidSyntax(ystdex::sfmt(f != '#'
				? "Unsupported literal prefix found in literal '%s'."
				: "Invalid literal '%s' found.", id.data()));
		return true;
	}
	return {};
}

YB_ATTR_nodiscard YB_PURE inline TermReference
EnsureLValueReference(TermReference&& ref)
{
	return TermReference(ref.GetTags() & ~TermTags::Unique, std::move(ref));
}

ReductionStatus
EvaluateLeafToken(TermNode& term, Context& ctx, string_view id)
{
	assert(id.data());

	auto pr(ctx.Resolve(ctx.GetRecordPtr(), id));

	if(pr.first)
	{
		auto& bound(*pr.first);

		if(!ctx.TrySetTailOperatorName(term))
			ctx.OperatorName.Clear();
		if(const auto p_bound = TryAccessLeafAtom<const TermReference>(bound))
		{
			term.GetContainerRef() = bound.GetContainer();
			term.Value = EnsureLValueReference(TermReference(*p_bound));
		}
		else
		{
			auto p_env(Unilang::Nonnull(pr.second));

			term.Value = TermReference(p_env->MakeTermTags(bound)
				& ~TermTags::Unique, bound, std::move(p_env));
		}
		return ReductionStatus::Neutral;
	}
	throw BadIdentifier(id);
}


using SourcedByteAllocator = pmr::polymorphic_allocator<yimpl(byte)>;

using SourceInfoMetadata = lref<const SourceInformation>;


template<typename _type, class _tByteAlloc = SourcedByteAllocator>
class SourcedHolder : public YSLib::AllocatorHolder<_type, _tByteAlloc>
{
public:
	using Creation = YSLib::IValueHolder::Creation;
	using value_type = _type;

private:
	using base = YSLib::AllocatorHolder<_type, _tByteAlloc>;

	SourceInformation source_information;

public:
	using base::value;

	template<typename... _tParams>
	inline
	SourcedHolder(const SourceName& name, const SourceLocation& src_loc,
		_tParams&&... args)
		: base(yforward(args)...), source_information(name, src_loc)
	{}
	template<typename... _tParams>
	inline
	SourcedHolder(const SourceInformation& src_info, _tParams&&... args)
		: base(yforward(args)...), source_information(src_info)
	{}
	SourcedHolder(const SourcedHolder&) = default;
	SourcedHolder(SourcedHolder&&) = default;

	SourcedHolder&
	operator=(const SourcedHolder&) = default;
	SourcedHolder&
	operator=(SourcedHolder&&) = default;

	YB_ATTR_nodiscard any
	Create(Creation c, const any& x) const ImplI(IValueHolder)
	{
		return YSLib::AllocatedHolderOperations<SourcedHolder,
			_tByteAlloc>::CreateHolder(c, x, value, YSLib::forward_as_tuple(
			source_information, ystdex::as_const(value)),
			YSLib::forward_as_tuple(source_information, std::move(value)));
	}

	YB_ATTR_nodiscard YB_PURE any
	Query(uintmax_t) const noexcept override
	{
		return ystdex::ref(source_information);
	}

	using base::get_allocator;
};


template<typename _func = lref<const ContextHandler>>
struct GLContinuation final
{
	_func Handler;

	GLContinuation(_func h) noexcept(noexcept(std::declval<_func>()))
		: Handler(h)
	{}

	ReductionStatus
	operator()(Context& ctx) const
	{
		return Handler(ctx.GetNextTermRef(), ctx);
	}
};

template<typename _func>
YB_ATTR_nodiscard YB_PURE inline GLContinuation<_func>
MakeGLContinuation(_func f) noexcept(noexcept(std::declval<_func>()))
{
	return GLContinuation<_func>(f);
}


ReductionStatus
CombinerReturnThunk(const ContextHandler& h, TermNode& term, Context& ctx)
{
	ctx.ClearCombiningTerm();
	ctx.SetNextTermRef(term);
	return RelaySwitched(ctx, GLContinuation<>(h));
}

YB_NORETURN ReductionStatus
ThrowCombiningFailure(TermNode& term, const Context& ctx, const TermNode& fm,
	bool has_ref)
{
	const auto p(ctx.TryGetTailOperatorName(term));

	throw ListReductionFailure(ystdex::sfmt(
		"No matching combiner '%s%s%s' for operand '%s'.", p ? p->c_str() : "",
		p ? ": " : "", TermToStringWithReferenceMark(fm, has_ref).c_str(),
		TermToStringWithReferenceMark(term, {}, 1).c_str()));
}

ReductionStatus
ReduceCombining(TermNode& term, Context& ctx)
{
	assert(IsCombiningTerm(term) && "Invalid term found.");
	AssertValueTags(term);
	if(!IsSingleElementList(term))
	{
		AssertNextTerm(ctx, term);
		ctx.LastStatus = ReductionStatus::Neutral;
		if(IsEmpty(AccessFirstSubterm(term)))
			RemoveHead(term);
		assert(IsBranch(term));
		ctx.SetCombiningTermRef(term);
		return ReduceSubsequent(AccessFirstSubterm(term), ctx,
			Unilang::NameTypedReducerHandler(std::bind(ReduceCombinedBranch,
			std::ref(term), std::placeholders::_1), "eval-combine-operands"));
	}

	// NOTE: The following is necessary to prevent unbounded overflow in
	//	handling recursive subterms.
	auto term_ref(ystdex::ref(term));

	ystdex::retry_on_cond([&]{
		return IsSingleElementList(term_ref);
	}, [&]{
		term_ref = AccessFirstSubterm(term_ref);
	});
	return ReduceOnceLifted(term, ctx, term_ref);
}


inline ReductionStatus
ReduceChildrenOrderedAsync(TNIter, TNIter, Context&);

ReductionStatus
ReduceChildrenOrderedAsyncUnchecked(TNIter first, TNIter last, Context& ctx)
{
	assert(first != last && "Invalid range found.");

	auto& term(*first++);

	return first != last ? ReduceSubsequent(term, ctx, NameTypedReducerHandler(
		MakeGLContinuation([first, last](TermNode&, Context& c){
		return ReduceChildrenOrderedAsync(first, last, c);
	}), "eval-argument-list")) : ReduceOnce(term, ctx);
}

inline ReductionStatus
ReduceChildrenOrderedAsync(TNIter first, TNIter last, Context& ctx)
{
	return first != last ? ReduceChildrenOrderedAsyncUnchecked(first, last, ctx)
		: ReductionStatus::Neutral;
}

ReductionStatus
ReduceCallArguments(TermNode& term, Context& ctx)
{
	assert(!term.empty() && "Invalid term found.");
	ReduceChildrenOrderedAsyncUnchecked(std::next(term.begin()), term.end(),
		ctx);
	return ReductionStatus::Partial;
}


struct EvalSequence final
{
	lref<TermNode> TermRef;
	TNIter Iter;

	ReductionStatus
	operator()(Context&) const;
};

ReductionStatus
ReduceSequenceOrderedAsync(TermNode& term, Context& ctx, TNIter i)
{
	assert(i != term.end() && "Invalid iterator found for sequence reduction.");
	return std::next(i) == term.end() ? ReduceOnceLifted(term, ctx, *i)
		: ReduceSubsequent(*i, ctx,
		NameTypedReducerHandler(EvalSequence{term, i}, "eval-sequence"));
}

ReductionStatus
EvalSequence::operator()(Context& ctx) const
{
	return ReduceSequenceOrderedAsync(TermRef, ctx, TermRef.get().erase(Iter));
}


inline void
CopyTermTags(TermNode& term, const TermNode& tm) noexcept
{
	term.Tags = GetLValueTagsOf(tm.Tags);
}

YB_ATTR_nodiscard YB_STATELESS constexpr TermTags
BindReferenceTags(TermTags ref_tags) noexcept
{
	return bool(ref_tags & TermTags::Unique) ? ref_tags | TermTags::Temporary
		: ref_tags;
}
YB_ATTR_nodiscard YB_PURE inline TermTags
BindReferenceTags(const TermReference& ref) noexcept
{
	return BindReferenceTags(GetLValueTagsOf(ref.GetTags()));
}


YB_NORETURN inline void
ThrowFormalParameterTypeError(const TermNode& term, bool has_ref,
	size_t n_skip = 0)
{
	ThrowTypeErrorForInvalidType(type_id<TokenValue>(), term, has_ref, n_skip);
}

void
ThrowNestedParameterTreeMismatch()
{
	std::throw_with_nested(ParameterMismatch("Failed initializing the operand"
		" in a parameter tree (expected a list, a symbol or '#ignore')."));
}

template<typename _func>
inline void
HandleOrIgnore(_func f, const TermNode& t, bool has_ref)
{
	if(const auto p = TermToNamePtr(t))
		f(*p);
	else if(!IsIgnore(t))
		ThrowFormalParameterTypeError(t, has_ref);
}


template<typename _fBindValue>
class GParameterValueMatcher final
{
public:
	_fBindValue BindValue;

private:
	mutable Action act{};

public:
	template<class _type>
	GParameterValueMatcher(_type&& arg)
		: BindValue(yforward(arg))
	{}

	void
	operator()(const TermNode& t) const
	{
		try
		{
			Match(t, {});
			while(act)
			{
				const auto a(std::move(act));

				a();
			}
		}
		catch(ParameterMismatch&)
		{
			throw;
		}
		catch(...)
		{
			ThrowNestedParameterTreeMismatch();
		}
	}

private:
	YB_FLATTEN void
	Match(const TermNode& t, bool has_ref) const
	{
		if(IsList(t))
		{
			if(IsBranch(t))
				MatchSubterms(t.begin(), t.end());
		}
		else if(const auto p_t = TryAccessLeafAtom<const TermReference>(t))
		{
			auto& nd(p_t->get());

			ystdex::update_thunk(act, [&]{
				Match(nd, true);
			});
		}
		else
			HandleOrIgnore(std::ref(BindValue), t, has_ref);
	}

	void
	MatchSubterms(TNCIter i, TNCIter last) const
	{
		if(i != last)
		{
			ystdex::update_thunk(act, [this, i, last]{
				return MatchSubterms(std::next(i), last);
			});
			Match(Unilang::Deref(i), {});
		}
	}
};

template<typename _fBindValue>
YB_ATTR_nodiscard inline GParameterValueMatcher<_fBindValue>
MakeParameterValueMatcher(_fBindValue bind_value)
{
	return GParameterValueMatcher<_fBindValue>(std::move(bind_value));
}

struct BindAdd final
{
	BindingMap& m;
	string_view& id;

	void
	operator()(const TermNode& tm) const
	{
		CopyTermTags(Environment::Bind(m, id, tm), tm);
	}
	TermNode&
	operator()(TermNode::Container&& c, ValueObject&& vo) const
	{
		return Environment::Bind(m, id, TermNode(std::move(c), std::move(vo)));
	}
};

struct BindInsert final
{
	TermNode::Container& tcon;

	void
	operator()(const TermNode& tm) const
	{
		CopyTermTags(tcon.emplace_back(tm.GetContainer(), tm.Value), tm);
	}
	TermNode&
	operator()(TermNode::Container&& c, ValueObject&& vo) const
	{
		tcon.emplace_back(std::move(c), std::move(vo));
		return tcon.back();
	}
};

class BindParameterObject final
{
public:
	lref<const EnvironmentReference> Referenced;

private:
	char sigil;

public:
	BindParameterObject(const EnvironmentReference& r_env, char s) noexcept
		: Referenced(r_env), sigil(s)
	{}

	template<typename _fInit>
	void
	operator()(TermTags o_tags, TermNode& o, _fInit init) const
	{
		const bool temp(bool(o_tags & TermTags::Temporary));

		if(sigil != '@')
		{
			const bool can_modify(!bool(o_tags & TermTags::Nonmodifying));
			const auto a(o.get_allocator());

			if(const auto p = TryAccessLeafAtom<TermReference>(o))
			{
				if(sigil != char())
				{
					const auto ref_tags(PropagateTo(sigil == '&'
						? BindReferenceTags(*p) : p->GetTags(), o_tags));

					if(can_modify && temp)
						init(std::move(o.GetContainerRef()),
							ValueObject(in_place_type<TermReference>, ref_tags,
							std::move(*p)));
					else
						init(TermNode::Container(o.GetContainer(), a),
							ValueObject(in_place_type<TermReference>, ref_tags,
							*p));
				}
				else
				{
					auto& src(p->get());

					if(!p->IsMovable())
						init(src);
					else
						init(std::move(src.GetContainerRef()),
							std::move(src.Value));
				}
			}
			else if((can_modify || sigil == '%') && temp)
				MarkTemporaryTerm(init(std::move(o.GetContainerRef()),
					std::move(o.Value)));
			else if(sigil == '&')
				init(TermNode::Container(a), ValueObject(std::allocator_arg, a,
					in_place_type<TermReference>,
					GetLValueTagsOf(o.Tags | o_tags), o, Referenced));
			else
				init(o);
		}
		else if(!temp)
			init(TermNode::Container(o.get_allocator()),
				ValueObject(std::allocator_arg, o.get_allocator(),
				in_place_type<TermReference>, o_tags & TermTags::Nonmodifying,
				o, Referenced));
		else
			throw
				InvalidReference("Invalid operand found on binding sigil '@'.");
	}
	template<typename _fInit>
	void
	operator()(TermTags o_tags, TermNode& o, TNIter first, _fInit init) const
	{
		const bool temp(bool(o_tags & TermTags::Temporary));
		const auto bind_subpair_val_fwd(
			[&](TermNode& src, TNIter j, TermTags tags) -> TermNode&{
			assert((sigil == char() || sigil == '%') && "Invalid sigil found.");

			auto t(CreateForBindSubpairPrefix(src, j, tags));

			if(src.Value)
				BindSubpairCopySuffix(t, src, j);
			return init(std::move(t.GetContainerRef()), std::move(t.Value));
		});
		const auto bind_subpair_ref_at([&](TermTags tags){
			assert((sigil == '&' || sigil == '@') && "Invalid sigil found.");

			const auto a(o.get_allocator());
			auto t(CreateForBindSubpairPrefix(o, first, tags));

			if(o.Value)
				LiftTermRef(t.Value, o.Value);

			auto p_sub(Unilang::AllocateSharedTerm(a, std::move(t)));
			auto& sub(*p_sub);
			auto& tcon(t.GetContainerRef());

			tcon.clear();
			tcon.push_back(MakeSubobjectReferent(a, std::move(p_sub)));
			init(std::move(tcon), ValueObject(std::allocator_arg, a,
				in_place_type<TermReference>, tags, sub, Referenced));
		});

		if(sigil != '@')
		{
			const auto a(o.get_allocator());
			const bool can_modify(!bool(o_tags & TermTags::Nonmodifying));

			if(const auto p = TryAccessLeaf<TermReference>(o))
			{
				if(sigil != char())
				{
					const auto ref_tags(PropagateTo(sigil == '&'
						? BindReferenceTags(*p) : p->GetTags(), o_tags));

					if(can_modify && temp)
						init(MoveSuffix(o, first),
							ValueObject(std::allocator_arg, a, in_place_type<
							TermReference>, ref_tags, std::move(*p)));
					else
						init(TermNode::Container(first, o.end(),
							o.get_allocator()), ValueObject(std::allocator_arg,
							a, in_place_type<TermReference>, ref_tags, *p));
				}
				else
				{
					auto& src(p->get());

					if(!p->IsMovable())
						CopyTermTags(bind_subpair_val_fwd(src, src.begin(),
							GetLValueTagsOf(o_tags & ~TermTags::Unique)), src);
					else
						init(MoveSuffix(o, first), std::move(src.Value));
				}
			}
			else if((can_modify || sigil == '%')
				&& (temp || bool(o_tags & TermTags::Unique)))
			{
				if(sigil == char())
					LiftPrefixToReturn(o, first);
				MarkTemporaryTerm(init(MoveSuffix(o, first),
					std::move(o.Value)));
			}
			else if(sigil == '&')
				bind_subpair_ref_at(GetLValueTagsOf(o.Tags | o_tags));
			else
				MarkTemporaryTerm(bind_subpair_val_fwd(o, first, o_tags));
		}
		else if(!temp)
			bind_subpair_ref_at(o_tags & TermTags::Nonmodifying);
		else
			throw
				InvalidReference("Invalid operand found on binding sigil '@'.");
	}

private:
	static void
	BindSubpairCopySuffix(TermNode& t, TermNode& o, TNIter& j)
	{
		while(j != o.end())
			t.emplace(*j++);
		t.Value = ValueObject(o.Value);
	}

	void
	BindSubpairPrefix(TermNode::Container& tcon, TermNode& o, TNIter& j,
		TermTags tags) const
	{
		assert(!bool(tags & TermTags::Temporary)
			&& "Unexpected temporary tag found.");
		for(; j != o.end() && !IsSticky(j->Tags); ++j)
			BindSubpairSubterm(tcon, tags, Unilang::Deref(j));
	}

	void
	BindSubpairSubterm(TermNode::Container& tcon, TermTags o_tags, TermNode& o)
		const
	{
		assert(!bool(o_tags & TermTags::Temporary)
			&& "Unexpected temporary tag found.");

		const BindInsert init{tcon};

		if(sigil != '@')
		{
			const auto a(o.get_allocator());

			if(const auto p = TryAccessLeafAtom<TermReference>(o))
			{
				if(sigil != char())
					init(TermNode::Container(o.GetContainer(), a),
						ValueObject(in_place_type<TermReference>,
						PropagateTo(p->GetTags(), o_tags), *p));
				else
				{
					auto& src(p->get());

					if(!p->IsMovable())
						init(src);
					else
						init(std::move(src.GetContainerRef()),
							std::move(src.Value));
				}
			}
			else if(sigil == '&')
				init(TermNode::Container(a), ValueObject(std::allocator_arg, a,
					in_place_type<TermReference>,
					GetLValueTagsOf(o.Tags | o_tags), o, Referenced));
			else
				init(o);
		}
		else
			init(TermNode::Container(o.get_allocator()),
				ValueObject(std::allocator_arg, o.get_allocator(),
				in_place_type<TermReference>, o_tags & TermTags::Nonmodifying,
				o, Referenced));
	}

	YB_ATTR_nodiscard TermNode
	CreateForBindSubpairPrefix(TermNode& o, TNIter first, TermTags tags) const
	{
		assert(!bool(tags & TermTags::Temporary)
			&& "Unexpected temporary tag found.");

		const auto a(o.get_allocator());
		TermNode t(a);
		auto& tcon(t.GetContainerRef());

		BindSubpairPrefix(tcon, o, first, tags);
		if(!o.Value)
			assert(first == o.end() && "Invalid representation found.");
		return t;
	}

	void
	MarkTemporaryTerm(TermNode& term) const noexcept
	{
		if(sigil != char())
			term.Tags |= TermTags::Temporary;
	}

	YB_ATTR_nodiscard static TermNode::Container
	MoveSuffix(TermNode& o, TNIter j)
	{
		TermNode::Container tcon(o.get_allocator());

		tcon.splice(tcon.end(), o.GetContainerRef(), j, o.end());
		return tcon;
	}
};


struct ParameterCheck final
{
	using HasReferenceArg = ystdex::true_;

	static void
	CheckBack(const TermNode& t, bool has_ref, bool indirect)
	{
		if(!IsList(t))
			ThrowFormalParameterTypeError(t, has_ref || indirect);
	}

	YB_NORETURN static void
	CheckSuffix(const TermNode& t, bool has_ref, bool indirect)
	{
		ThrowFormalParameterTypeError(t, has_ref || indirect,
			has_ref ? 0 : CountPrefix(t));
	}

	template<typename _func>
	static void
	HandleLeaf(_func f, const TermNode& t, bool has_ref)
	{
		HandleOrIgnore(std::ref(f), t, has_ref);
	}

	template<typename _func>
	static void
	WrapCall(_func f)
	{
		try
		{
			f();
		}
		catch(ParameterMismatch&)
		{
			throw;
		}
		catch(...)
		{
			ThrowNestedParameterTreeMismatch();
		}
	}
};


struct NoParameterCheck final
{
	using HasReferenceArg = ystdex::false_;

	static void
	CheckBack(const TermNode& t, bool)
	{
		yunused(t);
		assert(IsList(t));
	}

	template<typename _func>
	static void
	HandleLeaf(_func f, const TermNode& t)
	{
		if(!IsIgnore(t))
		{
			const auto p(TermToNamePtr(t));

			assert(bool(p) && "Invalid parameter tree found.");

			f(*p);
		}
	}

	static void
	CheckSuffix(const TermNode& t, bool)
	{
		yunused(t);
		assert(false && "Invalid parameter tree found.");
	}

	template<typename _func>
	static void
	WrapCall(_func f)
	{
		f();
	}
};


template<typename _fBindTrailing, typename _fBindValue>
struct GBinder final
{
	_fBindTrailing BindTrailing;
	_fBindValue BindValue;

	template<class _type, class _type2>
	inline
	GBinder(_type&& arg, _type2&& arg2)
		: BindTrailing(yforward(arg)), BindValue(yforward(arg2))
	{}

	YB_ATTR_always_inline void
	operator()(TermNode& o, TNIter first, string_view id, TermTags o_tags,
		const EnvironmentReference& r_env) const
	{
		BindTrailing(o, first, id, o_tags, r_env);
	}
	YB_ATTR_always_inline void
	operator()(const TokenValue& n, TermNode& o, TermTags o_tags,
		const EnvironmentReference& r_env) const
	{
		BindValue(n, o, o_tags, r_env);
	}
};


enum : size_t
{
	PTreeFirst,
	PTreeMid,
	OperandRef,
	OperandFirst,
	OperandTags,
	EnvironmentRef,
	Ellipsis
};

using MatchEntry = std::tuple<TNCIter, TNCIter, lref<TermNode>, TNIter, TermTags,
	lref<const EnvironmentReference>, bool
>;


template<class _tTraits, typename _fBind>
class GParameterMatcher final
{
public:
	_fBind Bind;

private:
	mutable stack<MatchEntry, vector<MatchEntry>> remained;

public:
	template<typename... _tParams>
	inline
	GParameterMatcher(TermNode::allocator_type a, _tParams&&... args)
		: Bind(yforward(args)...), remained(a)
	{}

	void
	operator()(const TermNode& t, TermNode& o, TermTags o_tags,
		const EnvironmentReference& r_env) const
	{
		_tTraits::WrapCall([&]{
			Dispatch(ystdex::true_(), typename _tTraits::HasReferenceArg(),
				ystdex::false_(), t, o, o_tags, r_env);
			while(!remained.empty())
			{
				auto& e(remained.top());
				auto& i(std::get<PTreeFirst>(e));

				if(i == std::get<PTreeMid>(e))
				{
					MatchTrailing(e);
					remained.pop();
				}
				else
				{
					auto& j(std::get<OperandFirst>(e));

					assert(!IsSticky(i->Tags)
						&& "Invalid representation found."),
					assert(!IsSticky(j->Tags)
						&& "Invalid representation found."),
					assert(j != std::get<OperandRef>(e).get().end()
						&& "Invalid state of operand found.");
					Dispatch(ystdex::true_(),
						typename _tTraits::HasReferenceArg(), ystdex::false_(),
						Unilang::Deref(i++), Unilang::Deref(j++),
						std::get<OperandTags>(e), std::get<EnvironmentRef>(e));
				}

			}
		});
	}

private:
	template<class _tEnableIndirect, class _tArg, typename... _tParams>
	inline void
	Dispatch(_tEnableIndirect, ystdex::true_, _tArg, _tParams&&... args) const
	{
		Match(_tEnableIndirect(), yforward(args)..., _tArg::value);
	}
	template<class _tEnableIndirect, class _tArg, typename... _tParams>
	inline void
	Dispatch(_tEnableIndirect, ystdex::false_, _tArg, _tParams&&... args) const
	{
		Match(_tEnableIndirect(), yforward(args)...);
	}

	template<typename... _tParams>
	inline void
	Match(ystdex::true_, const TermNode& t, _tParams&&... args) const
	{
		if(IsPair(t))
			MatchPair(t, yforward(args)...);
		else if(IsEmpty(t))
			ThrowIfNonempty(yforward(args)...);
		else if(const auto p_t = TryAccessLeaf<const TermReference>(t))
			MatchIndirect(p_t->get(), yforward(args)...);
		else
			MatchLeaf(t, yforward(args)...);
	}
	template<typename... _tParams>
	inline void
	Match(ystdex::false_, const TermNode& t, _tParams&&... args) const
	{
		if(IsPair(t))
			MatchPair(t, yforward(args)...);
		else if(IsEmpty(t))
			ThrowIfNonempty(yforward(args)...);
		else
			MatchLeaf(t, yforward(args)...);
	}

	template<typename... _tParams>
	inline void
	MatchIndirect(const TermNode& t, TermNode& o, TermTags o_tags,
		const EnvironmentReference& r_env, _tParams&&...) const
	{
		Dispatch(ystdex::false_(), typename _tTraits::HasReferenceArg(),
			ystdex::true_(), t, o, o_tags, r_env);
	}

	template<typename... _tParams>
	inline void
	MatchLeaf(const TermNode& t, TermNode& o, TermTags o_tags,
		const EnvironmentReference& r_env, _tParams&&... args) const
	{
		assert(!IsList(t) && "Invalid term found.");
		_tTraits::HandleLeaf([&](const TokenValue& n){
			Bind(n, o, o_tags, r_env);
		}, t, yforward(args)...);
	}

	template<typename... _tParams>
	inline void
	MatchPair(const TermNode& t, TermNode& o, TermTags o_tags,
		const EnvironmentReference& r_env, _tParams&&... args) const
	{
		assert(IsPair(t) && "Invalid term found.");

		size_t n_p(0);
		auto mid(t.begin());
		const auto is_list(IsList(t));

		while(mid != t.end() && !IsSticky(mid->Tags))
			yunseq(++n_p, ++mid);
		if(is_list && mid == t.end())
		{
			if(n_p != 0)
				ResolveTerm(
					[&](const TermNode& nd, ResolvedTermReferencePtr p_ref){
					if(IsAtom(nd))
					{
						if(const auto p = TryAccessLeaf<TokenValue>(nd))
						{
							if(!p->empty() && p->front() == '.')
								--mid;
						}
						else if(!IsIgnore(nd))
							_tTraits::CheckBack(nd, p_ref, yforward(args)...);
					}
				}, Unilang::Deref(std::prev(mid)));
		}
		else
			ResolveSuffix(
				[&](const TermNode& nd, ResolvedTermReferencePtr p_ref){
				if(YB_UNLIKELY(!(IsTyped<TokenValue>(nd) || IsIgnore(nd))))
					_tTraits::CheckSuffix(nd, p_ref, yforward(args)...);
			}, t);
		ResolveTerm([&, o_tags](TermNode& nd, ResolvedTermReferencePtr p_ref){
			const bool ellipsis(is_list && mid != t.end());

			if(IsPair(nd) || (ellipsis && IsEmpty(nd)))
			{
				if(is_list && !ellipsis && !IsList(nd))
					ThrowListTypeErrorForNonList(nd, p_ref);

				const auto n_o(CountPrefix(nd));

				if(n_p == n_o || (ellipsis && n_o >= n_p - 1))
				{
					auto tags(o_tags);

					if(p_ref)
					{
						const auto ref_tags(p_ref->GetTags());
						tags = (tags
							& ~(TermTags::Unique | TermTags::Temporary))
							| (ref_tags & TermTags::Unique);
						tags = PropagateTo(tags, ref_tags);
					}
					remained.emplace(t.begin(), mid, nd, nd.begin(), tags,
						p_ref ? p_ref->GetEnvironmentReference() : r_env,
						ellipsis);
				}
				else if(!ellipsis)
					throw ArityMismatch(n_p, n_o);
				else
					ThrowInsufficientTermsError(nd, p_ref);
			}
			else
				ThrowListTypeErrorForAtom(nd, p_ref);
		}, o);
	}

	void
	MatchTrailing(MatchEntry& e) const
	{
		auto& o_nd(std::get<OperandRef>(e).get());
		const auto& mid(std::get<PTreeMid>(e));
		if(std::get<Ellipsis>(e))
		{
			const auto& trailing(Unilang::Deref(mid));

			assert(IsAtom(trailing) && "Invalid state found.");
			assert(IsTyped<TokenValue>(ReferenceTerm(trailing))
				&& "Invalid ellipsis sequence token found.");

			string_view id(
				ReferenceTerm(trailing).Value.template GetObject<TokenValue>());

			assert(ystdex::begins_with(id, ".") && "Invalid symbol found.");
			id.remove_prefix(1);
			Bind(std::get<OperandRef>(e), std::get<OperandFirst>(e), id,
				std::get<OperandTags>(e), std::get<EnvironmentRef>(e));
		}
		else if(auto p = TryAccessTerm<TokenValue>(o_nd))
			Bind(o_nd, std::get<OperandFirst>(e), *p, std::get<OperandTags>(e),
				std::get<EnvironmentRef>(e));
	}

	template<typename... _tParams>
	static void
	ThrowIfNonempty(const TermNode& o, _tParams&&...)
	{
		ResolveTerm([&](const TermNode& nd, bool has_ref){
			if(nd)
				throw ParameterMismatch(ystdex::sfmt("Invalid nonempty operand"
					" value '%s' found for empty list parameter.",
					TermToStringWithReferenceMark(nd, has_ref).c_str()));
		}, o);
	}
	template<typename... _tParams>
	static void
	ThrowIfNonempty(const MatchEntry& e, _tParams&&...)
	{
		ThrowIfNonempty(Unilang::Deref(std::get<OperandFirst>(e)));
	}
};

template<class _tTraits, typename _fBind>
YB_ATTR_nodiscard inline GParameterMatcher<_tTraits, _fBind>
MakeParameterMatcher(TermNode::allocator_type a, _fBind bind)
{
	return GParameterMatcher<_tTraits, _fBind>(a, std::move(bind));
}
template<class _tTraits, typename _fBindTrailing, typename _fBindValue>
YB_ATTR_nodiscard inline
	GParameterMatcher<_tTraits, GBinder<_fBindTrailing, _fBindValue>>
MakeParameterMatcher(TermNode::allocator_type a,
	_fBindTrailing bind_trailing_seq, _fBindValue bind_value)
{
	return GParameterMatcher<_tTraits, GBinder<_fBindTrailing, _fBindValue>>(
		a, std::move(bind_trailing_seq), std::move(bind_value));
}


char
ExtractSigil(string_view& id)
{
	if(!id.empty())
	{
		char sigil(id.front());

		if(sigil != '&' && sigil != '%' && sigil != '@')
			sigil = char();
		else
			id.remove_prefix(1);
		return sigil;
	}
	return char();
}


struct DefaultBinder final
{
	lref<BindingMap> MapRef;

	inline
	DefaultBinder(BindingMap& m) noexcept
		: MapRef(m)
	{}

	void
	operator()(TermNode& o_nd, TNIter first, string_view id, TermTags o_tags,
		const EnvironmentReference& r_env) const
	{
		assert((IsPair(o_nd) || IsEmpty(o_nd)) && "Invalid term found.");
		if(!id.empty())
		{
			const char sigil(ExtractSigil(id));

			if(!id.empty())
				Bind(r_env, sigil, id, o_tags, o_nd, first);
		}
	}
	void
	operator()(string_view id, TermNode& o, TermTags o_tags,
		const EnvironmentReference& r_env) const
	{
		Bind(r_env, ExtractSigil(id), id, o_tags, o);
	}

	template<typename... _tParams>
	void
	Bind(const EnvironmentReference& r_env, char sigil, string_view& id,
		_tParams&&... args) const
	{
		BindParameterObject(r_env, sigil)(yforward(args)...,
			BindAdd{MapRef, id});
	}
};


template<class _tTraits>
inline void
BindParameterImpl(BindingMap& m, const TermNode& t, TermNode& o,
	const EnvironmentReference& r_env)
{
	AssertValueTags(o);
	MakeParameterMatcher<_tTraits>(t.get_allocator(), DefaultBinder(m))(t, o,
		TermTags::Temporary, r_env);
}
template<class _tTraits>
inline void
BindParameterImpl(const shared_ptr<Environment>& p_env, const TermNode& t,
	TermNode& o)
{
	BindParameterImpl<_tTraits>(Unilang::Deref(p_env).GetMapCheckedRef(), t, o,
		p_env);
}


struct UTag final
{};


using YSLib::lock_guard;
using YSLib::mutex;
mutex NameTableMutex;

using NameTable = unordered_map<type_index, string_view,
	std::hash<type_index>, ystdex::equal_to<type_index>,
	std::allocator<pair<const type_index, string_view>>>;

template<class _tKey>
YB_ATTR_nodiscard inline NameTable&
FetchNameTableRef()
{
	return ystdex::parameterize_static_object<NameTable, _tKey>();
}

} // unnamed namespace;


ReductionStatus
ReduceOnce(TermNode& term, Context& ctx)
{
	AssertValueTags(term);
	ctx.SetNextTermRef(term);
	return RelayDirect(ctx, ctx.ReduceOnce, term);
}

// NOTE: See Context.h for the declaration.
ReductionStatus
Context::DefaultReduceOnce(TermNode& term, Context& ctx)
{
	AssertValueTags(term);
	return IsCombiningTerm(term) ? ReduceCombining(term, ctx)
		: ReduceLeaf(term, ctx);
}

ReductionStatus
ReduceOrdered(TermNode& term, Context& ctx)
{
	if(IsBranch(term))
		return ReduceSequenceOrderedAsync(term, ctx, term.begin());
	term.Value = ValueToken::Unspecified;
	return ReductionStatus::Retained;
}


void
ParseLeaf(TermNode& term, string_view id)
{
	assert(id.data());
	assert(!id.empty() && "Invalid leaf token found.");
	switch(CategorizeBasicLexeme(id))
	{
	case LexemeCategory::Code:
		id = DeliteralizeUnchecked(id);
		term.SetValue(in_place_type<TokenValue>, id, term.get_allocator());
		break;
	case LexemeCategory::Symbol:
		if(ParseSymbol(term, id))
			term.SetValue(in_place_type<TokenValue>, id, term.get_allocator());
		break;
	case LexemeCategory::Data:
		term.SetValue(in_place_type<string>, Deliteralize(id),
			term.get_allocator());
		YB_ATTR_fallthrough;
	default:
		break;
	}
}

void
ParseLeafWithSourceInformation(TermNode& term, string_view id,
	const shared_ptr<string>& name, const SourceLocation& src_loc)
{
	assert(id.data());
	assert(!id.empty() && "Invalid leaf token found.");
	switch(CategorizeBasicLexeme(id))
	{
	case LexemeCategory::Code:
		id = DeliteralizeUnchecked(id);
		term.SetValue(any_ops::use_holder, in_place_type<SourcedHolder<
			TokenValue>>, name, src_loc, id, term.get_allocator());
		break;
	case LexemeCategory::Symbol:
		if(ParseSymbol(term, id))
			term.SetValue(any_ops::use_holder, in_place_type<SourcedHolder<
				TokenValue>>, name, src_loc, id, term.get_allocator());
		break;
	case LexemeCategory::Data:
		term.SetValue(any_ops::use_holder, in_place_type<SourcedHolder<string>>,
			name, src_loc, Deliteralize(id), term.get_allocator());
		YB_ATTR_fallthrough;
	default:
		break;
	}
}


ReductionStatus
FormContextHandler::CallHandler(TermNode& term, Context& ctx) const
{
	return Unilang::RelayCurrentOrDirect(ctx, std::ref(Handler), term);
}

ReductionStatus
FormContextHandler::CallN(size_t n, TermNode& term, Context& ctx) const
{
	if(n == 0 || term.size() <= 1)
		return FormContextHandler::CallHandler(term, ctx);
	return Unilang::RelayCurrentNext(ctx, term, ReduceCallArguments,
		NameTypedReducerHandler([&, n](Context& c){
		c.SetNextTermRef(term);
		return CallN(n - 1, term, c);
	}, "eval-combine-operator"));
}

void
FormContextHandler::CheckArguments(size_t n, const TermNode& term)
{
	if(n != 0)
		CheckArgumentList(term);
	else
		AssertCombiningTerm(term);
}

ReductionStatus
FormContextHandler::DoCall0(const FormContextHandler& fch, TermNode& term,
	Context& ctx)
{
	assert(fch.wrapping == 0 && "Unexpected wrapping count found.");
	AssertNextTerm(ctx, term);
	return fch.CallHandler(term, ctx);
}

ReductionStatus
FormContextHandler::DoCall1(const FormContextHandler& fch, TermNode& term,
	Context& ctx)
{
	assert(fch.wrapping == 1 && "Unexpected wrapping count found.");
	AssertNextTerm(ctx, term);
	return term.size() <= 1 ? fch.CallHandler(term, ctx)
		: Unilang::RelayCurrentNext(ctx, term, ReduceCallArguments,
		NameTypedReducerHandler([&](Context& c){
		c.SetNextTermRef(term);
		return fch.CallHandler(term, c);
	}, "eval-combine-operator"));
}

ReductionStatus
FormContextHandler::DoCallN(const FormContextHandler& fch, TermNode& term,
	Context& ctx)
{
	assert(fch.wrapping > 1 && "Unexpected wrapping count found.");
	return fch.CallN(fch.wrapping, term, ctx);
}

bool
FormContextHandler::Equals(const FormContextHandler& fch) const
{
	if(wrapping == fch.wrapping)
	{
		if(Handler == fch.Handler)
			return true;
		if(const auto p = Handler.target<RefContextHandler>())
			return p->HandlerRef.get() == fch.Handler;
		if(const auto p = fch.Handler.target<RefContextHandler>())
			return Handler == p->HandlerRef.get();
	}
	return {};
}


inline namespace Internals
{

ReductionStatus
ReduceAsSubobjectReference(TermNode& term, shared_ptr<TermNode> p_sub,
	const EnvironmentReference& r_env, TermTags tags)
{
	assert(bool(p_sub)
		&& "Invalid subterm to form a subobject reference found.");

	auto& con(term.GetContainerRef());
	auto i(con.begin());

	term.SetValue(TermReference(tags, Unilang::Deref(p_sub), r_env)),
	term.Tags = TermTags::Unqualified;
	con.insert(i,
		Unilang::MakeSubobjectReferent(con.get_allocator(), std::move(p_sub)));		
	con.erase(i, con.end());
	return ReductionStatus::Retained;
}

ReductionStatus
ReduceForCombinerRef(TermNode& term, const TermReference& ref,
	const ContextHandler& h, size_t n)
{
	const auto& r_env(ref.GetEnvironmentReference());
	const auto a(term.get_allocator());

	return ReduceAsSubobjectReference(term, Unilang::allocate_shared<TermNode>(
		a, Unilang::AsTermNode(a, ContextHandler(std::allocator_arg, a,
		FormContextHandler(RefContextHandler(h, r_env), n)))), r_env,
		ref.GetTags());
}

} // inline namespace Internals;


ReductionStatus
ReduceLeaf(TermNode& term, Context& ctx)
{
	const auto res(ystdex::call_value_or([&](string_view id){
		try
		{
			return EvaluateLeafToken(term, ctx, id);
		}
		catch(BadIdentifier& e)
		{
			if(const auto p_si = QuerySourceInformation(term.Value))
				e.Source = *p_si;
			throw;
		}
	}, TermToNamePtr(term), ReductionStatus::Retained));

	return CheckReducible(res) ? ReduceOnce(term, ctx) : res;
}

ReductionStatus
ReduceCombinedBranch(TermNode& term, Context& ctx)
{
	assert(IsCombiningTerm(term) && "Invalid term found for combined term.");

	auto& fm(AccessFirstSubterm(term));
	const auto p_ref_fm(TryAccessLeafAtom<const TermReference>(fm));

	if(p_ref_fm)
	{
		ClearCombiningTags(term);
		if(const auto p_handler
			= TryAccessLeafAtom<const ContextHandler>(p_ref_fm->get()))
		{
			EnsureTCOAction(ctx, term).AddOperator(ctx.OperatorName);
			return CombinerReturnThunk(*p_handler, term, ctx);
		}
	}
	else
		term.Tags |= TermTags::Temporary;
	if(const auto p_handler = TryAccessTerm<ContextHandler>(fm))
	{
		yunused(EnsureTCOAction(ctx, term));
		return
			CombinerReturnThunk(EnsureTCOAction(ctx, term).Attach(
				fm.Value).GetObject<ContextHandler>(), term, ctx);
	}
	assert(IsBranch(term));
	return ResolveTerm(std::bind(ThrowCombiningFailure, std::ref(term),
		std::ref(ctx), std::placeholders::_1, std::placeholders::_2), fm);
}


void
CheckParameterTree(const TermNode& term)
{
	MakeParameterValueMatcher([&](const TokenValue&) noexcept{})(term);
}

void
BindParameter(BindingMap& m, const TermNode& t, TermNode& o,
	const EnvironmentReference& r_env)
{
	BindParameterImpl<ParameterCheck>(m, t, o, r_env);
}
void
BindParameter(const shared_ptr<Environment>& p_env, const TermNode& t,
	TermNode& o)
{
	BindParameterImpl<ParameterCheck>(p_env, t, o);
}

void
BindParameterWellFormed(BindingMap& m, const TermNode& t, TermNode& o,
	const EnvironmentReference& r_env)
{
	BindParameterImpl<NoParameterCheck>(m, t, o, r_env);
}
void
BindParameterWellFormed(const shared_ptr<Environment>& p_env, const TermNode& t,
	TermNode& o)
{
	BindParameterImpl<NoParameterCheck>(p_env, t, o);
}

void
BindSymbol(BindingMap& m, const TokenValue& n, TermNode& o,
	const EnvironmentReference& r_env)
{
	AssertValueTags(o);
	DefaultBinder{m}(n, o, TermTags::Temporary, r_env);
}
void
BindSymbol(const shared_ptr<Environment>& p_env, const TokenValue& n,
	TermNode& o)
{
	auto& env(Unilang::Deref(p_env));

	BindSymbol(env.GetMapCheckedRef(), n, o,
		EnvironmentReference(p_env, env.GetAnchorPtr()));
}


bool
AddTypeNameTableEntry(const type_info& ti, string_view sv)
{
	assert(sv.data());

	const lock_guard<mutex> gd(NameTableMutex);

	return FetchNameTableRef<UTag>().emplace(std::piecewise_construct,
		YSLib::forward_as_tuple(ti), YSLib::forward_as_tuple(sv)).second;
}

string_view
QueryContinuationName(const Reducer& act)
{
	if(IsTyped<GLContinuation<>>(act.target_type()))
		return QueryTypeName(type_id<ContextHandler>());
	if(const auto p_cont = act.target<Continuation>())
		return QueryTypeName(p_cont->Handler.target_type());
	if(act.target_type() == type_id<TCOAction>())
		return "eval-tail";
	if(act.target_type() == type_id<EvalSequence>())
		return "eval-sequence";
	return QueryTypeName(act.target_type());
}

const SourceInformation*
QuerySourceInformation(const ValueObject& vo)
{
	const auto val(vo.Query());

	return ystdex::call_value_or([](const SourceInfoMetadata& r) noexcept{
		return &r.get();
	}, val.try_get_object_ptr<SourceInfoMetadata>());
}

string_view
QueryTypeName(const type_info& ti)
{
	const lock_guard<mutex> gd(NameTableMutex);
	const auto& tbl(FetchNameTableRef<UTag>());
	const auto i(tbl.find(ti));

	if(i != tbl.cend())
		return i->second;
	return {};
}

void
TraceAction(const Reducer& act, YSLib::Logger& trace)
{
	using YSLib::Notice;
	const auto name(Unilang::QueryContinuationName(act));
	const auto p(name.data() ? name.data() :
#if NDEBUG
		"?"
#else
		ystdex::call_value_or([](const Continuation& cont) -> const type_info&{
			return cont.Handler.target_type();
		}, act.target<Continuation>(), act.target_type()).name()
#endif
	);
	const auto print_cont([&]{
		trace.TraceFormat(Notice, "#[continuation (%s)]", p);
	});
	if(const auto p_act = act.target<TCOAction>())
	{
		for(const auto& r : p_act->GetFrameRecordList())
		{
			const auto& op(std::get<ActiveCombiner>(r));

			if(IsTyped<TokenValue>(op))
			{
				if(const auto p_opn_t = op.AccessPtr<TokenValue>())
				{
					const auto p_o(p_opn_t->data());
#	if true
					if(const auto p_si = QuerySourceInformation(op))
						trace.TraceFormat(Notice, "#[continuation: %s (%s) @ %s"
							" (line %zu, column %zu)]", p_o, p, p_si->first
							? p_si->first->c_str() : "<unknown>",
							p_si->second.Line + 1, p_si->second.Column + 1);
					else
#	endif
						trace.TraceFormat(Notice, "#[continuation: %s (%s)]",
							p_o, p);
				}
				else
					print_cont();
			}
			else
				print_cont();
		}
	}
	print_cont();
}

void
TraceBacktrace(Context::ReducerSequence::const_iterator first,
	Context::ReducerSequence::const_iterator last, YSLib::Logger& trace)
	noexcept
{
	if(first != last)
	{
		YSLib::FilterExceptions([&]{
			trace.TraceFormat(YSLib::Notice, "Backtrace:");
			for(; first != last; ++first)
				TraceAction(Unilang::Deref(first), trace);
		}, "guard unwinding for backtrace");
	}
}

} // namespace Unilang;

