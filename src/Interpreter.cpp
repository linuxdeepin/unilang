// © 2020-2021 Uniontech Software Technology Co.,Ltd.

#include "Interpreter.h" // for std::bind, HasValue, TokenValue,
//	std::placeholders, std::declval, string_view, ystdex::sfmt,
//	Context::DefaultHandleException, ByteParser, std::getline;
#include "Forms.h" // for Forms::Sequence, ReduceBranchToList;
#include <cassert> // for assert;
#include "Math.h" // for ReadDecimal, FPToString;
#include <ystdex/cctype.h> // for ystdex::isdigit;
#include "Exception.h" // for InvalidSyntax, UnilangException;
#include "Lexical.h" // for CategorizeBasicLexeme, LexemeCategory,
//	DeliteralizeUnchecked;
#include <ostream> // for std::ostream;
#include <YSLib/Service/YModules.h>
#include YFM_YSLib_Adaptor_YAdaptor // for YSLib::Logger, YSLib;
#include YFM_YSLib_Core_YException // for YSLib::ExtractException,
//	YSLib::stringstream;
#include "Context.h" // for Context::DefaultHandleException;
#include YFM_YSLib_Service_TextFile // for Text::OpenSkippedBOMtream,
//	Text::BOM_UTF_8, YSLib::share_move;
#include <exception> // for std::throw_with_nested;
#include <ystdex/scope_guard.hpp> // for ystdex::make_guard;
#include <functional> // for std::placeholders::_1;
#include <iostream> // for std::cout, std::cerr, std::endl, std::cin;
#include <algorithm> // for std::for_each;
#include "Syntax.h" // for ReduceSyntax;

namespace Unilang
{

namespace
{

ReductionStatus
DefaultEvaluateLeaf(TermNode& term, string_view id)
{
	assert(bool(id.data()));
	assert(!id.empty() && "Invalid leaf token found.");
	switch(id.front())
	{
	case '#':
		id.remove_prefix(1);
		if(!id.empty())
			switch(id.front())
			{
			case 't':
				if(id.size() == 1 || id.substr(1) == "rue")
				{
					term.Value = true;
					return ReductionStatus::Clean;
				}
				break;
			case 'f':
				if(id.size() == 1 || id.substr(1) == "alse")
				{
					term.Value = false;
					return ReductionStatus::Clean;
				}
				break;
			case 'i':
				if(id.substr(1) == "nert")
				{
					term.Value = ValueToken::Unspecified;
					return ReductionStatus::Clean;
				}
				else if(id.substr(1) == "gnore")
				{
					term.Value = ValueToken::Ignore;
					return ReductionStatus::Clean;
				}
				break;
			}
	default:
		break;
	case '1':
	case '2':
	case '3':
	case '4':
	case '5':
	case '6':
	case '7':
	case '8':
	case '9':
		ReadDecimal(term.Value, id, id.begin());
		return ReductionStatus::Clean;
	case '-':
		if(YB_UNLIKELY(id.find_first_not_of("+-") == string_view::npos))
			break;
		if(id.size() == 6 && id[4] == '.' && id[5] == '0')
		{
			if(id[1] == 'i' && id[2] == 'n' && id[3] == 'f')
			{
				term.Value = -std::numeric_limits<double>::infinity();
				return ReductionStatus::Clean;
			}
			if(id[1] == 'n' && id[2] == 'a' && id[3] == 'n')
			{
				term.Value = -std::numeric_limits<double>::quiet_NaN();
				return ReductionStatus::Clean;
			}
		}
		if(id.size() > 1)
			ReadDecimal(term.Value, id, std::next(id.begin()));
		else
			term.Value = 0;
		return ReductionStatus::Clean;
	case '+':
		if(YB_UNLIKELY(id.find_first_not_of("+-") == string_view::npos))
			break;
		if(id.size() == 6 && id[4] == '.' && id[5] == '0')
		{
			if(id[1] == 'i' && id[2] == 'n' && id[3] == 'f')
			{
				term.Value = std::numeric_limits<double>::infinity();
				return ReductionStatus::Clean;
			}
			if(id[1] == 'n' && id[2] == 'a' && id[3] == 'n')
			{
				term.Value = std::numeric_limits<double>::quiet_NaN();
				return ReductionStatus::Clean;
			}
		}
		YB_ATTR_fallthrough;
	case '0':
		if(id.size() > 1)
			ReadDecimal(term.Value, id, std::next(id.begin()));
		else
			term.Value = 0;
		return ReductionStatus::Clean;
	}
	return ReductionStatus::Retrying;
}

void
ParseLeaf(TermNode& term, string_view id)
{
	assert(id.data());
	assert(!id.empty() && "Invalid leaf token found.");
	switch(CategorizeBasicLexeme(id))
	{
	case LexemeCategory::Code:
		id = DeliteralizeUnchecked(id);
		term.SetValue(in_place_type<TokenValue>, id, term.get_allocator());
		break;
	case LexemeCategory::Symbol:
		if(CheckReducible(DefaultEvaluateLeaf(term, id)))
		{
			if(!id.empty())
			{
				const char f(id.front());

				if((id.size() > 1 && (f == '#'|| f == '+' || f == '-')
					&& id.find_first_not_of("+-") != string_view::npos)
					|| ystdex::isdigit(f))
					throw InvalidSyntax(ystdex::sfmt(f != '#'
						? "Unsupported literal prefix found in literal '%s'."
						: "Invalid literal '%s' found.", id.data()));
			}
			term.SetValue(in_place_type<TokenValue>, id, term.get_allocator());
		}
		break;
	case LexemeCategory::Data:
		term.SetValue(in_place_type<string>, Deliteralize(id),
			term.get_allocator());
		YB_ATTR_fallthrough;
	default:
		break;
	}
}

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
	using Filter = decltype(std::bind(HasValue<TokenValue>,
		std::placeholders::_1, std::declval<TokenValue&>()));

