// SPDX-FileCopyrightText: 2020-2021 UnionTech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Interpreter_h_
#define INC_Unilang_Interpreter_h_ 1

#include "Context.h" // for pair, lref, stack, vector, GlobalState, string,
//	shared_ptr, Environment, Context, TermNode, function,
//	YSLib::allocate_shared, YSLib::Logger, YSLib::unique_ptr;
#include <algorithm> // for std::find_if;
#include <cstdlib> // for std::getenv;
#include <istream> // for std::istream;
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
	shared_ptr<string> CurrentSource{};

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

	template<typename... _tParams>
	void
	ShareCurrentSource(_tParams&&... args)
	{
		CurrentSource = YSLib::allocate_shared<string>(Global.Allocator,
			yforward(args)...);
	}

	void
	HandleREPLException(std::exception_ptr, YSLib::Logger&);

	YSLib::unique_ptr<std::istream>
	OpenUnique(string);

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

	YB_ATTR_nodiscard TermNode
	ReadFrom(std::streambuf&) const;
	YB_ATTR_nodiscard TermNode
	ReadFrom(std::istream&) const;

	YB_ATTR_nodiscard TermNode
	ReadParserResult(const ByteParser&) const;
	YB_ATTR_nodiscard TermNode
	ReadParserResult(const SourcedByteParser&) const;

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

