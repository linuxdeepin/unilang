// © 2021-2022 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_TCO_h_
#define INC_Unilang_TCO_h_ 1

#include "Evaluation.h" // for shared_ptr, Context, YSLib::allocate_shared,
//	Unilang::Deref, UnilangException, map, lref, Environment, size_t, set,
//	weak_ptr, IsTyped, ContextHandler, list, TermNode, EnvironmentGuard,
//	ReductionStatus, NameTypedContextHandler;
#include <ystdex/functional.hpp> // for ystdex::get_less, ystdex::bind1;
#include <set> // for std::set;
#include <tuple> // for std::tuple;
#include <ystdex/scope_guard.hpp> // for ystdex::unique_guard;
#include <ystdex/optional.h> // for ystdex::optional;
#include <functional> // for std::bind, std::placeholder, std::ref;
#include <cassert> // for assert;

namespace Unilang
{

class OneShotChecker final
{
private:
	shared_ptr<bool> p_shot;

public:
	OneShotChecker(Context& ctx)
		: p_shot(YSLib::allocate_shared<bool>(ctx.get_allocator()))
	{}

	void
	operator()() const ynothrow
	{
		if(p_shot)
			Unilang::Deref(p_shot) = true;
	}

	void
	Check() const
	{
		auto& shot(Unilang::Deref(p_shot));

		if(!shot)
			shot = true;
		else
			throw UnilangException("One-shot continuation expired.");
	}
};


struct RecordCompressor final
{
	using RecordInfo = map<lref<Environment>, size_t, ystdex::get_less<>>;
	using ReferenceSet = set<lref<Environment>, ystdex::get_less<>>;

	weak_ptr<Environment> RootPtr;
	ReferenceSet Reachable, NewlyReachable;
	RecordInfo Universe;

	RecordCompressor(const shared_ptr<Environment>&);
	RecordCompressor(const shared_ptr<Environment>&,
		Environment::allocator_type);

	void
	AddParents(Environment&);

	void
	Compress();

	YB_ATTR_nodiscard YB_PURE static size_t
	CountReferences(const shared_ptr<Environment>&) noexcept;

	YB_ATTR_nodiscard YB_PURE static size_t
	CountStrong(const shared_ptr<Environment>&) noexcept;

	template<typename _fTracer>
	static void
	Traverse(Environment& e, ValueObject& parent, const _fTracer& trace)
	{
		const auto& tp(parent.type());

		if(IsTyped<EnvironmentList>(tp))
		{
			for(auto& vo : parent.GetObject<EnvironmentList>())
				Traverse(e, vo, trace);
		}
		else if(IsTyped<EnvironmentReference>(tp))
		{
			if(auto p = parent.GetObject<EnvironmentReference>().Lock())
				TraverseForSharedPtr(e, parent, trace, p);
		}
		else if(IsTyped<shared_ptr<Environment>>(tp))
		{
			if(auto p = parent.GetObject<shared_ptr<Environment>>())
				TraverseForSharedPtr(e, parent, trace, p);
		}
	}

private:
	template<typename _fTracer>
	static void
	TraverseForSharedPtr(Environment& e, ValueObject& parent,
		const _fTracer& trace, shared_ptr<Environment>& p)
	{
		if(ystdex::expand_proxy<void(const shared_ptr<Environment>&,
			Environment&, ValueObject&)>::call(trace, p, e, parent))
		{
			auto& dst(Unilang::Deref(p));

			p.reset();
			Traverse(dst, dst.Parent, trace);
		}
	}
};



enum RecordFrameIndex : size_t
{
	ActiveCombiner,
	ActiveEnvironmentPtr
};

using FrameRecord = std::tuple<ContextHandler, shared_ptr<Environment>>;

using FrameRecordList = list<FrameRecord>;

class TCOAction final
{
private:
	struct GuardFunction final
	{
		lref<TermNode> TermRef;

		void
		operator()() const noexcept
		{
			TermRef.get().Clear();
		}
	};

	mutable decltype(ystdex::unique_guard(std::declval<GuardFunction>()))
		term_guard;
	mutable size_t req_lift_result = 0;
	mutable list<ContextHandler> xgds;

public:
	mutable const ContextHandler* LastFunction{};
	mutable EnvironmentGuard EnvGuard;
	mutable FrameRecordList RecordList;
	mutable ValueObject OperatorName;

private:
	mutable ystdex::optional<ystdex::guard<OneShotChecker>> one_shot_guard{};

public:
	TCOAction(Context&, TermNode&, bool);
	TCOAction(const TCOAction&);
	TCOAction(TCOAction&&) = default;

