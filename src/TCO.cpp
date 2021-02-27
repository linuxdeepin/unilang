// © 2021 Uniontech Software Technology Co.,Ltd.

#include "TCO.h" // for std::get;
#include <ystdex/typeinfo.h> // for ystdex::type_id;
#include <cassert> // for assert;
#include "Exception.h" // for UnilangException;

namespace Unilang
{

TCOAction::TCOAction(Context& ctx, TermNode& term, bool lift)
	: term_guard(ystdex::unique_guard(GuardFunction{term})),
	req_lift_result(lift ? 1 : 0), xgds(ctx.get_allocator()), EnvGuard(ctx),
	RecordList(ctx.get_allocator())
{
	assert(term.Value.type() == ystdex::type_id<TokenValue>() || !term.Value);
}
TCOAction::TCOAction(const TCOAction& a)
	: term_guard(std::move(a.term_guard)),
	req_lift_result(a.req_lift_result), xgds(std::move(a.xgds)),
	EnvGuard(std::move(a.EnvGuard))
{}

ReductionStatus
TCOAction::operator()(Context& ctx) const
{
	assert(ystdex::ref_eq<>()(EnvGuard.func.ContextRef.get(), ctx));

	const auto res([&]() -> ReductionStatus{
		if(req_lift_result != 0)
		{
			RegularizeTerm(GetTermRef(), ctx.LastStatus);
			for(; req_lift_result != 0; --req_lift_result)
				LiftToReturn(GetTermRef());
			return ReductionStatus::Retained;
		}
		RegularizeTerm(GetTermRef(), ctx.LastStatus);
		return ctx.LastStatus;
	}());

	ystdex::dismiss(term_guard);
	{
		const auto egd(std::move(EnvGuard));
	}
	while(!xgds.empty())
		xgds.pop_back();
	while(!RecordList.empty())
	{
		auto& front(RecordList.front());

		std::get<ActiveCombiner>(front) = {};
		RecordList.pop_front();
	}
	return res;
}

[[nodiscard]] lref<const ContextHandler>
TCOAction::AttachFunction(ContextHandler&& h)
{
	ystdex::erase_all(xgds, h);
	xgds.emplace_back();
	swap(xgds.back(), h);
	return ystdex::as_const(xgds.back());
}

void
TCOAction::CompressFrameList()
{
	auto i(RecordList.cbegin());

	ystdex::retry_on_cond(ystdex::id<>(), [&]() -> bool{
		const auto orig_size(RecordList.size());

		i = RecordList.cbegin();
		while(i != RecordList.cend())
		{
			auto& p_frame_env_ref(std::get<ActiveEnvironmentPtr>(
				*ystdex::cast_mutable(RecordList, i)));

			if(p_frame_env_ref.use_count() != 1 || p_frame_env_ref->IsOrphan())
				i = RecordList.erase(i);
			else
				++i;
		}
		return RecordList.size() != orig_size;
	});
}

void
TCOAction::CompressForGuard(Context& ctx, EnvironmentGuard&& gd)
{
	if(EnvGuard.func.SavedPtr)
	{
		if(auto& p_saved = gd.func.SavedPtr)
		{
			CompressForContext(ctx);
			AddRecord(std::move(p_saved));
			return;
		}
	}
	else
		EnvGuard = std::move(gd);
	AddRecord({});
}

[[nodiscard]] ContextHandler
TCOAction::MoveFunction()
{
	ContextHandler res(std::allocator_arg, xgds.get_allocator());

	if(LastFunction)
	{
		const auto i(std::find_if(xgds.rbegin(), xgds.rend(),
			[this](const ContextHandler& h) ynothrow{
			return &h == LastFunction;
		}));

		if(i != xgds.rend())
		{
			res = ContextHandler(std::allocator_arg, xgds.get_allocator(),
				std::move(*i));
			xgds.erase(std::next(i).base());
		}
		LastFunction = {};
	}
	return res;
}

void
TCOAction::SetupLift() const
{
	++req_lift_result;
	if(req_lift_result == 0)
		throw UnilangException(
			"TCO action lift request count overflow detected.");
}


} // namespace Unilang;

