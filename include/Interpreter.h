// © 2020-2021 Uniontech Software Technology Co.,Ltd.

#ifndef INC_Unilang_Interpreter_h_
#define INC_Unilang_Interpreter_h_ 1

#include "Forms.h" // for TokenValue, HasValue, std::bind, std::placeholders,
//	TermNode, lref, stack, std::declval, Forms::Sequence, ReduceBranchToList,
//	string, shared_ptr, pmr, Unilang::Deref, YSLib::unique_ptr;
#include <algorithm> // for std::find_if;
#include <cstdlib> // for std::getenv;
#include <istream> // for std::istream;
#include "Parser.h" // for ByteParser;
#include <ostream> // for std::ostream;

namespace Unilang
{

class SeparatorPass
{
private:
	using Filter = decltype(std::bind(HasValue<TokenValue>,
		std::placeholders::_1, std::declval<TokenValue&>()));
	using TermStack = stack<lref<TermNode>, vector<lref<TermNode>>>;

	TermNode::allocator_type alloc;
	TokenValue delim{";"};
	TokenValue delim2{","};
	Filter
		filter{std::bind(HasValue<TokenValue>, std::placeholders::_1, delim)};
	Filter
		filter2{std::bind(HasValue<TokenValue>, std::placeholders::_1, delim2)};
	ValueObject pfx{ContextHandler(Forms::Sequence)};
	ValueObject pfx2{ContextHandler(FormContextHandler(ReduceBranchToList, 1))};
	mutable TermStack remained{alloc};

public:
	SeparatorPass(TermNode::allocator_type a)
		: alloc(a)
	{}

	ReductionStatus
	operator()(TermNode& term) const
	{
		assert(remained.empty());
		Transform(term, remained);
		while(!remained.empty())
		{
			const auto term_ref(std::move(remained.top()));

			remained.pop();
			for(auto& tm : term_ref.get())
				Transform(tm, remained);
		}
		return ReductionStatus::Clean;
	}

private:
	void
	Transform(TermNode& term, TermStack& terms) const
	{
		terms.push(term);
		if(std::find_if(term.begin(), term.end(), filter) != term.end())
			term = SeparatorTransformer::Process(std::move(term), pfx, filter);
		if(std::find_if(term.begin(), term.end(), filter2) != term.end())
			term = SeparatorTransformer::Process(std::move(term), pfx2,
				filter2);
	}
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

	ReductionStatus
	ExecuteOnce(Context&);

private:
	ReductionStatus
	ExecuteString(string_view, Context&);

public:
	ReductionStatus
	Exit();

	YSLib::unique_ptr<std::istream>
	OpenUnique(string);

	TermNode
	Perform(string_view);

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

