// © 2020-2021 Uniontech Software Technology Co.,Ltd.

#include "Interpreter.h" // for Interpreter, ValueObject;
#include "Context.h" // for EnvironmentSwitcher,
//	Unilang::SwitchToFreshEnvironment;
#include <ystdex/scope_guard.hpp> // for ystdex::guard;
#include <ystdex/invoke.hpp> // for ystdex::invoke;
#include <functional> // for std::bind, std::placeholders;
#include "Forms.h" // for Forms::RetainN, CallBinaryFold;
#include "Exception.h" // for ThrowNonmodifiableErrorForAssignee;
#include "BasicReduction.h" // for ReductionStatus;
#include "TermAccess.h" // for ResolvedTermReferencePtr, Unilang::ResolveTerm,
//	ComposeReferencedTermOp, IsBoundLValueTerm, EnvironmentReference, TermNode,
//	IsBranchedList;
#include <iterator> // for std::next, std::iterator_traits;
#include "Evaluation.h" // for CheckSymbol, RegisterStrict,
//	ThrowInsufficientTermsError;
#include <ystdex/functor.hpp> // for ystdex::plus, ystdex::equal_to,
//	ystdex::less, ystdex::less_equal, ystdex::greater, ystdex::greater_equal;
#include <regex> // for std::regex, std::regex_match;
#include <iostream> // for std::cout, std::endl;
#include <random> // for std::random_device, std::mt19937,
//	std::uniform_int_distribution;
#include <cstdlib> // for std::exit;
#include "UnilangFFI.h"

