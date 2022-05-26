// © 2020-2022 Uniontech Software Technology Co.,Ltd.

#include "Interpreter.h" // for TokenValue, ystdex::sfmt, HasValue,
//	string_view, Context::DefaultHandleException, std::bind, std::getline;
#include "Math.h" // for FPToString;
#include <ystdex/functional.hpp> // for ystdex::bind1, std::placeholders::_1;
#include "Forms.h" // for Forms::Sequence, ReduceBranchToList;
#include <cassert> // for assert;
#include "Evaluation.h" // for ParseLeaf, ParseLeafWithSourceInformation;
#include "Exception.h" // for UnilangException;
#include <ostream> // for std::ostream;
#include <YSLib/Service/YModules.h>
#include YFM_YSLib_Adaptor_YAdaptor // for YSLib, YSLib::Notice;
#include YFM_YSLib_Core_YException // for YSLib::ExtractException,
//	YSLib::stringstream;
#include YFM_YSLib_Service_TextFile // for Text::OpenSkippedBOMtream,
//	Text::BOM_UTF_8, YSLib::share_move;
#include <exception> // for std::throw_with_nested;
#include <ystdex/functor.hpp> // for ystdex::id;
#include <algorithm> // for std::find_if, std::for_each;
#include <ystdex/scope_guard.hpp> // for ystdex::make_guard;
#include <iostream> // for std::cout, std::cerr, std::endl, std::cin;
#include "Syntax.h" // for ReduceSyntax;

