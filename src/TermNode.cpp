// © 2021 Uniontech Software Technology Co.,Ltd.

#include "TermNode.h"
#include "Unilang.h" // for Unilang::Deref;

namespace Unilang
{

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