	TCOAction&
	operator=(TCOAction&&) = default;

	ReductionStatus
	operator()(Context&) const;

	TermNode&
	GetTermRef() const noexcept
	{
		return term_guard.func.func.TermRef;
	}

	void
	AddRecord(shared_ptr<Environment>&& p_env)
	{
		RecordList.emplace_front(MoveFunction(), std::move(p_env));
	}

	YB_ATTR_nodiscard lref<const ContextHandler>
	AttachFunction(ContextHandler&&);

	void
	CompressFrameList();

	void
	CompressForContext(Context& ctx)
	{
		CompressFrameList();
		RecordCompressor(ctx.GetRecordPtr()).Compress();
	}

	void
	CompressForGuard(Context&, EnvironmentGuard&&);

	YB_ATTR_nodiscard OneShotChecker
	MakeOneShotChecker()
	{
		if(!one_shot_guard.has_value())
			one_shot_guard.emplace(EnvGuard.func.ContextRef.get());
		return (*one_shot_guard).func;
	}

	YB_ATTR_nodiscard ContextHandler
	MoveFunction();

	void
	ReleaseOneShotGuard()
	{
		return one_shot_guard.reset();
	}

	void
	SetupLift() const;
	void
	SetupLift(bool lift) const
	{
		if(lift)
			SetupLift();
	}
};

YB_ATTR_nodiscard YB_PURE inline TCOAction*
AccessTCOAction(Context& ctx) noexcept
{
	return ctx.AccessCurrentAs<TCOAction>();
}

YB_ATTR_nodiscard TCOAction&
EnsureTCOAction(Context&, TermNode&);

YB_ATTR_nodiscard inline TCOAction&
RefTCOAction(Context& ctx)
{
	return Unilang::Deref(AccessTCOAction(ctx));
}

void
SetupTailTCOAction(Context&, TermNode&, bool);


TCOAction&
PrepareTCOEvaluation(Context&, TermNode&, EnvironmentGuard&&);


inline void
AssertNextTerm(Context& ctx, TermNode& term)
{
	yunused(ctx),
	yunused(term);
	assert(ystdex::ref_eq<>()(term, ctx.GetNextTermRef()));
}


template<typename _fCurrent>
inline ReductionStatus
RelayDirect(Context& ctx, _fCurrent&& cur)
{
	return cur(ctx);
}

template<typename _fCurrent>
inline auto
RelayDirect(Context& ctx, _fCurrent&& cur, TermNode& term)
	-> decltype(cur(term, ctx))
{
	// XXX: See https://gcc.gnu.org/bugzilla/show_bug.cgi?id=93470.
	return ystdex::unref(cur)(term, ctx);
}
inline ReductionStatus
RelayDirect(Context& ctx, const Continuation& cur, TermNode& term)
{
	return cur.Handler(term, ctx);
}

template<typename _fCurrent>
inline ReductionStatus
RelayCurrentOrDirect(Context& ctx, _fCurrent&& cur, TermNode& term)
{
	return Unilang::RelayDirect(ctx, yforward(cur), term);
}

template<typename _fCurrent, typename _fNext>
inline ReductionStatus
RelayCurrentNext(Context& ctx, TermNode& term, _fCurrent&& cur,
	_fNext&& next)
{
	RelaySwitched(ctx, yforward(next));
	return Unilang::RelayDirect(ctx, yforward(cur), term);
}

template<typename _fCurrent, typename _fNext>
inline ReductionStatus
ReduceCurrentNext(TermNode& term, Context& ctx, _fCurrent&& cur,
	_fNext&& next)
{
	ctx.SetNextTermRef(term);
	return Unilang::RelayCurrentNext(ctx, term, yforward(cur), yforward(next));
}


template<typename _fNext>
inline ReductionStatus
ReduceSubsequent(TermNode& term, Context& ctx, _fNext&& next)
{
	return Unilang::ReduceCurrentNext(term, ctx, std::ref(ReduceOnce),
		yforward(next));
}


struct NonTailCall final
{
	template<typename _fCurrent>
	static inline ReductionStatus
	RelayNextGuarded(Context& ctx, TermNode& term, EnvironmentGuard&& gd,
		_fCurrent&& cur)
	{
		return Unilang::RelayCurrentNext(ctx, term, yforward(cur),
			MoveKeptGuard(gd));
	}