namespace Unilang
{

namespace
{

template<typename _func>
void
PrintTermNode(std::ostream& os, const TermNode& term, _func f, size_t depth = 0,
	size_t idx = 0)
{
	if(depth != 0 && idx != 0)
		os << ' ';
	ResolveTerm([&](const TermNode& tm){
		try
		{
			f(os, tm);
			return;
		}
		catch(bad_any_cast&)
		{}
		os << '(';
		try
		{
			size_t i(0);

			for(const auto& nd : tm)
			{
				PrintTermNode(os, nd, f, depth + 1, i);
				++i;
			}
		}
		catch(std::out_of_range&)
		{}
		os << ')';
	}, term);
}

YB_ATTR_nodiscard YB_PURE string
StringifyValueObject(const ValueObject& vo)
{
	if(const auto p = vo.AccessPtr<string>())
		return ystdex::quote(*p);
	if(const auto p = vo.AccessPtr<TokenValue>())
		return *p;
	if(const auto p = vo.AccessPtr<bool>())
		return *p ? "#t" : "#f";
	if(const auto p = vo.AccessPtr<int>())
		return sfmt<string>("%d", *p);
	if(const auto p = vo.AccessPtr<unsigned>())
		return sfmt<string>("%u", *p);
	if(const auto p = vo.AccessPtr<long long>())
		return sfmt<string>("%lld", *p);
	if(const auto p = vo.AccessPtr<unsigned long long>())
		return sfmt<string>("%llu", *p);
	if(const auto p = vo.AccessPtr<double>())
		return FPToString(*p);
	if(const auto p = vo.AccessPtr<long>())
		return sfmt<string>("%ld", *p);
	if(const auto p = vo.AccessPtr<unsigned long>())
		return sfmt<string>("%lu", *p);
	if(const auto p = vo.AccessPtr<short>())
		return sfmt<string>("%hd", *p);
	if(const auto p = vo.AccessPtr<unsigned short>())
		return sfmt<string>("%hu", *p);
	if(const auto p = vo.AccessPtr<signed char>())
		return sfmt<string>("%d", int(*p));
	if(const auto p = vo.AccessPtr<unsigned char>())
		return sfmt<string>("%u", unsigned(*p));
	if(const auto p = vo.AccessPtr<float>())
		return FPToString(*p);
	if(const auto p = vo.AccessPtr<long double>())
		return FPToString(*p);
	if(const auto p = vo.AccessPtr<int>())
		return ystdex::sfmt<string>("%d", *p);
	if(const auto p = vo.AccessPtr<ValueToken>())
		if(*p == ValueToken::Unspecified)
			return "#inert";

	const auto& t(vo.type());

	if(t != typeid(void))
		return "#[" + string(t.name()) + ']';
	throw bad_any_cast();
}

YB_ATTR_nodiscard YB_PURE string
StringifyValueObjectForDisplay(const ValueObject& vo)
{
	if(const auto p = vo.AccessPtr<string>())
		return *p;
	return StringifyValueObject(vo);
}

YB_ATTR_nodiscard YB_PURE string
StringifyValueObjectForWrite(const ValueObject& vo)
{
	// XXX: At current, this is just same to the "print" routine.
	return StringifyValueObject(vo);
}

YB_ATTR_nodiscard YB_PURE std::string
MismatchedTypesToString(const bad_any_cast& e)
{
	return ystdex::sfmt("Mismatched types ('%s', '%s') found.", e.from(),
		e.to());
}

void
TraceException(std::exception& e, YSLib::Logger& trace)
{
	using namespace YSLib;

	ExtractException([&](const char* str, size_t level){
		const auto print([&](RecordLevel lv, const char* name, const char* msg){
			trace.TraceFormat(lv, "%*s%s<%u>: %s", int(level), "", name,
				unsigned(lv), msg);
		});

		try
		{
			throw;
		}
		catch(BadIdentifier& ex)
		{
			print(Err, "BadIdentifier", str);
			const auto& si(ex.Source);

			if(ex.Source.first)
				trace.TraceFormat(Err, "%*sIdentifier '%s' is at line %zu,"
					" column %zu in %s.", int(level + 1), "",
					ex.GetIdentifier().c_str(), si.second.Line + 1,
					si.second.Column + 1, si.first->c_str());
		}
		catch(bad_any_cast& ex)
		{
			print(Warning, "TypeError", MismatchedTypesToString(ex).c_str());
		}
		catch(LoggedEvent& ex)
		{
			print(Err, typeid(ex).name(), str);
		}
		catch(...)
		{
			print(Err, "Error", str);
		}
	}, e);
}

YSLib::unique_ptr<std::istream>
OpenFile(const char* filename)
{
	try
	{
		return YSLib::Text::OpenSkippedBOMtream<
			YSLib::IO::SharedInputMappedFileStream>(YSLib::Text::BOM_UTF_8,
			filename);
	}
	catch(...)
	{
		std::throw_with_nested(std::invalid_argument(
			ystdex::sfmt("Failed opening file '%s'.", filename)));
	}
}

} // unnamed namespace;

struct SeparatorPass::TransformationSpec final
{
	enum SeparatorKind
	{
		NAry,
		BinaryAssocLeft,
		BinaryAssocRight
	};

	function<bool(const TermNode&)> Filter;
	function<ValueObject(const ValueObject&)> MakePrefix;
	SeparatorKind Kind;

