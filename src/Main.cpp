﻿// © 2020-2022 Uniontech Software Technology Co.,Ltd.

#include "Interpreter.h" // for Interpreter, ValueObject, string_view,
//	string;
#include <cstdlib> // for std::getenv;
#include "Context.h" // for Context, EnvironmentSwitcher,
//	Unilang::SwitchToFreshEnvironment;
#include <ystdex/scope_guard.hpp> // for ystdex::guard;
#include <ystdex/invoke.hpp> // for ystdex::invoke;
#include <functional> // for std::bind, std::placeholders;
#include "BasicReduction.h" // for ReductionStatus, LiftOther;
#include "Evaluation.h" // for RetainN, ValueToken, RegisterStrict,
//	NameTypedContextHandler;
#include "Exception.h" // for ThrowNonmodifiableErrorForAssignee;
#include "TermAccess.h" // for ResolveTerm, ResolvedTermReferencePtr,
//	ThrowValueCategoryError, IsTypedRegular, ComposeReferencedTermOp,
//	IsReferenceTerm, IsBoundLValueTerm, IsUncollapsedTerm, IsUniqueTerm,
//	EnvironmentReference, TermNode, IsBranchedList, ThrowInsufficientTermsError;
#include <iterator> // for std::next, std::iterator_traits;
#include "Forms.h" // for Forms::CallRawUnary, Forms::CallBinaryFold and other
//	form implementations;
#include "Lexical.h" // for IsUnilangSymbol;
#include <ystdex/functor.hpp> // for ystdex::plus, ystdex::equal_to,
//	ystdex::less, ystdex::less_equal, ystdex::greater, ystdex::greater_equal,
//	ystdex::minus, ystdex::multiplies;
#include <regex> // for std::regex, std::regex_match, std::regex_replace;
#include <YSLib/Core/YModules.h>
#include YFM_YSLib_Adaptor_YAdaptor // for YSLib::ufexists,
//	YSLib::FetchEnvironmentVariable;
#include "Math.h" // for NumberLeaf, NumberNode and other math functions;
#include <ystdex/functional.hpp> // for ystdex::bind1;
#include <iostream> // for std::cout, std::endl;
#include YFM_YSLib_Core_YShellDefinition // for std::to_string,
//	YSLib::make_string_view, YSLib::to_std::string;
#include <random> // for std::random_device, std::mt19937,
//	std::uniform_int_distribution;
#include <cstdlib> // for std::exit;
#include "UnilangFFI.h"
#include "JIT.h"
#include <ystdex/string.hpp> // for ystdex::quote, ystdex::write_ntcts;
#include <vector> // for std::vector;
#include <array> // for std::array;
#include YFM_YSLib_Core_YException // for YSLib::LoggedEvent,
//	YSLib::FilterExceptions, YSLib::CommandArguments, YSLib::Alert;
#include YFM_YSLib_Core_YCoreUtilities // for YSLib::LockCommandArguments;
#include "UnilangQt.h"

