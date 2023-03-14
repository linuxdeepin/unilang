// SPDX-FileCopyrightText: 2021-2023 UnionTech Software Technology Co.,Ltd.

#include "TermNode.h" // for stack, pair, lref;
#include "Unilang.h" // for Unilang::Deref;
#include <ystdex/expanded_function.hpp> // for ystdex::retry_on_cond;

namespace Unilang
{

TermNode::Container
TermNode::ConSub(const Container& con, allocator_type a)
{
	Container res(a);

	if(!con.empty())
	{
		stack<pair<lref<TermNode>, lref<const TermNode>>> remained(a), tms(a);
	
		for(const auto& sub : con)
		{
			res.emplace_back();
			remained.emplace(res.back(), sub),
			tms.emplace(res.back(), sub),
			res.back().Tags = sub.Tags;
			ystdex::retry_on_cond([&]() noexcept{
				return !remained.empty();
			}, [&]{
				auto& dst_con(remained.top().first.get().GetContainerRef());
				auto& src(remained.top().second.get());

				remained.pop();
				for(auto i(src.rbegin()); i != src.rend(); ++i)
				{
					dst_con.emplace_front();

					auto& nterm(dst_con.front());
					auto& tm(*i);

					remained.emplace(nterm, tm),
					tms.emplace(nterm, tm),
					nterm.Tags = tm.Tags;
				}
			});
			ystdex::retry_on_cond([&]() noexcept{
				return !tms.empty();
			}, [&]{
				tms.top().first.get().Value = tms.top().second.get().Value;
				tms.pop();
			});
		}
	}
	return res;
}

void
TermNode::ClearContainer() noexcept
{
	while(!container.empty())
	{
		const auto i(container.begin());

		container.splice(i, std::move(Unilang::Deref(i).container));
		container.erase(i);
	}
}

} // namespace Unilang;

