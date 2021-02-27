// © 2021 Uniontech Software Technology Co.,Ltd.

#include "TCO.h" // for std::get;
#include <cassert> // for assert;
#include <ystdex/typeinfo.h> // for ystdex::type_id;
#include <ystdex/functional.hpp> // for ystdex::retry_on_cond, ystdex::id;
#include "Exception.h" // for UnilangException;

namespace Unilang
{

RecordCompressor::RecordCompressor(const shared_ptr<Environment>& p_root)
	: RecordCompressor(p_root, p_root->Bindings.get_allocator())
{}
RecordCompressor::RecordCompressor(const shared_ptr<Environment>& p_root,
	Environment::allocator_type a)
	: RootPtr(p_root), Reachable({*p_root}, a), NewlyReachable(a), Universe(a)
{
	AddParents(*p_root);
}

void
RecordCompressor::AddParents(Environment& e)
{
	Traverse(e, e.Parent,
		[this](const shared_ptr<Environment>& p_dst, const Environment&){
		return Universe.emplace(*p_dst, CountReferences(p_dst)).second;
	});
}

void
RecordCompressor::Compress()
{
	const auto p_root(RootPtr.lock());

	assert(bool(p_root));
	for(auto& pr : Universe)
	{
		auto& e(pr.first.get());

		Traverse(e, e.Parent, [this](const shared_ptr<Environment>& p_dst){
			auto& count(Universe.at(*p_dst));

			assert(count > 0);
			--count;
			return false;
		});
	}
	for(auto i(Universe.cbegin()); i != Universe.cend(); )
		if(i->second > 0)
		{
			NewlyReachable.insert(i->first);
			i = Universe.erase(i);
		}
		else
			++i;
	for(ReferenceSet rs; !NewlyReachable.empty();
		NewlyReachable = std::move(rs))
	{
		for(const auto& e : NewlyReachable)
			Traverse(e, e.get().Parent,
				[&](const shared_ptr<Environment>& p_dst) noexcept{
				auto& dst(*p_dst);

				rs.insert(dst);
				Universe.erase(dst);
				return false;
			});
		Reachable.insert(std::make_move_iterator(NewlyReachable.begin()),
			std::make_move_iterator(NewlyReachable.end()));
		for(auto i(rs.cbegin()); i != rs.cend(); )
			if(ystdex::exists(Reachable, *i))
				i = rs.erase(i);
			else
				++i;
	}

	ReferenceSet accessed;

	ystdex::retry_on_cond(ystdex::id<>(), [&]() -> bool{
		bool collected = {};

		Traverse(*p_root, p_root->Parent, [&](const shared_ptr<Environment>&
			p_dst, Environment& src, ValueObject& parent) -> bool{
			auto& dst(*p_dst);

			if(accessed.insert(src).second)
			{
				if(!ystdex::exists(Universe, ystdex::ref(dst)))
					return true;
				parent = dst.Parent;
				collected = true;
			}
			return {};
		});
		return collected;
	});
}

size_t
RecordCompressor::CountReferences(const shared_ptr<Environment>& p) noexcept
{
	const auto acnt(p->GetAnchorCount());

	assert(acnt > 0);
	return CountStrong(p) + size_t(acnt) - 2;
}

size_t
RecordCompressor::CountStrong(const shared_ptr<Environment>& p) noexcept
{
	const long scnt(p.use_count());

	assert(scnt > 0);
	return size_t(scnt);
}


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
			[this](const ContextHandler& h) noexcept{
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

TCOAction&
EnsureTCOAction(Context& ctx, TermNode& term)
{
	auto p_act(AccessTCOAction(ctx));

	if(!p_act)
	{
		SetupTailTCOAction(ctx, term, {});
		p_act = AccessTCOAction(ctx);
	}
	return *p_act;
}

void
SetupTailTCOAction(Context& ctx, TermNode& term, bool lift)
{
	ctx.SetupFront(TCOAction(ctx, term, lift));
}


TCOAction&
PrepareTCOEvaluation(Context& ctx, TermNode& term, EnvironmentGuard&& gd)
{
	auto& act(RefTCOAction(ctx));

	assert(&act.GetTermRef() == &term);
	yunused(term);
	act.CompressForGuard(ctx, std::move(gd));
	return act;
}


ReductionStatus
MoveGuard(EnvironmentGuard& gd, Context& ctx) noexcept
{
	const auto egd(std::move(gd));

	return ctx.LastStatus;
}

} // namespace Unilang;

