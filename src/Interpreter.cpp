// © 2020 Uniontech Software Technology Co.,Ltd.

#include "Interpreter.h" // for string_view, ByteParser, std::getline;
#include <ystdex/cctype.h> // for ystdex::isdigit;
#include "Exception.h" // for InvalidSyntax, UnilangException;
#include "Lexical.h" // for CategorizeBasicLexeme, LexemeCategory,
//	DeliteralizeUnchecked;
#include <ostream> // for std::ostream;
#include <YSLib/Service/YModules.h>
#include YFM_YSLib_Service_TextFile // for Text::OpenSkippedBOMtream,
//	Text::BOM_UTF_8, YSLib::share_move;
#include <ios> // for std::ios_base::eofbit;
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
			term.Value = ans;
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
ParseLeaf(string_view id)
{
	assert(id.data());

	auto term(Unilang::AsTermNode());

	if(!id.empty())
		switch(CategorizeBasicLexeme(id))
		{
		case LexemeCategory::Code:
			id = DeliteralizeUnchecked(id);
			[[fallthrough]];
		case LexemeCategory::Symbol:
			if(CheckReducible(DefaultEvaluateLeaf(term, id)))
				term.Value = TokenValue(id);
			break;
		case LexemeCategory::Data:
			term.Value = string(Deliteralize(id));
			[[fallthrough]];
		default:
			break;
		}
	return term;
}

void
PrintTermNodeImpl(std::ostream& os, const TermNode& term, size_t depth = 0,
	size_t n = 0)
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

	if(depth != 0 && n != 0)
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

} // unnamed namespace;

Interpreter::Interpreter()
{}

void
Interpreter::Evaluate(TermNode& term)
{
	Root.Evaluate(term);
}

YSLib::unique_ptr<std::istream>
Interpreter::OpenUnique(string filename)
{
	using namespace YSLib;
	auto p_is(Text::OpenSkippedBOMtream<IO::SharedInputMappedFileStream>(
		Text::BOM_UTF_8, filename.c_str()));

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

bool
Interpreter::Process()
{
	if(line == "exit")
		return {};
	else if(!line.empty())
		try
		{
			struct Guard final
			{
				Context& C;

				~Guard()
				{
					C.UnwindCurrent();
				}
			} gd{Root};
			auto term(Read(line));

			Evaluate(term);
			if(Echo)
				Print(term);
		}
		catch(UnilangException& e)
		{
			using namespace std;

			cerr << "UnilangException[" << typeid(e).name() << "]: "
				<< e.what() << endl;
		}
	return true;
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

	if(ReduceSyntax(term, parse_result.cbegin(), parse_result.cend(), ParseLeaf)
		!= parse_result.cend())
		throw UnilangException("Redundant ')' found.");
	Preprocess(term);
	return term;
}

void
Interpreter::Run()
{
	while(WaitForLine() && Process())
		;
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

