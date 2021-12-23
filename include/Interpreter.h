// © 2020-2021 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Interpreter_h_
#define INC_Unilang_Interpreter_h_ 1

#include "Context.h" // for stack, lref, vector, pmr, string, shared_ptr,
//	Environment, Context, TermNode, Unilang::Deref, YSLib::unique_ptr;
#include <algorithm> // for std::find_if;
#include <cstdlib> // for std::getenv;
#include <istream> // for std::istream;
#include "Parser.h" // for ByteParser;
#include <ostream> // for std::ostream;

namespace Unilang
{

class SeparatorPass final
{
private:
	using TermStack = stack<lref<TermNode>, vector<lref<TermNode>>>;
	struct TransformationSpec;

	TermNode::allocator_type allocator;
	vector<TransformationSpec> transformations;
	mutable TermStack remained{allocator};

public:
	SeparatorPass(TermNode::allocator_type);
	~SeparatorPass();

	ReductionStatus
	operator()(TermNode&) const;

	void
	Transform(TermNode&, TermStack&) const;
};


class Interpreter final
{
private:
	pmr::pool_resource resource{pmr::new_delete_resource()};
	string line{};
	shared_ptr<Environment> p_ground{};

public:
	shared_ptr<string> CurrentSource{};
	bool Echo = std::getenv("ECHO");
	TermNode::allocator_type Allocator{&resource};
	Context Root{resource};
	SeparatorPass Preprocess{Allocator};
	TermNode Term{Allocator};
	Context::ReducerSequence Backtrace{Allocator};

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

