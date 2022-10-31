// SPDX-FileCopyrightText: 2021-2022 UnionTech Software Technology Co.,Ltd.

#include "JIT.h"
#if !UNILANG_NO_LLVM
#if __GNUG__
#	pragma GCC diagnostic push
#	pragma GCC diagnostic ignored "-Wctor-dtor-privacy"
#	pragma GCC diagnostic ignored "-Wshadow"
#	pragma GCC diagnostic ignored "-Wsign-conversion"
#	pragma GCC diagnostic ignored "-Wunused-parameter"
#endif
#include "llvm/ADT/STLExtras.h"
#include "llvm/Support/TargetSelect.h" // for llvm::InitializeNativeTarget,
//	 llvm::InitializeNativeTargetAsmPrinter,
//	llvm::InitializeNativeTargetAsmParser;
#if __GNUG__
#	pragma GCC diagnostic pop
#endif
#endif

namespace Unilang
{

namespace
{

class Driver final
{
public:
	Driver();

	void
	InitializeModuleAndPassManager();

	void
	Run();
};

Driver::Driver()
{
#if !UNILANG_NO_LLVM
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();
	llvm::InitializeNativeTargetAsmParser();
#endif
	InitializeModuleAndPassManager();
}

void
Driver::InitializeModuleAndPassManager()
{
}

void
Driver::Run()
{
}


ReductionStatus
JITReduceOnce(TermNode& term, Context& ctx)
{
	return Context::DefaultReduceOnce(term, ctx);
}

} // unnamed namespace;

void
SetupJIT(Context& ctx)
{
	ctx.ReduceOnce = Continuation(JITReduceOnce, ctx);
}

void
JITMain()
{
	Driver d;

	d.Run();
}

} // namespace Unilang;

