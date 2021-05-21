// © 2021 Uniontech Software Technology Co.,Ltd.

#include "Unilang.h"
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

namespace Unilang
{

void
llvm_main();

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
	llvm::InitializeNativeTarget();
	llvm::InitializeNativeTargetAsmPrinter();
	llvm::InitializeNativeTargetAsmParser();
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

} // unnamed namespace;

void
llvm_main()
{
	Driver d;

	d.Run();
}

} // namespace Unilang;

