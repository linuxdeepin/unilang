// © 2020 Uniontech Software Technology Co.,Ltd.

#include "Interpreter.h" // for Interpreter;
#include "Forms.h" // for Forms::RetainN;
#include "Evaluation.h" // for CheckSymbol, RegisterStrict,
//	ThrowInsufficientTermsError;
#include "BasicReduction.h" // for ReductionStatus;
#include "Forms.h" // for RetainN;
#include "TermAccess.h" // for Unilang::ResolveTerm, ComposeReferencedTermOp,
//	IsBoundLValueTerm, EnvironmentReference, TermNode, ResolveTerm,
//	IsBranchedList;
#include <iterator> // for std::next, std::iterator_traits;
#include <functional> // for std::bind;
#include <ystdex/functor.hpp> // for ystdex::less, ystdex::less_equal,
//	ystdex::greater, ystdex::greater_equal;
#include <iostream> // for std::cout, std::endl;
#include <random> // for std::random_device, std::mt19937,
//	std::uniform_int_distribution;
#include <cstdlib> // for std::exit;
#include "UnilangFFI.h"

namespace Unilang
{

namespace
{

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

void
LoadFunctions(Interpreter& intp)
{
	auto& ctx(intp.Root);
	using namespace Forms;
	using namespace std::placeholders;

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
		$def! std.strings make-environment ($as-environment ($provide/let! (
			string-empty?
		)
		((mods () get-current-environment))
		(
			$defl/e! string-empty? mods (&s) eqv? s "";
		)));
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
	// NOTE: FFI and external libraries support.
	InitializeFFI(intp);
	intp.SaveGround();
}

#define APP_NAME "Unilang demo"
#define APP_VER "0.6.9"
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

