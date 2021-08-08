// © 2020-2021 Uniontech Software Technology Co.,Ltd.

#include "Interpreter.h" // for Interpreter, ValueObject;
#include <cstdlib> // for std::getenv;
#include "Context.h" // for EnvironmentSwitcher,
//	Unilang::SwitchToFreshEnvironment;
#include <ystdex/scope_guard.hpp> // for ystdex::guard;
#include <ystdex/invoke.hpp> // for ystdex::invoke;
#include <functional> // for std::bind, std::placeholders;
#include "Forms.h" // for Forms::RetainN, CallBinaryFold;
#include "Exception.h" // for ThrowNonmodifiableErrorForAssignee,
//	ThrowInvalidTokenError;
#include "BasicReduction.h" // for ReductionStatus;
#include "TermAccess.h" // for ResolvedTermReferencePtr, Unilang::ResolveTerm,
//	ComposeReferencedTermOp, IsBoundLValueTerm, IsUncollapsedTerm,
//	EnvironmentReference, TermNode, IsBranchedList;
#include <iterator> // for std::next, std::iterator_traits;
#include "Lexical.h" // for IsUnilangSymbol;
#include "Evaluation.h" // for RegisterStrict, ThrowInsufficientTermsError;
#include <ystdex/functor.hpp> // for ystdex::plus, ystdex::equal_to,
//	ystdex::less, ystdex::less_equal, ystdex::greater, ystdex::greater_equal,
//	ystdex::minus, ystdex::multiplies;
#include <regex> // for std::regex, std::regex_match, std::regex_replace;
#include <YSLib/Core/YModules.h>
#include YFM_YSLib_Adaptor_YAdaptor // for YSLib::ufexists;
#include "Arithmetic.h" // for Number;
#include <ystdex/functional.hpp> // for ystdex::bind1;
#include <iostream> // for std::cout, std::endl;
#include <random> // for std::random_device, std::mt19937,
//	std::uniform_int_distribution;
#include <cstdlib> // for std::exit;
#include "UnilangFFI.h"
#include "JIT.h"
#include YFM_YSLib_Core_YException // for YSLib::FilterExceptions, YSLib::Alert;

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
	// TODO: Freeze?
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
	Forms::RetainN(term);
	Unilang::ResolveTerm([&](TermNode& nd, ResolvedTermReferencePtr p_ref){
		f(term, nd, !p_ref || p_ref->IsModifiable());
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

template<typename _func>
auto
CheckSymbol(string_view id, _func f) -> decltype(f())
{
	if(IsUnilangSymbol(id))
		return f();
	ThrowInvalidTokenError(id);
}

YB_ATTR_nodiscard ReductionStatus
DoResolve(TermNode(&f)(const Context&, string_view), TermNode& term,
	const Context& c)
{
	Forms::RetainN(term);
	Unilang::ResolveTerm([&](TermNode& nd, ResolvedTermReferencePtr p_ref){
		const string_view id(Unilang::AccessRegular<TokenValue>(nd, p_ref));

		term = CheckSymbol(id, std::bind(f, std::ref(c), id));
	}, *std::next(term.begin()));
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

void
LoadModule_std_strings(Interpreter& intp)
{
	using namespace Forms;
	auto& renv(intp.Root.GetRecordRef());

	RegisterStrict(renv, "++",
		std::bind(CallBinaryFold<string, ystdex::plus<>>, ystdex::plus<>(),
		string(), std::placeholders::_1));
	RegisterUnary<Strict, const string>(renv, "string-empty?",
		[](const string& str) noexcept{
			return str.empty();
		});
	RegisterBinary<>(renv, "string<-", [](TermNode& x, TermNode& y){
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
	RegisterBinary<Strict, const string, const string>(renv, "string=?",
		ystdex::equal_to<>());
	RegisterBinary<Strict, string, string>(renv, "string-contains?",
		[](const string& x, const string& y){
		return x.find(y) != string::npos;
	});
	RegisterBinary<Strict, string, string>(renv, "string-contains-ci?",
		[](string x, string y){
		const auto to_lwr([](string& str) noexcept{
			for(auto& c : str)
				c = ystdex::tolower(c);
		});

		to_lwr(x),
		to_lwr(y);
		return x.find(y) != string::npos;
	});
	RegisterUnary<>(renv, "string->symbol", [](TermNode& term){
		return ResolveTerm([&](TermNode& nd, ResolvedTermReferencePtr p_ref){
			auto& s(Unilang::AccessRegular<string>(nd, p_ref));

			return Unilang::IsMovable(p_ref) ? StringToSymbol(std::move(s))
				: StringToSymbol(s);
		}, term);
	});
	RegisterUnary<Strict, const TokenValue>(renv, "symbol->string",
		SymbolToString);
	RegisterUnary<Strict, const string>(renv, "string->regex",
		[](const string& str){
		return std::regex(str);
	});
	RegisterStrict(renv, "regex-match?", [](TermNode& term){
		RetainN(term, 2);

		auto i(std::next(term.begin()));
		const auto& str(Unilang::ResolveRegular<const string>(*i));
		const auto& r(Unilang::ResolveRegular<const std::regex>(*++i));

		term.Value = std::regex_match(str, r);
		return ReductionStatus::Clean;
	});
	RegisterStrict(renv, "regex-replace", [](TermNode& term){
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
LoadModule_std_io(Interpreter& intp)
{
	using namespace Forms;
	auto& renv(intp.Root.GetRecordRef());

	RegisterStrict(renv, "newline", [&](TermNode& term){
		RetainN(term, 0);
		std::cout << std::endl;
		return ReduceReturnUnspecified(term);
	});
	RegisterUnary<Strict, const string>(renv, "readable-file?",
		[](const string& str) noexcept{
		return YSLib::ufexists(str.c_str());
	});
	RegisterStrict(renv, "load", [&](TermNode& term, Context& ctx){
		RetainN(term);
		ctx.SetupFront([&]{
			term = intp.ReadFrom(*intp.OpenUnique(std::move(
				Unilang::ResolveRegular<string>(*std::next(term.begin())))),
				ctx);
			return ReduceOnce(term, ctx);
		});
	});
	RegisterStrict(renv, "display", [&](TermNode& term){
		RetainN(term);
		LiftOther(term, *std::next(term.begin()));
		PrintTermNode(std::cout, term);
		return ReduceReturnUnspecified(term);
	});
}

void
LoadFunctions(Interpreter& intp, bool jit)
{
	using namespace Forms;
	using namespace std::placeholders;
	auto& ctx(intp.Root);
	auto& env(ctx.GetRecordRef());

	if(jit)
		SetupJIT(ctx);
	env.Bindings["ignore"].Value = TokenValue("#ignore");
	RegisterStrict(ctx, "eq?", Eq);
	RegisterStrict(ctx, "eqv?", EqValue);
	RegisterForm(ctx, "$if", If);
	RegisterUnary<>(ctx, "null?", ComposeReferencedTermOp(IsEmpty));
	RegisterUnary<>(ctx, "bound-lvalue?", IsBoundLValueTerm);
	RegisterUnary<>(ctx, "uncollapsed?", IsUncollapsedTerm);
	RegisterStrict(ctx, "as-const",
		ystdex::bind1(Qualify, TermTags::Nonmodifying));
	RegisterStrict(ctx, "expire",
		ystdex::bind1(Qualify, TermTags::Unique));
	RegisterStrict(ctx, "move!",
		std::bind(DoMoveOrTransfer, std::ref(LiftOtherOrCopy), _1));
	RegisterStrict(ctx, "cons", Cons);
	RegisterStrict(ctx, "cons%", ConsRef);
	RegisterUnary<Strict, const TokenValue>(ctx, "desigil", [](TokenValue s){
		return TokenValue(!s.empty() && (s.front() == '&' || s.front() == '%')
			? s.substr(1) : std::move(s));
	});
	RegisterStrict(ctx, "eval", Eval);
	RegisterStrict(ctx, "eval%", EvalRef);
	RegisterForm(ctx, "$resolve-identifier",
		std::bind(DoResolve, std::ref(ResolveIdentifier), _1, _2));
	RegisterUnary<Strict, const EnvironmentReference>(ctx, "lock-environment",
		[](const EnvironmentReference& wenv) noexcept{
		return wenv.Lock();
	});
	RegisterForm(ctx, "$move-resolved!",
		std::bind(DoResolve, std::ref(MoveResolved), _1, _2));
	RegisterStrict(ctx, "make-environment", MakeEnvironment);
	RegisterForm(ctx, "$def!", Define);
	RegisterForm(ctx, "$vau/e", VauWithEnvironment);
	RegisterForm(ctx, "$vau/e%", VauWithEnvironmentRef);
	RegisterStrict(ctx, "wrap", Wrap);
	RegisterStrict(ctx, "unwrap", Unwrap);
	RegisterUnary<Strict, const string>(ctx, "raise-error",
		[] (const string& str){
		throw UnilangException(str.c_str());
	});
	RegisterUnary<Strict, const string>(ctx, "raise-invalid-syntax-error",
		[](const string& str){
		throw InvalidSyntax(str.c_str());
	});
	RegisterStrict(ctx, "make-encapsulation-type", MakeEncapsulationType);
	RegisterStrict(ctx, "get-current-environment", GetCurrentEnvironment);
	RegisterForm(ctx, "$vau", Vau);
	RegisterForm(ctx, "$vau%", VauRef);
	intp.Perform(R"Unilang(
$def! id wrap ($vau% (%x) #ignore $move-resolved! x);
$def! lock-current-environment (wrap ($vau () d lock-environment d));
$def! $lambda $vau (&formals .&body) d
	wrap (eval (cons $vau (cons formals (cons ignore (move! body)))) d);
$def! $lambda% $vau (&formals .&body) d
	wrap (eval (cons $vau% (cons formals (cons ignore (move! body)))) d);
	)Unilang");
	RegisterStrict(ctx, "list", ReduceBranchToListValue);
	RegisterStrict(ctx, "list%", ReduceBranchToList);
	intp.Perform(R"Unilang(
$def! $remote-eval $vau (&o &e) d eval o (eval e d);
$def! $set! $vau (&e &formals .&expr) d
	eval (list $def! formals (unwrap eval) expr d) (eval e d);
$def! $defv! $vau (&$f &formals &ef .&body) d
	eval (list $set! d $f $vau formals ef (move! body)) d;
$defv! $defv/e%! (&$f &e &formals &ef .&body) d
	eval (list $set! d $f $vau/e% e formals ef (move! body)) d;
$defv! $defl! (f formals .body) d
	eval (list $set! d f $lambda formals (move! body)) d;
$defv! $defl%! (&f &formals .&body) d
	eval (list $set! d f $lambda% formals (move! body)) d;
$def! make-standard-environment $lambda () () lock-current-environment;
$def! $lvalue-identifier? $vau (&s) d
	eval (list bound-lvalue? (list $resolve-identifier s)) d;
$defl%! forward! (%x) $if ($lvalue-identifier? x) x (move! x);
	)Unilang");
	RegisterForm(ctx, "$sequence", Sequence);
	intp.Perform(R"Unilang(
$defl%! apply (&appv &arg .&opt)
	eval% (cons% () (cons% (unwrap (forward! appv)) (forward! arg)))
		($if (null? opt) (() make-environment)
			(($lambda ((&e .&eopt))
				$if (null? eopt) e
					(raise-invalid-syntax-error
						"Syntax error in applying form.")) opt));
$defl! list* (&head .&tail)
	$if (null? tail) (forward! head)
		(cons (forward! head) (apply list* (move! tail)));
$defl! list*% (&head .&tail)
	$if (null? tail) (forward! head)
		(cons% (forward! head) (apply list*% (move! tail)));
$defv! $lambda/e (&e &formals .&body) d
	wrap (eval (list* $vau/e e formals ignore (move! body)) d);
$defv! $defl/e! (&f &e &formals .&body) d
	eval (list $set! d f $lambda/e e formals (move! body)) d;
$defv! $defw%! (&f &formals &ef .&body) d
	eval (list $set! d f wrap (list* $vau% formals ef (move! body))) d;
$defw%! forward-first% (&appv (&x .)) d
	apply (forward! appv) (list% ($move-resolved! x)) d;
$defl%! first (%l)
	($lambda% (fwd) forward-first% forward! (fwd l))
		($if ($lvalue-identifier? l) id expire);
$defl%! first% (%l)
	($lambda (fwd (@x .)) fwd x) ($if ($lvalue-identifier? l) id expire) l;
$defl! rest ((#ignore .xs)) xs;
$defl! rest% ((#ignore .%xs)) move! xs;
$defv! $defv%! (&$f &formals &ef .&body) d
	eval (list $set! d $f $vau% formals ef (move! body)) d;
$defv! $defw! (&f &formals &ef .&body) d
	eval (list $set! d f wrap (list* $vau formals ef (move! body))) d;
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
$defv! $and? x d $cond
	((null? x) #t)
	((null? (rest x)) eval (first x) d)
	((eval (first x) d) apply (wrap $and?) (rest x) d)
	(#t #f);
$defv! $or? x d $cond
	((null? x) #f)
	((null? (rest x)) eval (first x) d)
	(#t ($lambda (r) $if r r
		(apply (wrap $or?) (rest x) d)) (eval (move! (first x)) d));
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
$defw! derive-current-environment (.&envs) d
	apply make-environment (append envs (list d)) d;
$def! ($let $let* $letrec) ($lambda (&ce)
(
	$def! mods () ($lambda/e ce ()
	(
		$def! idv $lambda% (x) $move-resolved! x;
		$defl%! rulist (&l)
			$if ($lvalue-identifier? l)
				(accr (($lambda ((.@xs)) xs) l) null? ()
					($lambda% (%l) $sequence
						($def! %x idv (($lambda% ((@x .)) x) l))
						(($if (uncollapsed? x) idv expire) (expire x))) rest%
					($lambda (%x &xs)
						(cons% ($resolve-identifier x) (move! xs))))
				(idv (forward! l));
		$defl%! list-extract-first (&l) map1 first l;
		$defl%! list-extract-rest% (&l) map1 rest% l;
		$defv%! $lqual (&ls) d
			($if (eval (list $lvalue-identifier? ls) d) as-const rulist)
				(eval% ls d);
		$defv%! $lqual* (&x) d
			($if (eval (list $lvalue-identifier? x) d) as-const expire)
				(eval% x d);
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
	$defv/e%! $let* mods (&bindings .&body) d
		eval% (mk-let* $let $let* ($lqual* bindings) (move! body)) d;
	$defv/e%! $letrec mods (&bindings .&body) d
		eval% (mk-letrec $let ($lqual bindings) (move! body)) d;
	map1 move! (list% $let $let* $letrec)
)) (() get-current-environment);
$defv! $as-environment (.&body) d
	eval (list $let () (list $sequence (move! body)
		(list () lock-current-environment))) d;
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
	// NOTE: Arithmetics.
	// TODO: Use generic types.
	RegisterBinary<Strict, const Number, const Number>(ctx, "<?",
		ystdex::less<>());
	RegisterBinary<Strict, const Number, const Number>(ctx, "<=?",
		ystdex::less_equal<>());
	RegisterBinary<Strict, const Number, const Number>(ctx, ">=?",
		ystdex::greater_equal<>());
	RegisterBinary<Strict, const Number, const Number>(ctx, ">?",
		ystdex::greater<>());
	RegisterStrict(ctx, "+", std::bind(CallBinaryFold<Number, ystdex::plus<>>,
		ystdex::plus<>(), 0, _1));
	RegisterBinary<Strict, const Number, const Number>(ctx, "add2",
		ystdex::plus<>());
	RegisterBinary<Strict, const Number, const Number>(ctx, "-",
		ystdex::minus<>());
	RegisterStrict(ctx, "*", std::bind(CallBinaryFold<Number,
		ystdex::multiplies<>>, ystdex::multiplies<>(), 1, _1));
	RegisterBinary<Strict, const Number, const Number>(ctx, "multiply2",
		ystdex::multiplies<>());
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

	load_std_module("strings", LoadModule_std_strings);
	load_std_module("io", LoadModule_std_io);
	// NOTE: FFI and external libraries support.
	InitializeFFI(intp);
	// NOTE: Prevent the ground environment from modification.
	env.Frozen = true;
	intp.SaveGround();
	// NOTE: User environment initialization.
	intp.Perform(R"Unilang(
$import! std.io newline load display;
	)Unilang");
}

#define APP_NAME "Unilang demo"
#define APP_VER "0.7.42"
#define APP_PLATFORM "[C++11] + YSLib"
constexpr auto
	title(APP_NAME " " APP_VER " @ (" __DATE__ ", " __TIME__ ") " APP_PLATFORM);

} // unnamed namespace;

} // namespace Unilang;

int
main(int argc, char* argv[])
{
	using namespace Unilang;
	using namespace std;
	using YSLib::LoggedEvent;

	return YSLib::FilterExceptions([&]{
		if(argc > 1)
		{
			if(std::strcmp(argv[1], "-e") == 0)
			{
				if(argc == 3)
				{
					Interpreter intp{};

					LoadFunctions(intp, Unilang_UseJIT);
					llvm_main();
					intp.RunLine(argv[2]);
				}
				else
					throw LoggedEvent("Option '-e' expect exact one argument.");
			}
			else
				throw LoggedEvent("Too many arguments.");
		}
		else if(argc == 1)
		{
			Interpreter intp{};

			cout << title << endl << "Initializing the interpreter "
				<< (Unilang_UseJIT ? "[JIT enabled]" : "[JIT disabled]")
				<< " ..." << endl;
			LoadFunctions(intp, Unilang_UseJIT);
			llvm_main();
			cout << "Initialization finished." << endl;
			cout << "Type \"exit\" to exit." << endl << endl;
			intp.Run();
		}
	}, yfsig, YSLib::Alert) ? EXIT_FAILURE : EXIT_SUCCESS;
}

