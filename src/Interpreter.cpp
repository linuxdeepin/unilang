// © 2020-2021 Uniontech Software Technology Co.,Ltd.

#include "Interpreter.h" // for string_view, ystdex::sfmt, ByteParser,
//	std::getline;
#include <ystdex/cctype.h> // for ystdex::isdigit;
#include "Exception.h" // for InvalidSyntax, UnilangException;
#include "Arithmetic.h" // for Number;
#include "Lexical.h" // for CategorizeBasicLexeme, LexemeCategory,
//	DeliteralizeUnchecked;
#include <ostream> // for std::ostream;
#include <YSLib/Service/YModules.h>
#include YFM_YSLib_Adaptor_YAdaptor // for YSLib::Logger, YSLib;
#include YFM_YSLib_Core_YException // for YSLib::ExtractException;
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
	assert(id.data());
	if(!id.empty())
	{
		const char f(id.front());

		if(ystdex::isdigit(f))
		{
		    int ans(0);

			for(auto p(id.begin()); p != id.end(); ++p)
				if(ystdex::isdigit(*p))
				{
					if(unsigned((ans << 3) + (ans << 1) + *p - '0')
						<= unsigned(INT_MAX))
						ans = (ans << 3) + (ans << 1) + *p - '0';
					else
						throw InvalidSyntax(ystdex::sfmt<std::string>(
							"Value of identifier '%s' is out of the range of"
							" the supported integer.", id.data()));
				}
				else
					throw InvalidSyntax(ystdex::sfmt<std::string>("Literal"
						" postfix is unsupported in identifier '%s'.",
						id.data()));
			term.Value = Number(ans);
		}
		else if(id == "#t")
			term.Value = true;
		else if(id == "#f")
			term.Value = false;
		else if(id == "#inert")
			term.Value = ValueToken::Unspecified;
		else
			return ReductionStatus::Retrying;
		return ReductionStatus::Clean;
	}
	return ReductionStatus::Retrying;
}

TermNode
ParseLeaf(string_view id, TermNode::allocator_type a)
{
	assert(id.data());

	auto term(Unilang::AsTermNode());

	if(!id.empty())
		switch(CategorizeBasicLexeme(id))
		{
		case LexemeCategory::Code:
			id = DeliteralizeUnchecked(id);
			YB_ATTR_fallthrough;
		case LexemeCategory::Symbol:
			if(CheckReducible(DefaultEvaluateLeaf(term, id)))
				term.SetValue(in_place_type<TokenValue>, id, a);
			break;
		case LexemeCategory::Data:
			term.SetValue(in_place_type<string>, Deliteralize(id), a);
			YB_ATTR_fallthrough;
		default:
			break;
		}
	return term;
}

void
PrintTermNodeImpl(std::ostream& os, const TermNode& term, size_t depth = 0,
	size_t i = 0)
{
	const auto print_node_str([&](const TermNode& subterm){
		return ResolveTerm(
			[&](const TermNode& tm) -> pair<lref<const TermNode>, bool>{
			try
			{
				const auto& vo(tm.Value);

				os << [&]() -> string{
					if(const auto p = vo.AccessPtr<string>())
						return ystdex::quote(*p);
					if(const auto p = vo.AccessPtr<TokenValue>())
						return *p;
					if(const auto p = vo.AccessPtr<bool>())
						return *p ? "#t" : "#f";
					if(const auto p = vo.AccessPtr<int>())
						return ystdex::sfmt<string>("%d", *p);
					if(const auto p = vo.AccessPtr<Number>())
						// TODO: Support different internal representations.
						return ystdex::sfmt<string>("%d", int(*p));
					if(const auto p = vo.AccessPtr<ValueToken>())
						if(*p == ValueToken::Unspecified)
							return "#inert";

					const auto& t(vo.type());

					if(t != typeid(void))
						return "#[" + string(t.name()) + ']';
					throw bad_any_cast();
				}();
				return {tm, true};
			}
			catch(bad_any_cast&)
			{}
			return {tm, false};
		}, subterm);
	});

	if(depth != 0 && i != 0)
		os << ' ';

	const auto pr(print_node_str(term));

	if(!pr.second)
	{
		size_t n(0);

		os << '(';
		for(const auto& nd : pr.first.get())
		{
			try
			{
				PrintTermNodeImpl(os, nd, depth + 1, n);
			}
			catch(std::out_of_range&)
			{}
			++n;
		}
		os << ')';
	}
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

Interpreter::Interpreter()
{}

void
Interpreter::Evaluate(TermNode& term)
{
	Root.RewriteTermGuarded(term);
}

ReductionStatus
Interpreter::EvaluateOnceIn(Context& ctx)
{
	return ReduceOnce(Term, ctx);
}

ReductionStatus
Interpreter::ExecuteOnce(string_view unit, Context& ctx)
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
	if(Echo)
		RelaySwitched(ctx, [&]{
			Print(Term);
			return ReductionStatus::Neutral;
		});
	Term = Read(unit);
	return EvaluateOnceIn(ctx);
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
	ByteParser parse{};

	std::for_each(unit.begin(), unit.end(), ystdex::ref(parse));

	return ReadParserResult(parse);
}

TermNode
Interpreter::ReadFrom(std::streambuf& buf, Context&) const
{
	using s_it_t = std::istreambuf_iterator<char>;
	ByteParser parse{};

	std::for_each(s_it_t(&buf), s_it_t(), ystdex::ref(parse));

	return ReadParserResult(parse);
}
TermNode
Interpreter::ReadFrom(std::istream& is, Context& ctx) const
{
	if(is)
	{
		if(const auto p = is.rdbuf())
			return ReadFrom(*p, ctx);
		throw std::invalid_argument("Invalid stream buffer found.");
	}
	else
		throw std::invalid_argument("Invalid stream found.");
}

TermNode
Interpreter::ReadParserResult(const ByteParser& parse) const
{
	TermNode term{};
	const auto& parse_result(parse.GetResult());

	if(ReduceSyntax(term, parse_result.cbegin(), parse_result.cend(),
		std::bind(ParseLeaf, std::placeholders::_1, std::ref(Allocator)))
		!= parse_result.cend())
		throw UnilangException("Redundant ')' found.");
	Preprocess(term);
	return term;
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
			return ExecuteOnce(unit, ctx);
		}));
}

ReductionStatus
Interpreter::RunLoop(Context& ctx)
{
	if(WaitForLine())
	{
		RelaySwitched(ctx, std::bind(&Interpreter::RunLoop, std::ref(*this),
			std::placeholders::_1));
		return !line.empty() ? (line != "exit" ? ExecuteOnce(line, ctx)
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
PrintTermNode(std::ostream& os, const TermNode& term)
{
	PrintTermNodeImpl(os, term);
}

} // namespace Unilang;

