// © 2021 Uniontech Software Technology Co.,Ltd.

// XXX: No other headers are included to allow coexistence of different version
//	of the standard libraries.
#if __GNUG__
#	pragma push_macro("_GLIBCXX_DEBUG")
#	pragma push_macro("_GLIBCXX_DEBUG_PEDANTIC")
#	undef _GLIBCXX_DEBUG
#	undef _GLIBCXX_DEBUG_PEDANTIC
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#	pragma GCC diagnostic ignored "-Wshadow"
#	pragma GCC diagnostic ignored "-Wsign-conversion"
#	pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include "llvm/ADT/STLExtras.h"
#if __GNUG__
#	pragma GCC diagnostic pop
#	pragma pop_macro("_GLIBCXX_DEBUG_PEDANTIC")
#	pragma pop_macro("_GLIBCXX_DEBUG")
#endif

namespace Unilang
{

void
llvm_main();

void
llvm_main()
{}

} // namespace Unilang;

