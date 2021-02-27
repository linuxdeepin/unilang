// © 2021 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_TCO_h_
#define INC_Unilang_TCO_h_ 1

#include "Evaluation.h" // for shared_ptr, Environment, ContextHandler, list,
//	TermNode, EnvironmentGuard, ReductionStatus, Context;
#include <ystdex/scope_guard.hpp> // for ystdex::unique_guard;
#include <tuple> // for std::tuple;

namespace Unilang
{

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
	CompressForContext(Context&)
	{
		CompressFrameList();
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

