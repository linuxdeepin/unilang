// © 2021 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_TCO_h_
#define INC_Unilang_TCO_h_ 1

#include "Evaluation.h" // for lref, map, set, weak_ptr, shared_ptr,
//	Environment, ContextHandler, list, TermNode, EnvironmentGuard,
//	ReductionStatus, Context;
#include <ystdex/functional.hpp> // for ystdex::get_less;
#include <set> // for std::set;
#include <ystdex/scope_guard.hpp> // for ystdex::unique_guard;
#include <tuple> // for std::tuple;

namespace Unilang
{

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

	[[nodiscard, gnu::pure]] static size_t
	CountReferences(const shared_ptr<Environment>&) noexcept;

	[[nodiscard, gnu::pure]] static size_t
	CountStrong(const shared_ptr<Environment>&) noexcept;

	template<typename _fTracer>
	static void
	Traverse(Environment& e, ValueObject& parent, const _fTracer& trace)
	{
		const auto& tp(parent.type());

		if(tp == ystdex::type_id<EnvironmentList>())
		{
			for(auto& vo : parent.GetObject<EnvironmentList>())
				Traverse(e, vo, trace);
		}
		else if(tp == ystdex::type_id<EnvironmentReference>())
		{
			if(auto p = parent.GetObject<EnvironmentReference>().Lock())
				TraverseForSharedPtr(e, parent, trace, p);
		}
		else if(tp == ystdex::type_id<shared_ptr<Environment>>())
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
			auto& dst(*p);

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

		PDefHOp(void, (), ) const ynothrow
			ImplExpr(TermRef.get().Clear())
	};

	mutable decltype(ystdex::unique_guard(std::declval<GuardFunction>()))
		term_guard;
	mutable size_t req_lift_result = 0;
	mutable list<ContextHandler> xgds;

public:
	mutable const ContextHandler* LastFunction{};
	mutable EnvironmentGuard EnvGuard;
	mutable FrameRecordList RecordList;

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

	[[nodiscard]] lref<const ContextHandler>
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

	[[nodiscard]] ContextHandler
	MoveFunction();

	void
	SetupLift() const;
	void
	SetupLift(bool lift) const
	{
		if(lift)
			SetupLift();
	}
};

} // namespace Unilang;

#endif

