// © 2020 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Unilang_h_
#define INC_Unilang_Unilang_h_ 1

#include <ydef.h> // for byte, ystdex::size_t, std::pair;
#include <ystdex/ref.hpp> // for ystdex::lref;
#include <ystdex/function.hpp> // for ystdex::function;
#include <ystdex/memory.hpp> // for ystdex::make_shared, std::shared_ptr,
//	std::weak_ptr, ystdex::share_move, std::allocator_arg_t,
//	std::allocate_shared, ystdex::make_obj_using_allocator;
#include <ystdex/string_view.hpp> // for ystdex::string_view;
#include <ystdex/memory_resource.h> // for ystdex::pmr and
//	complete ystdex::pmr::polymorphic_allocator;
#include <ystdex/string.hpp> // for ystdex::basic_string, ystdex::sfmt;
#include <forward_list> // for std::forward_list;
#include <list> // for std::list;
#include <ystdex/map.hpp> // for ystdex::map;
#include <ystdex/functor.hpp> // for ystdex::less;
#include <cctype> // for std::isgraph;
#include <vector> // for std::vector;
#include <deque> // for std::deque;
#include <stack> // for std::stack;
#include <sstream> // for std::basic_ostringstream, std::ostream,
//	std::streamsize;
#include <YSLib/Core/YModules.h>
#include YFM_YSLib_Core_YObject // for YSLib::ValueObject, YSLib::any_ops,
//	YSLib::any, YSLib::bad_any_cast, YSLib::in_place_type;

namespace Unilang
{

using ystdex::byte;
using ystdex::size_t;

using ystdex::lref;

using ystdex::function;
using ystdex::make_shared;
using std::pair;
using std::shared_ptr;
using ystdex::string_view;
using std::weak_ptr;

namespace pmr = ystdex::pmr;

template<typename _tChar, typename _tTraits = std::char_traits<_tChar>,
	class _tAlloc = pmr::polymorphic_allocator<_tChar>>
using basic_string = ystdex::basic_string<_tChar, _tTraits, _tAlloc>;

using string = basic_string<char>;

template<typename _type, class _tAlloc = pmr::polymorphic_allocator<_type>>
using forward_list = std::forward_list<_type, _tAlloc>;

template<typename _type, class _tAlloc = pmr::polymorphic_allocator<_type>>
using list = ystdex::list<_type, _tAlloc>;

template<typename _tKey, typename _tMapped, typename _fComp
	= ystdex::less<_tKey>, class _tAlloc
	= pmr::polymorphic_allocator<std::pair<const _tKey, _tMapped>>>
using map = ystdex::map<_tKey, _tMapped, _fComp, _tAlloc>;

template<typename _tKey, typename _fComp = ystdex::less<_tKey>,
	class _tAlloc = pmr::polymorphic_allocator<_tKey>>
using set = std::set<_tKey, _fComp, _tAlloc>;

template<typename _type, class _tAlloc = pmr::polymorphic_allocator<_type>>
using vector = std::vector<_type, _tAlloc>;

template<typename _type, class _tAlloc = pmr::polymorphic_allocator<_type>>
using deque = std::deque<_type, _tAlloc>;

template<typename _type, class _tSeqCon = deque<_type>>
using stack = std::stack<_type, _tSeqCon>;

using ostringstream = std::basic_ostringstream<char, std::char_traits<char>,
	string::allocator_type>;

// NOTE: Only use the unqualified call for unqualified 'string' type.
using ystdex::sfmt;


using YSLib::ValueObject;
namespace any_ops = YSLib::any_ops;
using YSLib::any;
using YSLib::bad_any_cast;
using YSLib::in_place_type;

} // namespace Unilang;

#endif