	TokenValue Delimiter;
	Filter TransformFilter;
	ValueObject Prefix;

	TransformationSpec(TokenValue, ValueObject);
};

SeparatorPass::TransformationSpec::TransformationSpec(TokenValue delim,
	ValueObject pfx)
	: Delimiter(delim), TransformFilter(std::bind(HasValue<TokenValue>,
	std::placeholders::_1, delim)), Prefix(pfx) 
{}

SeparatorPass::SeparatorPass(TermNode::allocator_type a)
	: allocator(a), transformations({{";", ContextHandler(Forms::Sequence)},
	{",", ContextHandler(FormContextHandler(ReduceBranchToList, 1))}}, a)
{}
SeparatorPass::~SeparatorPass() = default;

ReductionStatus
SeparatorPass::operator()(TermNode& term) const
{
	assert(remained.empty() && "Invalid state found.");
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

void
SeparatorPass::Transform(TermNode& term, SeparatorPass::TermStack& terms) const
{
	terms.push(term);
	for(const auto& trans : transformations)
		if(std::find_if(term.begin(), term.end(), trans.TransformFilter)
			!= term.end())
			term = SeparatorTransformer::Process(std::move(term), trans.Prefix,
				trans.TransformFilter);
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
	ctx.HandleException = std::bind([&](std::exception_ptr p,
		const Context::ReducerSequence::const_iterator& i){
		ctx.TailAction = nullptr;
		ctx.Shift(Backtrace, i);
		try
		{
			Context::DefaultHandleException(std::move(p));
		}
		catch(std::exception& e)
		{
			const auto gd(ystdex::make_guard([&]() noexcept{
				Backtrace.clear();
			}));
			static YSLib::Logger trace;

			TraceException(e, trace);
		}
	}, std::placeholders::_1, ctx.GetCurrent().cbegin());
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
	ByteParser parse(lexer, Allocator);

	std::for_each(unit.begin(), unit.end(), ystdex::ref(parse));
	return ReadParserResult(parse);
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
		[&](string_view id){
		auto term(Unilang::AsTermNode(Allocator));

		if(!id.empty())
			ParseLeaf(term, id);
		return term;
	}) != parse_result.cend())
		throw UnilangException("Redundant ')' found.");
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
		Root.Rewrite(Unilang::ToReducer(Allocator, [&](Context& ctx){
			return ExecuteString(unit, ctx);
		}));
}

void
Interpreter::RunScript(string filename)
{
	if(filename == "-")
	{
		Root.Rewrite(Unilang::ToReducer(Allocator, [&](Context& ctx){
			PrepareExecution(ctx);
			Term = ReadFrom(std::cin);
			return ExecuteOnce(ctx);
		}));
	}
	else if(!filename.empty())
	{
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