namespace Unilang
{

namespace
{

const bool Unilang_UseJIT(!std::getenv("UNILANG_NO_JIT"));

template<typename _fCallable>
shared_ptr<Environment>
GetModuleFor(Context& ctx, _fCallable&& f)
{
	ystdex::guard<EnvironmentSwitcher> gd(ctx,
		Unilang::SwitchToFreshEnvironment(ctx,
		ValueObject(ctx.WeakenRecord())));

	ystdex::invoke(f);
	ctx.GetRecordRef().Frozen = true;
	return ctx.ShareRecord();
}

template<typename _fCallable>
inline void
LoadModuleChecked(Context& ctx, string_view module_name, _fCallable&& f)
{
	ctx.GetRecordRef().DefineChecked(module_name,
		Unilang::GetModuleFor(ctx, yforward(f)));
}

YB_ATTR_nodiscard ReductionStatus
DoMoveOrTransfer(void(&f)(TermNode&, TermNode&, bool), TermNode& term)
{
	RetainN(term);
	ResolveTerm([&](TermNode& nd, ResolvedTermReferencePtr p_ref){
		f(term, nd, !p_ref || p_ref->IsModifiable());
		EnsureValueTags(term.Tags);
	}, *std::next(term.begin()));
	return ReductionStatus::Retained;
}

YB_ATTR_nodiscard ReductionStatus
Qualify(TermNode& term, TermTags tag_add)
{
	return Forms::CallRawUnary([&](TermNode& tm){
		if(const auto p = Unilang::TryAccessLeaf<TermReference>(tm))
			p->AddTags(tag_add);
		LiftTerm(term, tm);
		return ReductionStatus::Retained;
	}, term);
}

YB_ATTR_nodiscard TermNode
MoveResolvedValue(const Context& ctx, string_view id)
{
	auto tm(MoveResolved(ctx, id));

	EnsureValueTags(tm.Tags);
	return tm;
}

void
CheckForAssignment(TermNode& nd, ResolvedTermReferencePtr p_ref)
{
	if(p_ref)
	{
		if(p_ref->IsModifiable())
			return;
		ThrowNonmodifiableErrorForAssignee();
	}
	ThrowValueCategoryError(nd);
}

template<typename _func>
YB_ATTR_nodiscard ValueToken
DoAssign(_func f, TermNode& x)
{
	ResolveTerm([&](TermNode& nd, ResolvedTermReferencePtr p_ref){
		CheckForAssignment(nd, p_ref);
		f(nd);
	}, x);
	return ValueToken::Unspecified;
}

YB_ATTR_nodiscard ReductionStatus
DoResolve(TermNode(&f)(const Context&, string_view), TermNode& term,
	const Context& c)
{
	Forms::CallRegularUnaryAs<const TokenValue>([&](string_view id){
		term = f(c, id);
	}, term);
	return ReductionStatus::Retained;
}

TokenValue
StringToSymbol(const string& s)
{
	return TokenValue(s);
}
TokenValue
StringToSymbol(string&& s)
{
	return TokenValue(std::move(s));
}

const string&
SymbolToString(const TokenValue& s) noexcept
{
	return s;
}


struct LeafPred
{
	template<typename _func>
	YB_ATTR_nodiscard YB_PURE bool
	operator()(const TermNode& nd, _func f) const
		noexcept(noexcept(f(nd.Value)))
	{
		return IsLeaf(nd) && f(nd.Value);
	}
};


void
LoadModule_std_continuations(Interpreter& intp)
{
	using namespace Forms;
	auto& ctx(intp.Root.GetRecordRef());

	RegisterStrict(ctx, "call/1cc", Call1CC);
	RegisterStrict(ctx, "continuation->applicative",
		ContinuationToApplicative);
}

void
LoadModule_std_promises(Interpreter& intp)
{
	intp.Perform(R"Unilang(
$provide/let! (promise? memoize $lazy $lazy% $lazy/d $lazy/d% force)
((mods $as-environment (
	$def! (encapsulate% promise? decapsulate) () make-encapsulation-type;
	$defl%! do-force (&prom fwd) $let% ((((&o &env) evf) decapsulate prom))
		$if (null? env) (first (first (decapsulate (fwd prom))))
		(
			$let*% ((&y evf (fwd o) env) (&x decapsulate prom)
				(((&o &env) &evf) x))
				$cond
					((null? env) first (first (decapsulate (fwd prom))))
					((promise? y) $sequence
						($if (eqv? (firstv (rest& (decapsulate y))) eval)
							(assign! evf eval))
						(set-first%! x (first (decapsulate (forward! y))))
						(do-force prom fwd))
					(#t $sequence
						(list% (assign%! o (forward! y)) (assign@! env ()))
						(first (first (decapsulate (fwd prom)))))
		)
)))
(
	$import! mods &promise?,
	$defl/e%! &memoize mods (&x)
		encapsulate% (list (list% (forward! x) ()) #inert),
	$defv/e%! &$lazy mods (.&body) d
		encapsulate% (list (list (move! body) d) eval),
	$defv/e%! &$lazy% mods (.&body) d
		encapsulate% (list (list (move! body) d) eval%),
	$defv/e%! &$lazy/d mods (&e .&body) d
		encapsulate%
			(list (list (move! body) (check-environment (eval e d))) eval),
	$defv/e%! &$lazy/d% mods (&e .&body) d
		encapsulate%
			(list (list (move! body) (check-environment (eval e d))) eval%),
	$defl/e%! &force mods (&x)
		($lambda% (fwd) $if (promise? x) (do-force x fwd) (fwd x))
			($if ($lvalue-identifier? x) id move!)
);
	)Unilang");
}

void
LoadModule_std_strings(Interpreter& intp)
{
	using namespace Forms;
	auto& ctx(intp.Root.GetRecordRef());

	RegisterUnary(ctx, "string?", [](const TermNode& x) noexcept{
		return IsTypedRegular<string>(ReferenceTerm(x));
	});
	RegisterStrict(ctx, "++",
		std::bind(CallBinaryFold<string, ystdex::plus<>>, ystdex::plus<>(),
		string(), std::placeholders::_1));
	RegisterUnary<Strict, const string>(ctx, "string-empty?",
		[](const string& str) noexcept{
			return str.empty();
		});
	RegisterBinary(ctx, "string<-", [](TermNode& x, TermNode& y){
		ResolveTerm([&](TermNode& nd_x, ResolvedTermReferencePtr p_ref_x){
			if(!p_ref_x || p_ref_x->IsModifiable())
			{
				auto& str_x(Unilang::AccessRegular<string>(nd_x, p_ref_x));

				ResolveTerm(
					[&](TermNode& nd_y, ResolvedTermReferencePtr p_ref_y){
					auto& str_y(Unilang::AccessRegular<string>(nd_y, p_ref_y));

					if(Unilang::IsMovable(p_ref_y))
						str_x = std::move(str_y);
					else
						str_x = str_y;
				}, y);
			}
			else
				ThrowNonmodifiableErrorForAssignee();
		}, x);
		return ValueToken::Unspecified;
	});
	RegisterBinary<Strict, const string, const string>(ctx, "string=?",
		ystdex::equal_to<>());
	RegisterStrict(ctx, "string-split", [](TermNode& term){
		return CallBinaryAs<string, const string>(
			[&](string& x, const string& y) -> ReductionStatus{
			if(!x.empty())
			{
				TermNode::Container con(term.get_allocator());
				const auto len(y.length());

				if(len != 0)
				{
					string::size_type pos(0), orig(0);

					while((pos = x.find(y, pos)) != string::npos)
					{
						TermNode::AddValueTo(con, x.substr(orig, pos - orig));
						orig = (pos += len);
					}
					TermNode::AddValueTo(con, x.substr(orig, pos - orig));
				}
				else
					TermNode::AddValueTo(con, std::move(x));
				con.swap(term.GetContainerRef());
				return ReductionStatus::Retained;
			}
			return ReductionStatus::Clean;
		}, term);
	});
	RegisterBinary<Strict, string, string>(ctx, "string-contains?",
		[](const string& x, const string& y){
		return x.find(y) != string::npos;
	});
	RegisterBinary<Strict, string, string>(ctx, "string-contains-ci?",
		[](string x, string y){
		const auto to_lwr([](string& str) noexcept{
			for(auto& c : str)
				c = ystdex::tolower(c);
		});

		to_lwr(x),
		to_lwr(y);
		return x.find(y) != string::npos;
	});
	RegisterUnary(ctx, "string->symbol", [](TermNode& term){
		return ResolveTerm([&](TermNode& nd, ResolvedTermReferencePtr p_ref){
			auto& s(Unilang::AccessRegular<string>(nd, p_ref));

			return Unilang::IsMovable(p_ref) ? StringToSymbol(std::move(s))
				: StringToSymbol(s);
		}, term);
	});
	RegisterUnary<Strict, const TokenValue>(ctx, "symbol->string",
		SymbolToString);
	RegisterUnary<Strict, const string>(ctx, "string->regex",
		[](const string& str){
		return std::regex(str);
	});
	RegisterStrict(ctx, "regex-match?", [](TermNode& term){
		RetainN(term, 2);

		auto i(std::next(term.begin()));
		const auto& str(Unilang::ResolveRegular<const string>(*i));
		const auto& r(Unilang::ResolveRegular<const std::regex>(*++i));

		term.Value = std::regex_match(str, r);
		return ReductionStatus::Clean;
	});
	RegisterStrict(ctx, "regex-replace", [](TermNode& term){
		RetainN(term, 3);

		auto i(term.begin());
		const auto&
			str(Unilang::ResolveRegular<const string>(Unilang::Deref(++i)));
		const auto&
			re(Unilang::ResolveRegular<const std::regex>(Unilang::Deref(++i)));

		return EmplaceCallResultOrReturn(term, string(std::regex_replace(str,
			re, Unilang::ResolveRegular<const string>(Unilang::Deref(++i)))));
	});
}

void
LoadModule_std_math(Interpreter& intp)
{
	using namespace Forms;
	auto& ctx(intp.Root.GetRecordRef());

	RegisterUnary(ctx, "number?",
		ComposeReferencedTermOp(ystdex::bind1(LeafPred(), IsNumberValue)));
	RegisterUnary(ctx, "real?",
		ComposeReferencedTermOp(ystdex::bind1(LeafPred(), IsNumberValue)));
	RegisterUnary(ctx, "rational?",
		ComposeReferencedTermOp(ystdex::bind1(LeafPred(), IsRationalValue)));
	RegisterUnary(ctx, "integer?",
		ComposeReferencedTermOp(ystdex::bind1(LeafPred(), IsIntegerValue)));
	RegisterUnary(ctx, "exact-integer?",
		ComposeReferencedTermOp(ystdex::bind1(LeafPred(), IsExactValue)));
	RegisterUnary<Strict, const NumberLeaf>(ctx, "exact?", IsExactValue);
	RegisterUnary<Strict, const NumberLeaf>(ctx, "inexact?", IsInexactValue);
	RegisterUnary<Strict, const NumberLeaf>(ctx, "finite?", IsFinite);
	RegisterUnary<Strict, const NumberLeaf>(ctx, "infinite?", IsInfinite);
	RegisterUnary<Strict, const NumberLeaf>(ctx, "nan?", IsNaN);
	RegisterBinary<Strict, const NumberLeaf, const NumberLeaf>(ctx, "=?",
		Equal);
	RegisterBinary<Strict, const NumberLeaf, const NumberLeaf>(ctx, "<?", Less);
	RegisterBinary<Strict, const NumberLeaf, const NumberLeaf>(ctx, ">?",
		Greater);
	RegisterBinary<Strict, const NumberLeaf, const NumberLeaf>(ctx, "<=?",
		LessEqual);
	RegisterBinary<Strict, const NumberLeaf, const NumberLeaf>(ctx, ">=?",
		GreaterEqual);
	RegisterUnary<Strict, const NumberLeaf>(ctx, "zero?", IsZero);
	RegisterUnary<Strict, const NumberLeaf>(ctx, "positive?", IsPositive);
	RegisterUnary<Strict, const NumberLeaf>(ctx, "negative?", IsNegative);
	RegisterUnary<Strict, const NumberLeaf>(ctx, "odd?", IsOdd);
	RegisterUnary<Strict, const NumberLeaf>(ctx, "even?", IsEven);
	RegisterBinary<Strict, NumberNode, NumberNode>(ctx, "max", Max);
	RegisterBinary<Strict, NumberNode, NumberNode>(ctx, "min", Min);
	RegisterUnary<Strict, NumberNode>(ctx, "add1", Add1);
	RegisterBinary<Strict, NumberNode, NumberNode>(ctx, "+", Plus);
	RegisterUnary<Strict, NumberNode>(ctx, "sub1", Sub1);
	RegisterBinary<Strict, NumberNode, NumberNode>(ctx, "-", Minus);
	RegisterBinary<Strict, NumberNode, NumberNode>(ctx, "*", Multiplies);
	RegisterBinary<Strict, NumberNode, NumberNode>(ctx, "/", Divides);
	RegisterUnary<Strict, NumberNode>(ctx, "abs", Abs);
	RegisterBinary<Strict, NumberNode, NumberNode>(ctx, "floor/", FloorDivides);
	RegisterBinary<Strict, NumberNode, NumberNode>(ctx, "floor-quotient",
		FloorQuotient);
	RegisterBinary<Strict, NumberNode, NumberNode>(ctx, "floor-remainder",
		FloorRemainder);
	RegisterBinary<Strict, NumberNode, NumberNode>(ctx, "truncate/",
		TruncateDivides);
	RegisterBinary<Strict, NumberNode, NumberNode>(ctx, "truncate-quotient",
		TruncateQuotient);
	RegisterBinary<Strict, NumberNode, NumberNode>(ctx, "truncate-remainder",
		TruncateRemainder);
	RegisterBinary<Strict, const int, const int>(ctx, "div",
		[](const int& e1, const int& e2){
		if(e2 != 0)
			return e1 / e2;
		throw std::domain_error("Runtime error: divided by zero.");
	});
	RegisterBinary<Strict, const int, const int>(ctx, "mod",
		[](const int& e1, const int& e2){
		if(e2 != 0)
			return e1 % e2;
		throw std::domain_error("Runtime error: divided by zero.");
	});
	RegisterUnary<Strict, const int&>(ctx, "itos", [](const int& x){
		return string(YSLib::make_string_view(std::to_string(int(x))));
	});
	RegisterUnary<Strict, const string>(ctx, "stoi", [](const string& x){
		return int(std::stoi(YSLib::to_std_string(x)));
	});
}

void
LoadModule_std_io(Interpreter& intp)
{
	using namespace Forms;
	auto& ctx(intp.Root.GetRecordRef());

	RegisterStrict(ctx, "newline", [&](TermNode& term){
		RetainN(term, 0);
		std::cout << std::endl;
		return ReduceReturnUnspecified(term);
	});
	RegisterUnary<Strict, const string>(ctx, "readable-file?",
		[](const string& str) noexcept{
		return YSLib::ufexists(str.c_str());
	});
	RegisterStrict(ctx, "load", [&](TermNode& term, Context& c){
		RetainN(term);
		return RelaySwitched(c, NameTypedContextHandler([&]{
			term = intp.ReadFrom(*intp.OpenUnique(std::move(
				Unilang::ResolveRegular<string>(*std::next(term.begin())))));
			return ReduceOnce(term, c);
		}, "load-external"));
	});
	RegisterUnary(ctx, "write", [&](TermNode& term){
		WriteTermValue(std::cout, term);
		return ValueToken::Unspecified;
	});
	RegisterUnary(ctx, "display", [&](TermNode& term){
		DisplayTermValue(std::cout, term);
		return ValueToken::Unspecified;
	});
	RegisterUnary<Strict, const string>(ctx, "put", [&](const string& str){
		YSLib::IO::StreamPut(std::cout, str.c_str());
		return ValueToken::Unspecified;
	});
	intp.Perform(R"Unilang(
$defl! puts (&s) $sequence (put s) (() newline);
	)Unilang");
}

void
LoadModule_std_system(Interpreter& intp)
{
	using namespace Forms;
	auto& ctx(intp.Root.GetRecordRef());

	RegisterUnary<Strict, const string>(ctx, "env-get", [](const string& var){
		string res(var.get_allocator());

		YSLib::FetchEnvironmentVariable(res, var.c_str());
		return res;
	});
}

void
LoadModule_std_modules(Interpreter& intp)
{
	intp.Perform(R"Unilang(
$provide/let! (registered-requirement? register-requirement!
	unregister-requirement! find-requirement-filename)
((mods $as-environment (
	$import! std.strings &string-empty? &++ &string->symbol;

	$defl! requirement-error ()
		raise-error "Empty requirement name found.",
	(
	$def! registry () make-environment;
	$defl! bound-name? (&req)
		$and (eval (list bound? req) registry)
			(not? (string-empty? (eval (string->symbol req) registry))),
	$defl! set-value! (&req &v)
		eval (list $def! (string->symbol req) v) registry
	),
	$def! prom_pathspecs ($remote-eval% $lazy std.promises)
		$let ((spec ($remote-eval% env-get std.system) "UNILANG_PATH"))
			$if (string-empty? spec) (list "./?" "./?.u" "./?.txt")
				(($remote-eval% string-split std.strings) spec ";"),
	(
	$def! placeholder ($remote-eval% string->regex std.strings) "\?",
	$defl! get-requirement-filename (&specs &req)
		$if (null? specs)
			(raise-error (++ "No module for requirement '" req
				"' found."))
			(
				$let* ((spec first& specs) (path ($remote-eval% regex-replace
					std.strings) spec placeholder req))
					$if (($remote-eval% readable-file? std.io) path) path
						(get-requirement-filename (rest& specs) req)
			)
	)
)))
(
	$defl/e! &registered-requirement? mods (&req)
		$if (string-empty? req) (() requirement-error) (bound-name? req),
	$defl/e! &register-requirement! mods (&req)
		$if (string-empty? req) (() requirement-error)
			($if (bound-name? req) (raise-error (++ "Requirement '" req
				"' is already registered.")) (set-value! req req)),
	$defl/e! &unregister-requirement! mods (&req)
		$if (string-empty? req) (() requirement-error)
			($if (bound-name? req) (set-value! req "") (raise-error
				(++ "Requirement '" req "' is not registered."))),
	$defl/e! &find-requirement-filename mods (&req)
		get-requirement-filename
			(($remote-eval% force std.promises) prom_pathspecs) req
);
$defl%! require (&req)
	$if (registered-requirement? req) #inert
		($let ((filename find-requirement-filename req))
			$sequence (register-requirement! (move! req))
				(($remote-eval% load std.io) filename));
	)Unilang");
}

[[gnu::nonnull(2)]] void
PreloadExternal(Interpreter& intp, const char* filename)
{
	try
	{
		auto term(intp.ReadFrom(*intp.OpenUnique(filename)));

		intp.Evaluate(term);
	}
	catch(...)
	{
		std::throw_with_nested(UnilangException(
			ystdex::sfmt("Failed loading external unit '%s'.", filename)));
	}
}

void
LoadFunctions(Interpreter& intp, bool jit, int& argc, char* argv[])
{
	using namespace Forms;
	using namespace std::placeholders;
	auto& ctx(intp.Root);
	auto& env(ctx.GetRecordRef());

	if(jit)
		SetupJIT(ctx);
	env.Bindings["ignore"].Value = ValueToken::Ignore;
	RegisterStrict(ctx, "eq?", Eq);
	RegisterStrict(ctx, "eqv?", EqValue);
	RegisterForm(ctx, "$if", If);
	RegisterUnary(ctx, "null?", ComposeReferencedTermOp(IsEmpty));
	RegisterUnary(ctx, "branch?", ComposeReferencedTermOp(IsBranch));
	RegisterUnary(ctx, "list?", ComposeReferencedTermOp(IsList));
	RegisterUnary(ctx, "reference?", IsReferenceTerm);
	RegisterUnary(ctx, "unique?", IsUniqueTerm);
	RegisterUnary(ctx, "modifiable?", IsModifiableTerm);
	RegisterUnary(ctx, "bound-lvalue?", IsBoundLValueTerm);
	RegisterUnary(ctx, "uncollapsed?", IsUncollapsedTerm);
	RegisterStrict(ctx, "as-const",
		ystdex::bind1(Qualify, TermTags::Nonmodifying));
	RegisterStrict(ctx, "expire",
		ystdex::bind1(Qualify, TermTags::Unique));
	RegisterStrict(ctx, "move!",
		std::bind(DoMoveOrTransfer, std::ref(LiftOtherOrCopy), _1));
	RegisterBinary(ctx, "assign@!", [](TermNode& x, TermNode& y){
		return DoAssign(ystdex::bind1(static_cast<void(&)(TermNode&,
			TermNode&)>(LiftOther), std::ref(y)), x);
	});
	RegisterStrict(ctx, "cons", Cons);
	RegisterStrict(ctx, "cons%", ConsRef);
	RegisterUnary<Strict, const TokenValue>(ctx, "desigil", [](TokenValue s){
		return TokenValue(!s.empty() && (s.front() == '&' || s.front() == '%')
			? s.substr(1) : std::move(s));
	});
	RegisterStrict(ctx, "eval", Eval);
	RegisterStrict(ctx, "eval%", EvalRef);
	RegisterUnary<Strict, const string>(ctx, "bound?",
		[](const string& id, Context& c){
		return bool(ResolveName(c, id).first);
	});
	RegisterForm(ctx, "$resolve-identifier",
		std::bind(DoResolve, std::ref(ResolveIdentifier), _1, _2));
	RegisterUnary<Strict, const EnvironmentReference>(ctx, "lock-environment",
		[](const EnvironmentReference& wenv) noexcept{
		return wenv.Lock();
	});
	RegisterForm(ctx, "$move-resolved!",
		std::bind(DoResolve, std::ref(MoveResolvedValue), _1, _2));
	RegisterStrict(ctx, "make-environment", MakeEnvironment);
	RegisterUnary<Strict, const shared_ptr<Environment>>(ctx,
		"weaken-environment", [](const shared_ptr<Environment>& p_env) noexcept{
		return EnvironmentReference(p_env);
	});
	RegisterForm(ctx, "$def!", Define);
	RegisterForm(ctx, "$vau/e", VauWithEnvironment);
	RegisterForm(ctx, "$vau/e%", VauWithEnvironmentRef);
	RegisterStrict(ctx, "wrap", Wrap);
	RegisterStrict(ctx, "wrap%", WrapRef);
	RegisterStrict(ctx, "unwrap", Unwrap);
	RegisterUnary<Strict, const string>(ctx, "raise-error",
		[] (const string& str){
		throw UnilangException(str.c_str());
	});
	RegisterUnary<Strict, const string>(ctx, "raise-invalid-syntax-error",
		[](const string& str){
		throw InvalidSyntax(str.c_str());
	});
	RegisterStrict(ctx, "check-list-reference", CheckListReference);
	RegisterStrict(ctx, "make-encapsulation-type", MakeEncapsulationType);
	RegisterStrict(ctx, "get-current-environment", GetCurrentEnvironment);
	RegisterForm(ctx, "$vau", Vau);
	RegisterForm(ctx, "$vau%", VauRef);
	intp.Perform(R"Unilang(
$def! lock-current-environment (wrap ($vau () d lock-environment d));
$def! $quote $vau% (x) #ignore $move-resolved! x;
$def! id wrap ($vau% (%x) #ignore $move-resolved! x);
$def! idv wrap $quote;
	)Unilang");
	RegisterStrict(ctx, "list", ReduceBranchToListValue);
	RegisterStrict(ctx, "list%", ReduceBranchToList);
	intp.Perform(R"Unilang(
$def! $lvalue-identifier? $vau (&s) d
	eval (list bound-lvalue? (list $resolve-identifier s)) d;
$def! forward! wrap
	($vau% (%x) #ignore $if ($lvalue-identifier? x) x (move! x));
$def! $remote-eval $vau (&o &e) d eval o (eval e d);
$def! $remote-eval% $vau% (&o &e) d eval% o (eval e d);
$def! $set! $vau (&e &formals .&expr) d
	eval (list $def! formals (unwrap eval) expr d) (eval e d);
$def! $wvau $vau (&formals &ef .&body) d
	wrap (eval (cons $vau (cons formals (cons ef (move! body)))) d);
$def! $wvau% $vau (&formals &ef .&body) d
	wrap (eval (cons $vau% (cons formals (cons ef (move! body)))) d);
$def! $wvau/e $vau (&p &formals &ef .&body) d
	wrap (eval (cons $vau/e (cons p (cons formals (cons ef (move! body))))) d);
$def! $wvau/e% $vau (&p &formals &ef .&body) d
	wrap (eval (cons $vau/e% (cons p (cons formals (cons ef (move! body))))) d);
$def! $lambda $vau (&formals .&body) d
	wrap (eval (cons $vau (cons formals (cons #ignore (move! body)))) d);
$def! $lambda% $vau (&formals .&body) d
	wrap (eval (cons $vau% (cons formals (cons #ignore (move! body)))) d);
$def! $lambda/e $vau (&p &formals .&body) d
	wrap (eval
		(cons $vau/e (cons p (cons formals (cons #ignore (move! body))))) d);
$def! $lambda/e% $vau (&p &formals .&body) d
	wrap (eval
		(cons $vau/e% (cons p (cons formals (cons #ignore (move! body))))) d);
	)Unilang");
	RegisterForm(ctx, "$sequence", Sequence);
	intp.Perform(R"Unilang(
$def! collapse $lambda% (%x)
	$if (uncollapsed? x) (($if ($lvalue-identifier? x) ($lambda% (%x) x) id)
		($if (modifiable? x) (idv x) (as-const (idv x)))) ($move-resolved! x);
$def! assign%! $lambda (&x &y) assign@! (forward! x) (forward! (collapse y));
$def! assign! $lambda (&x &y) assign@! (forward! x) (idv (collapse y));
$def! apply $lambda% (&appv &arg .&opt)
	eval% (cons% () (cons% (unwrap (forward! appv)) (forward! arg)))
		($if (null? opt) (() make-environment)
			(($lambda ((&e .&eopt))
				$if (null? eopt) e
					(raise-invalid-syntax-error
						"Syntax error in applying form.")) opt));
$def! list* $lambda (&head .&tail)
	$if (null? tail) (forward! head)
		(cons (forward! head) (apply list* (move! tail)));
$def! list*% $lambda (&head .&tail)
	$if (null? tail) (forward! head)
		(cons% (forward! head) (apply list*% (move! tail)));
$def! $defv! $vau (&$f &formals &ef .&body) d
	eval (list $set! d $f $vau formals ef (move! body)) d;
$defv! $defv%! (&$f &formals &ef .&body) d
	eval (list $set! d $f $vau% formals ef (move! body)) d;
$defv! $defv/e! (&$f &p &formals &ef .&body) d
	eval (list* $def! $f $vau/e p formals ef (move! body)) d;
$defv! $defv/e%! (&$f &e &formals &ef .&body) d
	eval (list $set! d $f $vau/e% e formals ef (move! body)) d;
$defv! $defw! (&f &formals &ef .&body) d
	eval (list* $def! f $wvau formals ef (move! body)) d;
$defv! $defw%! (&f &formals &ef .&body) d
	eval (list* $def! f $wvau% formals ef (move! body)) d;
$defv! $defw/e! (&f &p &formals &ef .&body) d
	eval (list* $def! f $wvau/e p formals ef (move! body)) d;
$defv! $defw/e%! (&f &p &formals &ef .&body) d
	eval (list* $def! f $wvau/e% p formals ef (move! body)) d;
$defv! $defl! (f formals .body) d
	eval (list $set! d f $lambda formals (move! body)) d;
$defv! $defl%! (&f &formals .&body) d
	eval (list $set! d f $lambda% formals (move! body)) d;
$defv! $defl/e! (&f &e &formals .&body) d
	eval (list $set! d f $lambda/e e formals (move! body)) d;
$defv! $defl/e%! (&f &p &formals .&body) d
	eval (list* $def! f $lambda/e% p formals (move! body)) d;
$defw%! forward-first% (&appv (&x .)) d
	apply (forward! appv) (list% ($move-resolved! x)) d;
$defl%! first (&l)
	$if ($lvalue-identifier? l) (($lambda% ((@x .)) $if (uncollapsed? x)
		($if (modifiable? x) (idv x) (as-const (idv x))) x) l)
		(forward-first% idv (expire l));
$defl%! first@ (&l) ($lambda% ((@x .)) x) (check-list-reference (forward! l));
$defl%! first% (&l)
	($lambda (fwd (@x .)) fwd x) ($if ($lvalue-identifier? l) id expire) l;
$defl%! first& (&l)
	($lambda% ((@x .)) $if (uncollapsed? x) (idv x) x)
		(check-list-reference (forward! l));
$defl! firstv ((&x .)) $move-resolved! x;
$defl! rest ((#ignore .xs)) xs;
$defl! rest% ((#ignore .%xs)) move! xs;
$defl%! rest& (&l)
	($lambda% ((#ignore .&xs)) xs) (check-list-reference (forward! l));
$defl! set-first%! (&l &x) assign%! (first@ (forward! l)) (forward! x);
$defl! equal? (&x &y)
	$if ($if (branch? x) (branch? y) #f)
		($if (equal? (first& x) (first& y))
		(equal? (rest& x) (rest& y)) #f) (eqv? x y);
$defl%! check-environment (&e) $sequence (eval% #inert e) (forward! e);
$defv%! $cond &clauses d
	$if (null? clauses) #inert
		(apply ($lambda% ((&test .&body) .&clauses)
			$if (eval test d) (eval% (move! body) d)
				(apply (wrap $cond) (move! clauses) d)) (move! clauses));
$defv%! $when (&test .&exprseq) d
	$if (eval test d) (eval% (list* () $sequence (move! exprseq)) d);
$defv%! $unless (&test .&exprseq) d
	$if (eval test d) #inert (eval% (list* () $sequence (move! exprseq)) d);
$defv%! $while (&test .&exprseq) d
	$when (eval test d)
		(eval% (list* () $sequence exprseq) d)
		(eval% (list* () $while (move! test) (move! exprseq)) d);
$defv%! $until (&test .&exprseq) d
	$unless (eval test d)
		(eval% (list* () $sequence exprseq) d)
		(eval% (list* () $until (move! test) (move! exprseq)) d);
$defl! not? (x) eqv? x #f;
$defv! $and x d $cond
	((null? x) #t)
	((null? (rest x)) eval (first x) d)
	((eval (first x) d) apply (wrap $and) (rest x) d)
	(#t #f);
$defv! $or x d $cond
	((null? x) #f)
	((null? (rest x)) eval (first x) d)
	(#t ($lambda (r) $if r r
		(apply (wrap $or) (rest x) d)) (eval (move! (first x)) d));
$defl%! and &x $sequence
	($defl%! and-aux (&h &l) $if (null? l) (forward! h)
		(and-aux ($if h (forward! (first% l)) #f) (forward! (rest% l))))
	(and-aux #t (forward! x));
$defl%! or &x $sequence
	($defl%! or-aux (&h &l) $if (null? l) (forward! h)
		($let% ((&c forward! (first% l)))
			or-aux (forward! ($if c c h)) (forward! (rest% l))))
	(or-aux #f (forward! x));
$defw%! accr (&l &pred? &base &head &tail &sum) d
	$if (apply pred? (list% l) d) (forward! base)
		(apply sum (list% (apply head (list% l) d)
			(apply accr (list% (apply tail (list% l) d)
			pred? (forward! base) head tail sum) d)) d);
$defw%! foldr1 (&kons &knil &l) d
	apply accr (list% (($lambda ((.@xs)) xs) l) null? (forward! knil)
		($if ($lvalue-identifier? l) ($lambda (&l) first% l)
		($lambda (&l) expire (first% l))) rest% kons) d;
$defw%! map1 (&appv &l) d
	foldr1 ($lambda (%x &xs) cons%
		(apply appv (list% ($move-resolved! x)) d) (move! xs)) () (forward! l);
$defl! list-concat (&x &y) foldr1 cons% (forward! y) (forward! x);
$defl! append (.&ls) foldr1 list-concat () (move! ls);
$defl! filter (&accept? &ls) apply append
	(map1 ($lambda (&x) $if (apply accept? (list x)) (list x) ()) ls);
$def! ($let $let% $let* $let*% $letrec) ($lambda (&ce)
(
	$def! mods () ($lambda/e ce ()
	(
		$defl%! rulist (&l)
			$if ($lvalue-identifier? l)
				(accr (($lambda ((.@xs)) xs) l) null? ()
					($lambda% (%l) $sequence ($def! %x idv (first@ l))
						(($if (uncollapsed? x) idv expire) (expire x))) rest%
					($lambda (%x &xs) (cons% ($resolve-identifier x) (move! xs))))
				(idv (forward! l));
		$defl%! list-extract-first (&l) map1 first (forward! l);
		$defl%! list-extract-rest% (&l) map1 rest% (forward! l);
		$defv%! $lqual (&ls) d
			($if (eval (list $lvalue-identifier? ls) d) id rulist) (eval% ls d);
		$defv%! $lqual* (&x) d
			($if (eval (list $lvalue-identifier? x) d) id expire) (eval% x d);
		$defl%! mk-let ($ctor &bindings &body)
			list* () (list* $ctor (list-extract-first bindings)
				(list (move! body))) (list-extract-rest% bindings);
		$defl%! mk-let* ($let $let* &bindings &body)
			$if (null? bindings) (list* $let () (move! body))
				(list $let (list (first% ($lqual* bindings)))
				(list* $let* (rest% ($lqual* bindings)) (move! body)));
		$defl%! mk-letrec ($let &bindings &body)
			list $let () $sequence (list $def! (list-extract-first bindings)
				(list* () list (list-extract-rest% bindings))) (move! body);
		() lock-current-environment
	));
	$defv/e%! $let mods (&bindings .&body) d
		eval% (mk-let $lambda ($lqual bindings) (move! body)) d;
	$defv/e%! $let% mods (&bindings .&body) d
		eval% (mk-let $lambda% ($lqual bindings) (move! body)) d;
	$defv/e%! $let* mods (&bindings .&body) d
		eval% (mk-let* $let $let* ($lqual* bindings) (move! body)) d;
	$defv/e%! $let*% mods (&bindings .&body) d
		eval% (mk-let* $let% $let*% ($lqual* bindings) (move! body)) d;
	$defv/e%! $letrec mods (&bindings .&body) d
		eval% (mk-letrec $let ($lqual bindings) (move! body)) d;
	map1 move! (list% $let $let% $let* $let*% $letrec)
)) (() get-current-environment);
$defv! $as-environment (.&body) d
	eval (list $let () (list $sequence (move! body)
		(list () lock-current-environment))) d;
$defw! derive-current-environment (.&envs) d
	apply make-environment (append envs (list d)) d;
$defl! make-standard-environment () () lock-current-environment;
$defv! $bindings/p->environment (&parents .&bindings) d $sequence
	($def! res apply make-environment (map1 ($lambda (x) eval x d) parents))
	(eval (list $set! res (map1 first bindings)
		(list* () list (map1 rest bindings))) d)
	res;
$defv! $bindings->environment (.&bindings) d
	eval (list* $bindings/p->environment () bindings) d;
$defl! symbols->imports (&symbols)
	list* () list% (map1 ($lambda (&s) list forward! (desigil s))
		(forward! symbols));
$defv! $provide/let! (&symbols &bindings .&body) d
	eval% (list% $let (forward! bindings) $sequence
		(move! body) (list% $set! d (append symbols ((unwrap list%) .))
		(symbols->imports symbols)) (list () lock-current-environment)) d;
$defv! $provide! (&symbols .&body) d
	eval (list*% $provide/let! (forward! symbols) () (move! body)) d;
$defv! $import! (&e .&symbols) d
	eval% (list $set! d (append symbols ((unwrap list%) .))
		(symbols->imports symbols)) (eval e d);
	)Unilang");
	// NOTE: Supplementary functions.
	RegisterStrict(ctx, "random.choice", [&](TermNode& term){
		RetainN(term);
		return ResolveTerm([&](TermNode& nd, ResolvedTermReferencePtr p_ref){
			if(IsBranchedList(nd))
			{
				static std::random_device rd;
				static std::mt19937 mt(rd());

				LiftOtherOrCopy(term, *std::next(nd.begin(),
					std::iterator_traits<TermNode::iterator>::difference_type(
					std::uniform_int_distribution<size_t>(0, nd.size() - 1)(mt)
					)), Unilang::IsMovable(p_ref));
				return ReductionStatus::Retained;
			}
			ThrowInsufficientTermsError(nd, p_ref);
		}, *std::next(term.begin()));
	});
	RegisterUnary<Strict, const int>(ctx, "sys.exit", [&](int status){
		std::exit(status);
	});

	auto& rctx(intp.Root);
	const auto load_std_module([&](string_view module_name,
		void(&load_module)(Interpreter& intp)){
		LoadModuleChecked(rctx, "std." + string(module_name),
			std::bind(load_module, std::ref(intp)));
	});

	load_std_module("continuations", LoadModule_std_continuations);
	load_std_module("promises", LoadModule_std_promises);
	load_std_module("strings", LoadModule_std_strings);
	load_std_module("math", LoadModule_std_math);
	load_std_module("io", LoadModule_std_io);
	load_std_module("system", LoadModule_std_system);
	load_std_module("modules", LoadModule_std_modules);
	// NOTE: FFI and external libraries support.
	InitializeFFI(intp);
	// NOTE: Qt support.
	InitializeQt(intp, argc, argv);
	// NOTE: Prevent the ground environment from modification.
	env.Frozen = true;
	intp.SaveGround();
	// NOTE: User environment initialization.
	PreloadExternal(intp, "init.txt");
}

template<class _tString>
auto
Quote(_tString&& str) -> decltype(ystdex::quote(yforward(str), '\''))
{
	return ystdex::quote(yforward(str), '\'');
}


const struct Option
{
	const char *prefix, *option_arg;
	std::vector<const char*> option_details;

	Option(const char* pfx, const char* opt_arg,
		std::initializer_list<const char*> il)
		: prefix(pfx), option_arg(opt_arg), option_details(il)
	{}
} OptionsTable[]{
	{"-h, --help", "", {"Print this message."}},
	{"-e", " [STRING]", {"Evaluate a string if the following argument exists."
		" This option can occur more than once and combined with SRCPATH.\n"
		"\tEach instance of this option (with its optional argument) will be"
		" evaluated in order before evaluate the script specified by SRCPATH"
		" (if any)."}}
};

const std::array<const char*, 3> DeEnvs[]{
	{{"ECHO", "", "If set, echo the result after evaluating each string."}},
	{{"UNILANG_NO_JIT", "", "If set, disable any optimization based on the JIT"
		" (just-in-time) execution."}},
	{{"UNILANG_NO_SRCINFO", "", "If set, disable the source information from"
		" the source code for diagnostics. The source names are used"
		" regardless of this variable."}},
	{{"UNILANG_PATH", "", "Unilang loader path template string."}}
};


void
PrintHelpMessage(const string& prog)
{
	using ystdex::sfmt;
	auto& os(std::cout);

	ystdex::write_ntcts(os, sfmt("Usage: \"%s\" [OPTIONS ...] SRCPATH"
		" [OPTIONS ... [-- [ARGS...]]]\n"
		"  or:  \"%s\" [OPTIONS ... [-- [[SRCPATH] ARGS...]]]\n"
		"\tThis program is an interpreter of Unilang.\n"
		"\tThere are two execution modes, scripting mode and interactive mode,"
		" exclusively. In scripting mode, a script file specified in the"
		" command line arguments is run. Otherwise, the program runs in the"
		" interactive mode and the REPL (read-eval-print loop) is entered,"
		" see below for details.\n"
		"\tThere are no checks on the values. Any behaviors depending"
		" on the locale-specific values are unspecified.\n"
		"\tCurrently accepted environment variable are:\n\n",
		prog.c_str(), prog.c_str()).c_str());
	for(const auto& env : DeEnvs)
		ystdex::write_ntcts(os, sfmt("  %s\n\t%s Default value is %s.\n\n",
			env[0], env[2], env[1][0] == '\0' ? "empty"
			: Quote(string(env[1])).c_str()).c_str());
	ystdex::write_literal(os,
		"SRCPATH\n"
		"\tThe source path. It is handled if and only if this program runs in"
		" scripting mode. In this case, SRCPATH is the 1st command line"
		" argument not recognized as an option (see below). Otherwise, the"
		" command line argument is treated as an option.\n"
		"\tSRCPATH shall specify a path to a source file, or"
		" the special value '-' which indicates the standard input. The source"
		" specified by SRCPATH shall have Unilang source tokens, which are to"
		" be read and evaluated in the initial environment of the interpreter."
		" Otherwise, errors are raise to reject the source.\n\n"
		"OPTIONS ...\nOPTIONS ... -- [[SRCPATH] ARGS ...]\n"
		"\tThe options and arguments for the program execution. After '--',"
		" options parsing is turned off and every remained command line"
		" argument (if any) is interpreted as an argument, except that the"
		" 1st command line argument is treated as SRCPATH (implying scripting"
		" mode) when there is no SRCPATH before '--'.\n"
		"\tRecognized options are handled in this program, and the remained"
		" arguments will be passed to the Unilang user program (in the script"
		" or in the interactive environment) which can access the arguments via"
		" appropriate API.\n\t"
		"The recognized options are:\n\n");
	for(const auto& opt : OptionsTable)
	{
		ystdex::write_ntcts(os,
			sfmt("  %s%s\n", opt.prefix, opt.option_arg).c_str());
		for(const auto& des : opt.option_details)
			ystdex::write_ntcts(os, sfmt("\t%s\n", des).c_str());
		os << '\n';
	}
	ystdex::write_literal(os,
		"The exit status is 0 on success and 1 otherwise.\n");
}


#define APP_NAME "Unilang interpreter"
#define APP_VER "0.11.77"
#define APP_PLATFORM "[C++11] + YSLib"
constexpr auto
	title(APP_NAME " " APP_VER " @ (" __DATE__ ", " __TIME__ ") " APP_PLATFORM);

} // unnamed namespace;

} // namespace Unilang;

int
main(int argc, char* argv[])
{
	using namespace Unilang;
	using YSLib::LoggedEvent;

	return YSLib::FilterExceptions([&]{
		const YSLib::CommandArguments xargv(argc, argv);
		const auto xargc(xargv.size());

		if(xargc > 1)
		{
			vector<string> args;
			bool opt_trans(true);
			bool requires_eval = {};
			vector<string> eval_strs;

			for(size_t i(1); i < xargc; ++i)
			{
				string arg(xargv[i]);

				if(opt_trans)
				{
					if(YB_UNLIKELY(arg == "--"))
					{
						opt_trans = {};
						continue;
					}
					if(arg == "-h" || arg == "--help")
					{
						PrintHelpMessage(xargv[0]);
						return;
					}
					else if(arg == "-e")
					{
						requires_eval = true;
						continue;
					}
				}
				if(requires_eval)
				{
					eval_strs.push_back(std::move(arg));
					requires_eval = {};
				}
				else
					args.push_back(std::move(arg));
			}
			if(!args.empty())
			{
				auto src(std::move(args.front()));

				args.erase(args.begin());

				const auto p_cmd_args(YSLib::LockCommandArguments());

				p_cmd_args->Arguments = std::move(args);

				Interpreter intp{};

				LoadFunctions(intp, Unilang_UseJIT, argc, argv);
				llvm_main();
				for(const auto& str : eval_strs)
					intp.RunLine(str);
				intp.RunScript(std::move(src));
			}
			else if(!eval_strs.empty())
			{
				Interpreter intp{};

				LoadFunctions(intp, Unilang_UseJIT, argc, argv);
				for(const auto& str : eval_strs)
					intp.RunLine(str);
			}
		}
		else if(xargc == 1)
		{
			using namespace std;

			Unilang::Deref(YSLib::LockCommandArguments()).Reset(argc, argv);

			Interpreter intp{};

			cout << title << endl << "Initializing the interpreter "
				<< (Unilang_UseJIT ? "[JIT enabled]" : "[JIT disabled]")
				<< " ..." << endl;
			LoadFunctions(intp, Unilang_UseJIT, argc, argv);
			llvm_main();
			cout << "Initialization finished." << endl;
			cout << "Type \"exit\" to exit." << endl << endl;
			intp.Run();
		}
	}, yfsig, YSLib::Alert) ? EXIT_FAILURE : EXIT_SUCCESS;
}