	template<typename _fCurrent>
	static inline ReductionStatus
	RelayNextGuardedLifted(Context& ctx, TermNode& term,
		EnvironmentGuard&& gd, _fCurrent&& cur)
	{
		auto act(MoveKeptGuard(gd));
		Continuation cont(NameTypedContextHandler([&]{
			return ReduceForLiftedResult(term);
		}, "eval-lift-result"), ctx);

		RelaySwitched(ctx, std::move(act));
		return Unilang::RelayCurrentNext(ctx, term, yforward(cur),
			std::move(cont));
	}

	template<typename _fCurrent>
	static ReductionStatus
	RelayNextGuardedProbe(Context& ctx, TermNode& term,
		EnvironmentGuard&& gd, bool lift, _fCurrent&& cur)
	{
		auto act(MoveKeptGuard(gd));

		if(lift)
		{
			Continuation cont(NameTypedContextHandler([&]{
				return ReduceForLiftedResult(term);
			}, "eval-lift-result"), ctx);

			RelaySwitched(ctx, std::move(act));
			return Unilang::RelayCurrentNext(ctx, term, yforward(cur),
				std::move(cont));
		}
		return Unilang::RelayCurrentNext(ctx, term, yforward(cur), std::move(act));
	}

	static void
	SetupForNonTail(Context& ctx, TermNode& term)
	{
		assert(!AccessTCOAction(ctx));
		ctx.LastStatus = ReductionStatus::Neutral;
		SetupTailTCOAction(ctx, term, {});
	}
};


struct TailCall final
{
	template<typename _fCurrent>
	static inline ReductionStatus
	RelayNextGuarded(Context& ctx, TermNode& term, EnvironmentGuard&& gd,
		_fCurrent&& cur)
	{
		PrepareTCOEvaluation(ctx, term, std::move(gd));
		return Unilang::RelayCurrentOrDirect(ctx, yforward(cur), term);
	}

	template<typename _fCurrent>
	static inline ReductionStatus
	RelayNextGuardedLifted(Context& ctx, TermNode& term,
		EnvironmentGuard&& gd, _fCurrent&& cur)
	{
		PrepareTCOEvaluation(ctx, term, std::move(gd)).SetupLift();
		return Unilang::RelayCurrentOrDirect(ctx, yforward(cur), term);
	}

	template<typename _fCurrent>
	static inline ReductionStatus
	RelayNextGuardedProbe(Context& ctx, TermNode& term,
		EnvironmentGuard&& gd, bool lift, _fCurrent&& cur)
	{
		PrepareTCOEvaluation(ctx, term, std::move(gd)).SetupLift(lift);
		return Unilang::RelayCurrentOrDirect(ctx, yforward(cur), term);
	}

	static void
	SetupForNonTail(Context&, TermNode&) noexcept
	{}
};


template<class _tTraits>
struct Combine final
{
	static ReductionStatus
	RelayEnvSwitch(Context& ctx, TermNode& term, EnvironmentGuard gd)
	{
		return _tTraits::RelayNextGuarded(ctx, term, std::move(gd),
			std::ref(ReduceCombinedBranch));
	}
	static ReductionStatus
	RelayEnvSwitch(Context& ctx, TermNode& term,
		shared_ptr<Environment> p_env)
	{
		return RelayEnvSwitch(ctx, term, EnvironmentGuard(ctx,
			ctx.SwitchEnvironmentUnchecked(std::move(p_env))));
	}

	template<class _tGuardOrEnv>
	static inline ReductionStatus
	ReduceEnvSwitch(TermNode& term, Context& ctx, _tGuardOrEnv&& gd_or_env)
	{
		_tTraits::SetupForNonTail(ctx, term);
		return RelayEnvSwitch(ctx, term, yforward(gd_or_env));
	}

	template<typename _fNext, class _tGuardOrEnv>
	static ReductionStatus
	ReduceCallSubsequent(TermNode& term, Context& ctx,
		_tGuardOrEnv&& gd_or_env, _fNext&& next)
	{
		return Unilang::ReduceCurrentNext(term, ctx,
			ystdex::bind1([](TermNode& t, Context& c, _tGuardOrEnv& g_e){
			return ReduceEnvSwitch(t, c, std::move(g_e));
		}, std::placeholders::_2, std::move(gd_or_env)), yforward(next));
	}
};


using Action = function<void()>;


} // namespace Unilang;

#endif

