// © 2020-2021 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Unilang_h_
#define INC_Unilang_Unilang_h_ 1

#include <YSLib/Core/YModules.h>
#include YFM_YSLib_Adaptor_YAdaptor // for YSLib::byte, YSLib::size_t,
//	YSLib::lref, YSLib::pair, YSLib::make_shared, YSLib::shared_ptr,
//	YSLib::weak_ptr, YSLib::string_view, YSLib::pmr, YSLib::basic_string,
//	YSLib::string, YSLib::forward-list, YSLib::list, YSLib::map, YSLib::set,
//	YSLib::vector, YSLib::deque, YSLib::stack, YSLib::ostringstream,
//	YSLib::sfmt, YSLib::Deref, YSLib::Nonnull;
#include YFM_YSLib_Core_YFunc // for YSLib::function;
#include YFM_YSLib_Core_YObject // for YSLib::ValueObject, YSLib::any_ops,
//	YSLib::any, YSLib::bad_any_cast, YSLib::in_place_type;
#include <ystdex/memory.hpp> // for ystdex::share_move, std::allocator_arg_t,
//	std::allocate_shared, ystdex::make_obj_using_allocator;
#include <ystdex/memory_resource.h> // for complete
//	ystdex::pmr::polymorphic_allocator;
#include <cctype> // for std::isgraph;
#include <sstream> // for std::basic_ostringstream, std::ostream,
//	std::streamsize;

namespace Unilang
{

using YSLib::byte;
using YSLib::size_t;

using YSLib::lref;

using YSLib::pair;

using YSLib::function;

using YSLib::make_shared;
using YSLib::shared_ptr;
using YSLib::weak_ptr;

using YSLib::string_view;

namespace pmr = YSLib::pmr;

using YSLib::basic_string;
using YSLib::string;
using YSLib::forward_list;
using YSLib::list;
using YSLib::map;
using YSLib::set;
using YSLib::vector;
using YSLib::deque;
using YSLib::stack;
using YSLib::ostringstream;

// NOTE: Only use the unqualified call for unqualified 'string' type.
using YSLib::sfmt;

using YSLib::ValueObject;
namespace any_ops = YSLib::any_ops;
using YSLib::any;
using YSLib::bad_any_cast;
using YSLib::in_place_type;

using YSLib::Deref;
using YSLib::Nonnull;

} // namespace Unilang;

#endif

