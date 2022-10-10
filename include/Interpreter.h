// SPDX-FileCopyrightText: 2020-2021 UnionTech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Interpreter_h_
#define INC_Unilang_Interpreter_h_ 1

#include "Context.h" // for pair, lref, stack, vector, GlobalState, string,
//	shared_ptr, Environment, Context, TermNode,
//	YSLib::Logger, YSLib::unique_ptr, std::istream;
#include <cstdlib> // for std::getenv;
#include <ostream> // for std::ostream;

namespace Unilang
{

class Interpreter final
{
public:
	bool Echo = std::getenv("ECHO");
	bool UseSourceLocation = !std::getenv("UNILANG_NO_SRCINFO");

private:
	string line{};
	shared_ptr<Environment> p_ground{};

public:
	GlobalState Global{};
	Context Main{Global};
	TermNode Term{Global.Allocator};
	Context::ReducerSequence Backtrace{Global.Allocator};

	Interpreter();
	Interpreter(const Interpreter&) = delete;

	void
	Evaluate(TermNode&);

private:
	ReductionStatus
	ExecuteOnce(Context&);

	ReductionStatus
	ExecuteString(string_view, Context&);

public:
	ReductionStatus
	Exit();

	void
	HandleREPLException(std::exception_ptr, YSLib::Logger&);

	static YSLib::unique_ptr<std::istream>
	OpenUnique(Context&, string);

	TermNode
	Perform(string_view);

private:
	void
	PrepareExecution(Context&);

public:
	static void
	Print(const TermNode&);

	YB_ATTR_nodiscard TermNode
	Read(string_view);

	void
	Run();

	void
	RunLine(string_view);

private:
	ReductionStatus
	RunLoop(Context&);

public:
	void
	RunScript(string);

	bool
	SaveGround();

	std::istream&
	WaitForLine();
};


void
DisplayTermValue(std::ostream&, const TermNode&);

void
PrintTermNode(std::ostream&, const TermNode&);

void
WriteTermValue(std::ostream&, const TermNode&);

} // namespace Unilang;

#endif