namespace Unilang
{

namespace
{

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

[[nodiscard]] ReductionStatus
DoMoveOrTransfer(void(&f)(TermNode&, TermNode&, bool), TermNode& term)
{
	Forms::RetainN(term);
	Unilang::ResolveTerm([&](TermNode& nd, ResolvedTermReferencePtr p_ref){
		f(term, nd, !p_ref || p_ref->IsModifiable());
	}, *std::next(term.begin()));
	return ReductionStatus::Retained;
}

[[nodiscard]] ReductionStatus
DoResolve(TermNode(&f)(const Context&, string_view), TermNode& term,
	const Context& c)
{
	Forms::RetainN(term);
	Unilang::ResolveTerm([&](TermNode& nd, ResolvedTermReferencePtr p_ref){
		const auto& id(Unilang::AccessRegular<string_view>(nd, p_ref));

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
LoadModule_std_strings(Context& ctx)
{
	using namespace Forms;
	auto& renv(ctx.GetRecordRef());

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
}

void
LoadFunctions(Interpreter& intp)
{
	using namespace Forms;
	using namespace std::placeholders;
	auto& ctx(intp.Root);

	ctx.GetRecordRef().Bindings["ignore"].Value = TokenValue("#ignore");
	RegisterStrict(ctx, "eq?", Eq);
	RegisterStrict(ctx, "eqv?", EqValue);
	RegisterForm(ctx, "$if", If);
	RegisterUnary<>(ctx, "null?", ComposeReferencedTermOp(IsEmpty));
	RegisterUnary<>(ctx, "bound-lvalue?", IsBoundLValueTerm);
	RegisterStrict(ctx, "move!",
		std::bind(DoMoveOrTransfer, std::ref(LiftOtherOrCopy), _1));
	RegisterStrict(ctx, "cons", Cons);
	RegisterStrict(ctx, "eval", Eval);
	RegisterStrict(ctx, "eval%", EvalRef);
	RegisterForm(ctx, "$resolve-identifier",
		std::bind(DoResolve, std::ref(ResolveIdentifier), _1, _2));
	RegisterUnary<Strict, const EnvironmentReference>(ctx, "lock-environment",
		[](const EnvironmentReference& wenv) noexcept{
		return wenv.Lock();
	});
	RegisterStrict(ctx, "make-environment", MakeEnvironment);
	RegisterForm(ctx, "$def!", Define);
	RegisterForm(ctx, "$vau/e", VauWithEnvironment);
	RegisterForm(ctx, "$vau/e%", VauWithEnvironmentRef);
	RegisterStrict(ctx, "wrap", Wrap);
	RegisterStrict(ctx, "unwrap", Unwrap);
	RegisterUnary<Strict, const string>(ctx, "raise-invalid-syntax-error",
		[](const string& str){
		throw InvalidSyntax(str.c_str());
	});
	RegisterStrict(ctx, "make-encapsulation-type", MakeEncapsulationType);
	RegisterStrict(ctx, "get-current-environment", GetCurrentEnvironment);
	intp.Perform(R"Unilang(
		$def! $vau $vau/e (() get-current-environment) (&formals &ef .&body) d
			eval (cons $vau/e (cons d (cons formals (cons ef (move! body))))) d;
		$def! $vau% $vau (&formals &ef .&body) d
			eval (cons $vau/e% (cons d (cons formals (cons ef (move! body)))))
				d;
		$def! lock-current-environment (wrap ($vau () d lock-environment d));
		$def! $lambda $vau (&formals .&body) d wrap
			(eval (cons $vau (cons formals (cons ignore (move! body)))) d);
		$def! $lambda% $vau (&formals .&body) d wrap
			(eval (cons $vau% (cons formals (cons ignore (move! body)))) d);
	)Unilang");
	RegisterStrict(ctx, "list", ReduceBranchToListValue);
	RegisterStrict(ctx, "list%", ReduceBranchToList);
	intp.Perform(R"Unilang(
		$def! $set! $vau (&e &formals .&expr) d
			eval (list $def! formals (unwrap eval) expr d) (eval e d);
		$def! $defv! $vau (&$f &formals &ef .&body) d
			eval (list $set! d $f $vau formals ef (move! body)) d;
		$defv! $defl! (f formals .body) d
			eval (list $set! d f $lambda formals (move! body)) d;
		$defv! $defl%! (&f &formals .&body) d
			eval (list $set! d f $lambda% formals (move! body)) d;
		$defv! $lambda/e (&e &formals .&body) d
			wrap (eval (list* $vau/e e formals ignore (move! body)) d);
		$defv! $defl/e! (&f &e &formals .&body) d
			eval (list $set! d f $lambda/e e formals (move! body)) d;
		$def! make-standard-environment
			$lambda () () lock-current-environment;
	)Unilang");
	RegisterForm(ctx, "$sequence", Sequence);
	intp.Perform(R"Unilang(
		$defl%! forward! (%x)
			$if (bound-lvalue? ($resolve-identifier x)) x (move! x);
		$defl! apply (&appv &arg .&opt)
			eval (cons () (cons (unwrap appv) arg))
				($if (null? opt) (() make-environment)
					(($lambda ((&e .&eopt))
						$if (null? eopt) e
							(raise-invalid-syntax-error
								"Syntax error in applying form.")) opt));
		$defl! list* (&head .&tail)
			$if (null? tail) head (cons head (apply list* tail));
		$defl! first ((&x .)) x;
		$defl! rest ((#ignore .x)) x;
		$defv! $defv%! (&$f &formals &ef .&body) d
			eval (list $set! d $f $vau% formals ef (move! body)) d;
		$defv! $defw! (&f &formals &ef .&body) d
			eval (list $set! d f wrap (list* $vau formals ef (move! body))) d;
		$defv! $cond &clauses d
			$if (null? clauses) #inert
				(apply ($lambda ((&test .&body) .&clauses)
					$if (eval test d) (eval (move! body) d)
						(apply (wrap $cond) (move! clauses) d)) clauses);
		$defv! $when (&test .&exprseq) d
			$if (eval test d) (eval (list* () $sequence exprseq) d);
		$defv! $unless (&test .&exprseq) d
			$if (eval test d) #inert (eval (list* () $sequence exprseq) d);
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
		$defw! accr (&l &pred? &base &head &tail &sum) d
			$if (apply pred? (list l) d) base
				(apply sum (list (apply head (list l) d)
					(apply accr (list (apply tail (list l) d)
					pred? base head tail sum) d)) d);
		$defw! foldr1 (&kons &knil &l) d
			apply accr (list l null? knil first rest kons) d;
		$defw! map1 (&appv &l) d
			foldr1 ($lambda (&x &xs) cons (apply appv (list x) d) xs) () l;
		$defl! list-concat (&x &y) foldr1 cons y x;
		$defl! append (.&ls) foldr1 list-concat () (move! ls);
		$defl! filter (&accept? &ls) apply append
			(map1 ($lambda (&x) $if (apply accept? (list x)) (list x) ()) ls);
		$defw! derive-current-environment (.&envs) d
			apply make-environment (append envs (list d)) d;
		$defv! $as-environment (.&body) d
			eval (list $let () (list $sequence (move! body)
				(list () lock-current-environment))) d;
		$defv! $let (&bindings .&body) d
			eval (list* () (list* $lambda (map1 first bindings)
				(list (move! body)))
				(map1 ($lambda (x) list (rest x)) bindings)) d;
		$defv! $let/d (&bindings &ef .&body) d
			eval (list* () (list wrap (list* $vau (map1 first bindings)
				ef (list (move! body))))
				(map1 ($lambda (x) list (rest x)) bindings)) d;
		$defv! $let* (&bindings .&body) d
			eval ($if (null? bindings) (list* $let bindings (move! body))
				(list $let (list (first bindings))
				(list* $let* (rest bindings) (move! body)))) d;
		$defv! $letrec (&bindings .&body) d
			eval (list $let () $sequence (list $def! (map1 first bindings)
				(list* () list (map1 rest bindings))) (move! body)) d;
		$defv! $bindings/p->environment (&parents .&bindings) d $sequence
			($def! res apply make-environment (map1 ($lambda (x) eval x d)
				parents))
			(eval (list $set! res (map1 first bindings)
				(list* () list (map1 rest bindings))) d)
			res;
		$defv! $bindings->environment (.&bindings) d
			eval (list* $bindings/p->environment () bindings) d;
		$defv! $provide/let! (&symbols &bindings .&body) d
			$sequence (eval% (list% $def! symbols (list $let bindings $sequence
				(list% ($vau% (&e) d $set! e res (lock-environment d))
				(() get-current-environment)) (move! body)
				(list* () list symbols))) d) res;
		$defv! $provide! (&symbols .&body) d
			eval (list*% $provide/let! (forward! symbols) () (move! body)) d;
		$defv! $import! (&e .&symbols) d
			eval (list $set! d symbols (list* () list symbols)) (eval e d);
	)Unilang");
	// NOTE: Arithmetics.
	// TODO: Use generic types.
	RegisterBinary<Strict, const int, const int>(ctx, "<?", ystdex::less<>());
	RegisterBinary<Strict, const int, const int>(ctx, "<=?",
		ystdex::less_equal<>());
	RegisterBinary<Strict, const int, const int>(ctx, ">=?",
		ystdex::greater_equal<>());
	RegisterBinary<Strict, const int, const int>(ctx, ">?",
		ystdex::greater<>());
	// NOTE: The standard I/O library.
	RegisterStrict(ctx, "load", [&](TermNode& term, Context& c){
		RetainN(term);
		c.SetupFront([&]{
			term = intp.ReadFrom(*intp.OpenUnique(std::move(
				Unilang::ResolveRegular<string>(*std::next(term.begin())))),
				c);
			return ReduceOnce(term, c);
		});
	});
	RegisterStrict(ctx, "display", [&](TermNode& term){
		RetainN(term);
		LiftOther(term, *std::next(term.begin()));
		PrintTermNode(std::cout, term);
		return ReduceReturnUnspecified(term);
	});
	RegisterStrict(ctx, "newline", [&](TermNode& term){
		RetainN(term, 0);
		std::cout << std::endl;
		return ReduceReturnUnspecified(term);
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
					std::uniform_int_distribution<size_t>(0, nd.size()
					- 1)(mt))), Unilang::IsMovable(p_ref));
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
		void(&load_module)(Context& ctx)){
		LoadModuleChecked(rctx, "std." + string(module_name),
			std::bind(load_module, std::ref(ctx)));
	});

	load_std_module("strings", LoadModule_std_strings);
	// NOTE: FFI and external libraries support.
	InitializeFFI(intp);
	intp.SaveGround();
}

#define APP_NAME "Unilang demo"
#define APP_VER "0.6.22"
#define APP_PLATFORM "[C++11] + YSLib"
constexpr auto
	title(APP_NAME " " APP_VER " @ (" __DATE__ ", " __TIME__ ") " APP_PLATFORM);

} // unnamed namespace;

} // namespace Unilang;

int
main()
{
	using namespace Unilang;
	using namespace std;
	Interpreter intp{};

	cout << title << endl << "Initializing...";
	LoadFunctions(intp);
	cout << "Initialization finished." << endl;
	cout << "Type \"exit\" to exit." << endl << endl;
	intp.Run();
}

