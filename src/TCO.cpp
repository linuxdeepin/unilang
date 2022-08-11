// SPDX-FileCopyrightText: 2021-2022 UnionTech Software Technology Co.,Ltd.

#include "TCO.h" // for Unilang::Nonnull, std::get, Unilang::Deref, IsTyped;
#include <cassert> // for assert;
#include <ystdex/scope_guard.hpp> // for ystdex::dismiss;
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
	const auto p_root(Nonnull(RootPtr.lock()));

	assert(bool(p_root));
	for(auto& pr : Universe)
	{
		auto& e(pr.first.get());

		Traverse(e, e.Parent, [this](const shared_ptr<Environment>& p_dst){
			auto& count(Universe.at(Unilang::Deref(p_dst)));

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
				auto& dst(Unilang::Deref(p_dst));

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
			auto& dst(Unilang::Deref(p_dst));

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
	: req_lift_result(lift ? 1 : 0), record_list(ctx.get_allocator()),
	env_guard(ctx), term_guard(ystdex::unique_guard(GuardFunction{term})),
	OperatorName([&]() noexcept{
		return std::move(ctx.OperatorName);
	}())
{}
TCOAction::TCOAction(const TCOAction& a)
	: req_lift_result(a.req_lift_result), env_guard(std::move(a.env_guard)),
	term_guard(std::move(a.term_guard))
{}

ReductionStatus
TCOAction::operator()(Context& ctx) const
{
	assert(ystdex::ref_eq<>()(env_guard.func.ContextRef.get(), ctx)
		&& "Invalid context found.");

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
	return res;
}

void
TCOAction::CompressFrameList()
{
	ystdex::retry_on_cond(ystdex::id<>(), [&]() -> bool{
		bool removed = {};

		record_list.remove_if([&](const FrameRecord& r) noexcept -> bool{
			const auto& p_frame_env_ref(std::get<ActiveEnvironmentPtr>(r));

			if(p_frame_env_ref.use_count() != 1
				|| Unilang::Deref(p_frame_env_ref).IsOrphan())
			{
				removed = true;
				return true;
			}
			return {};
		});
		return removed;
	});
}

void
TCOAction::CompressForGuard(Context& ctx, EnvironmentGuard&& gd)
{
	if(env_guard.func.SavedPtr)
	{
		if(auto& p_saved = gd.func.SavedPtr)
		{
			CompressForContext(ctx);
			if(!record_list.empty() && !record_list.front().second)
				record_list.front().second = std::move(p_saved);
			else
				record_list.emplace_front(ContextHandler(),
					std::move(p_saved));
		}
	}
	else
		env_guard = std::move(gd);
}

YB_ATTR_nodiscard ContextHandler
TCOAction::MoveFunction() const
{
	assert(!record_list.empty() && !record_list.front().second
		&& "Invalid state found.");
	auto r(std::move(record_list.front().first));

	record_list.pop_front();
	return r;
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
	return Unilang::Deref(p_act);
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

} // namespace Unilang;

