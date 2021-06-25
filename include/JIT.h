// © 2021 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_JIT_h_
#define INC_Unilang_JIT_h_ 1

#include "Context.h" // for Context;
#include YFM_YSLib_Adaptor_YAdaptor // for YSLib::make_unique,
//	YSLib::unique_ptr, YSLib::to_pmr_string, YSLib::to_std_string;
#include <cstdint> // for std::int8_t;

namespace Unilang
{

using YSLib::make_unique;
using YSLib::unique_ptr;

using YSLib::to_pmr_string;
using YSLib::to_std_string;

inline namespace JITTypes
{
// NOTE: As of LLVM 12, LLVM has not built-in 'void*' type, and opaque types
//	are not ready. The documentation suggests to use 'i8*' instead, see
//	https://llvm.org/docs/LangRef.html#pointer-type. It should be safe in
//	the sense of TBAA, while the type is unknown by the LLVM optimization
//	passes. 
using HostPtr = std::int8_t*;

using InteropFuncPtr = ReductionStatus(*)(HostPtr);

} // inline namespace JITTypes;

void
SetupJIT(Context&);

void
llvm_main();

} // namespace Unilang;

#endif