	TransformationSpec(decltype(Filter), decltype(MakePrefix),
		SeparatorKind = NAry);
	TransformationSpec(TokenValue, ValueObject, SeparatorKind = NAry);
};

SeparatorPass::TransformationSpec::TransformationSpec(
	decltype(Filter) filter, decltype(MakePrefix) make_pfx, SeparatorKind kind)
	: Filter(std::move(filter)), MakePrefix(std::move(make_pfx)), Kind(kind)
{}
SeparatorPass::TransformationSpec::TransformationSpec(TokenValue delim,
	ValueObject pfx, SeparatorKind kind)
	: TransformationSpec(ystdex::bind1(HasValue<TokenValue>, delim),
		[=](const ValueObject&){
		return pfx;
	}, kind)
{}

SeparatorPass::SeparatorPass(TermNode::allocator_type a)
	: allocator(a), transformations({{";", ContextHandler(Forms::Sequence)},
	{",", ContextHandler(FormContextHandler(ReduceBranchToList, 1))},
	{[](const TermNode& nd) noexcept{
		return HasValue<TokenValue>(nd, ":=");
	}, [](const ValueObject&){
		return TokenValue("assign!");
	}, TransformationSpec::BinaryAssocRight},
	{[](const TermNode& nd) noexcept{
		return HasValue<TokenValue>(nd, "=") || HasValue<TokenValue>(nd, "!=");
	}, ystdex::id<>(), TransformationSpec::BinaryAssocLeft},
	{[](const TermNode& nd) noexcept{
		return HasValue<TokenValue>(nd, "<") || HasValue<TokenValue>(nd, ">")
			|| HasValue<TokenValue>(nd, "<=") || HasValue<TokenValue>(nd, ">=");
	}, ystdex::id<>(), TransformationSpec::BinaryAssocLeft},
	{[](const TermNode& nd) noexcept{
		return HasValue<TokenValue>(nd, "+") || HasValue<TokenValue>(nd, "-"); 
	}, ystdex::id<>(), TransformationSpec::BinaryAssocLeft},
	{[](const TermNode& nd) noexcept{
		return HasValue<TokenValue>(nd, "*") || HasValue<TokenValue>(nd, "/"); 
	}, ystdex::id<>(), TransformationSpec::BinaryAssocLeft}}, a)
{}
SeparatorPass::~SeparatorPass() = default;

ReductionStatus
SeparatorPass::operator()(TermNode& term) const
{
	assert(remained.empty() && "Invalid state found.");
	Transform(term, {}, remained);
	while(!remained.empty())
	{
		const auto entry(std::move(remained.top()));

		remained.pop();
		for(auto& tm : entry.first.get())
			Transform(tm, entry.second, remained);
	}
	return ReductionStatus::Clean;
}

void
SeparatorPass::Transform(TermNode& term, bool skip_binary,
	SeparatorPass::TermStack& terms) const
{
	if(IsBranch(term))
	{
		if(IsEmpty(*term.begin()))
			skip_binary = true;
		terms.push({term, skip_binary});
		for(const auto& trans : transformations)
		{
			const auto& filter(trans.Filter);
			auto i(std::find_if(term.begin(), term.end(), filter));

			if(i != term.end())
				switch(trans.Kind)
				{
				case TransformationSpec::NAry:
					term = SeparatorTransformer::Process(std::move(term),
						trans.MakePrefix(i->Value), filter);
					break;
				case TransformationSpec::BinaryAssocLeft:
				case TransformationSpec::BinaryAssocRight:
					if(skip_binary)
						break;
					if(trans.Kind == TransformationSpec::BinaryAssocLeft)
					{
						const auto ri(std::find_if(term.rbegin(), term.rend(),
							filter));

						assert(ri != term.rend());
						i = ri.base();
						--i;
					}
					if(i != term.begin() && std::next(i) != term.end())
					{
						const auto a(term.get_allocator());
						auto res(Unilang::AsTermNode(a, yforward(term).Value));
						auto im(std::make_move_iterator(i));
						using it_t = decltype(im);
						const auto range_add([&](it_t b, it_t e){
							const auto add([&](TermNode& node, it_t j){
								node.Add(Unilang::Deref(j));
							});

							assert(b != e);
							if(std::next(b) == e)
								add(res, b);
							else
							{
								auto child(Unilang::AsTermNode(a));

								do
								{
									add(child, b++);
								}while(b != e);
								res.Add(std::move(child));
							}
						});

						res.Add(
							Unilang::AsTermNode(a, trans.MakePrefix(i->Value)));
						range_add(std::make_move_iterator(term.begin()), im);
						range_add(++im, std::make_move_iterator(term.end()));
						term = std::move(res);
					}
				}
		}
	}
}


Interpreter::Interpreter()
{}

void
Interpreter::Evaluate(TermNode& term)
{
	Root.RewriteTermGuarded(term);
}

ReductionStatus
Interpreter::ExecuteOnce(Context& ctx)
{
	return ReduceOnce(Term, ctx);
}

ReductionStatus
Interpreter::ExecuteString(string_view unit, Context& ctx)
{
	PrepareExecution(ctx);
	if(Echo)
		RelaySwitched(ctx, [&]{
			Print(Term);
			return ReductionStatus::Neutral;
		});
	Term = Read(unit);
	return ExecuteOnce(ctx);
}

ReductionStatus
Interpreter::Exit()
{
	Root.UnwindCurrent();
	return ReductionStatus::Neutral;
}

void
Interpreter::HandleREPLException(std::exception_ptr p, YSLib::Logger& trace)
{
	try
	{
		Context::DefaultHandleException(std::move(p));
	}
	catch(std::exception& e)
	{
		const auto gd(ystdex::make_guard([&]() noexcept{
			Backtrace.clear();
		}));

		TraceException(e, trace);
		trace.TraceFormat(YSLib::Notice, "Location: %s.", CurrentSource
			? CurrentSource->c_str() : "<unknown>");
	}
}

YSLib::unique_ptr<std::istream>
Interpreter::OpenUnique(string filename)
{
	using namespace YSLib;
	auto p_is(OpenFile(filename.c_str()));

	CurrentSource = YSLib::share_move(filename);
	return p_is;
}

void
Interpreter::PrepareExecution(Context& ctx)
{
	ctx.SaveExceptionHandler();
	ctx.HandleException = ystdex::bind1([&](std::exception_ptr p,
		const Context::ReducerSequence::const_iterator& i){
		ctx.TailAction = nullptr;
		ctx.Shift(Backtrace, i);

		static YSLib::Logger trace;

		HandleREPLException(std::move(p), trace);
	}, ctx.GetCurrent().cbegin());
}

void
Interpreter::Print(const TermNode& term)
{
	PrintTermNode(std::cout, term);
	std::cout << std::endl;
}

TermNode
Interpreter::Perform(string_view unit)
{
	auto term(Read(unit));

	Evaluate(term);
	return term;
}

TermNode
Interpreter::Read(string_view unit)
{
	LexicalAnalyzer lexer;

	if(UseSourceLocation)
	{
		SourcedByteParser parse(lexer, Allocator);

		std::for_each(unit.begin(), unit.end(), ystdex::ref(parse));
		return ReadParserResult(parse);
	}
	else
	{
		ByteParser parse(lexer, Allocator);

		std::for_each(unit.begin(), unit.end(), ystdex::ref(parse));
		return ReadParserResult(parse);
	}
}

TermNode
Interpreter::ReadFrom(std::streambuf& buf) const
{
	using s_it_t = std::istreambuf_iterator<char>;
	LexicalAnalyzer lexer;
	ByteParser parse(lexer, Allocator);

	std::for_each(s_it_t(&buf), s_it_t(), ystdex::ref(parse));
	return ReadParserResult(parse);
}
TermNode
Interpreter::ReadFrom(std::istream& is) const
{
	if(is)
	{
		if(const auto p = is.rdbuf())
			return ReadFrom(*p);
		throw std::invalid_argument("Invalid stream buffer found.");
	}
	else
		throw std::invalid_argument("Invalid stream found.");
}

TermNode
Interpreter::ReadParserResult(const ByteParser& parse) const
{
	TermNode res{Allocator};
	const auto& parse_result(parse.GetResult());

	if(ReduceSyntax(res, parse_result.cbegin(), parse_result.cend(),
		[&](const GParsedValue<ByteParser>& str){
		auto term(Unilang::AsTermNode(Allocator));
		const auto id(YSLib::make_string_view(str));

		if(!id.empty())
			ParseLeaf(term, id);
		return term;
	}) != parse_result.cend())
		throw UnilangException("Redundant ')', ']' or '}' found.");
	Preprocess(res);
	return res;
}
TermNode
Interpreter::ReadParserResult(const SourcedByteParser& parse) const
{
	TermNode res{Allocator};
	const auto& parse_result(parse.GetResult());

	if(ReduceSyntax(res, parse_result.cbegin(), parse_result.cend(),
		[&](const GParsedValue<SourcedByteParser>& val){
		auto term(Unilang::AsTermNode(Allocator));
		const auto id(YSLib::make_string_view(val.second));

		if(!id.empty())
			ParseLeafWithSourceInformation(term, id, CurrentSource, val.first);
		return term;
	}) != parse_result.cend())
		throw UnilangException("Redundant ')', ']' or '}' found.");
	Preprocess(res);
	return res;
}

void
Interpreter::Run()
{
	Root.Rewrite(Unilang::ToReducer(Allocator, std::bind(
		&Interpreter::RunLoop, std::ref(*this), std::placeholders::_1)));
}

void
Interpreter::RunLine(string_view unit)
{
	if(!unit.empty() && unit != "exit")
	{
		ShareCurrentSource("*STDIN*");
		Root.Rewrite(Unilang::ToReducer(Allocator, [&](Context& ctx){
			return ExecuteString(unit, ctx);
		}));
	}
}

void
Interpreter::RunScript(string filename)
{
	if(filename == "-")
	{
		ShareCurrentSource("*STDIN*");
		Root.Rewrite(Unilang::ToReducer(Allocator, [&](Context& ctx){
			PrepareExecution(ctx);
			Term = ReadFrom(std::cin);
			return ExecuteOnce(ctx);
		}));
	}
	else if(!filename.empty())
	{
		ShareCurrentSource(filename);
		Root.Rewrite(Unilang::ToReducer(Allocator, [&](Context& ctx){
			PrepareExecution(ctx);
			Term = ReadFrom(*OpenUnique(std::move(filename)));
			return ExecuteOnce(ctx);
		}));
	}
}

ReductionStatus
Interpreter::RunLoop(Context& ctx)
{
	if(WaitForLine())
	{
		ShareCurrentSource("*STDIN*");
		RelaySwitched(ctx, std::bind(&Interpreter::RunLoop, std::ref(*this),
			std::placeholders::_1));
		return !line.empty() ? (line != "exit" ? ExecuteString(line, ctx)
			: Exit()) : ReductionStatus::Partial;
	}
	return ReductionStatus::Retained;
}

bool
Interpreter::SaveGround()
{
	if(!p_ground)
	{
		auto& cs(Root);

		p_ground = Unilang::SwitchToFreshEnvironment(cs,
			ValueObject(cs.WeakenRecord()));
		return true;
	}
	return {};
}

std::istream&
Interpreter::WaitForLine()
{
	using namespace std;

	cout << "> ";
	return getline(cin, line);
}


void
DisplayTermValue(std::ostream& os, const TermNode& term)
{
	YSLib::stringstream oss(string(term.get_allocator()));

	PrintTermNode(oss, term, [](std::ostream& os0, const TermNode& nd){
		os0 << StringifyValueObjectForDisplay(nd.Value);
	});
	os << oss.str().c_str();
}

void
PrintTermNode(std::ostream& os, const TermNode& term)
{
	PrintTermNode(os, term, [](std::ostream& os0, const TermNode& nd){
		os0 << StringifyValueObject(nd.Value);
	});
}

void
WriteTermValue(std::ostream& os, const TermNode& term)
{
	YSLib::stringstream oss(string(term.get_allocator()));

	PrintTermNode(oss, term, [](std::ostream& os0, const TermNode& nd){
		os0 << StringifyValueObjectForWrite(nd.Value);
	});
	os << oss.str().c_str();
}

} // namespace Unilang;

