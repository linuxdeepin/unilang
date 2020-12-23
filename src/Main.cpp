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
#include "UnilangFFI.h"
#include <iostream> // for std::cout, std::endl;
#include <random> // for std::random_device, std::mt19937,
//	std::uniform_int_distribution;
#include <cstdlib> // for std::exit;

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
	RegisterStrict(ctx, "eq?", Equal);
	RegisterStrict(ctx, "eqv?", EqualValue);
	RegisterForm(ctx, "$if", If);
	RegisterUnary<>(ctx, "null?", ComposeReferencedTermOp(IsEmpty));
	RegisterUnary<>(ctx, "bound-lvalue?", IsBoundLValueTerm);
	RegisterStrict(ctx, "move!",
		std::bind(DoMoveOrTransfer, std::ref(LiftOtherOrCopy), _1));
	RegisterStrict(ctx, "cons", Cons);
	RegisterStrict(ctx, "eval", Eval);
	RegisterForm(ctx, "$resolve-identifier",
		std::bind(DoResolve, std::ref(ResolveIdentifier), _1, _2));
	RegisterUnary<Strict, const EnvironmentReference>(ctx, "lock-environment",
		[](const EnvironmentReference& wenv) noexcept{
		return wenv.Lock();
	});
	RegisterStrict(ctx, "make-environment", MakeEnvironment);
	RegisterForm(ctx, "$def!", Define);
	RegisterForm(ctx, "$vau/e", VauWithEnvironment);
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
			eval (cons $vau/e (cons d (cons formals (cons ef body)))) d;
		$def! lock-current-environment (wrap ($vau () d lock-environment d));
		$def! $lambda $vau (&formals .&body) d wrap
			(eval (cons $vau (cons formals (cons ignore body))) d);
	)Unilang");
	RegisterStrict(ctx, "list", ReduceBranchToListValue);
	RegisterStrict(ctx, "list%", ReduceBranchToList);
	intp.Perform(R"Unilang(
		$def! $set! $vau (&e &formals .&expr) d
			eval (list $def! formals (unwrap eval) expr d) (eval e d);
		$def! $defv! $vau (&$f &formals &ef .&body) d
			eval (list $set! d $f $vau formals ef body) d;
		$defv! $defl! (f formals .body) d
			eval (list $set! d f $lambda formals body) d;
		$defv! $lambda/e (&e &formals .&body) d
			wrap (eval (list* $vau/e e formals ignore body) d);
		$def! make-standard-environment
			$lambda () () lock-current-environment;
	)Unilang");
	RegisterForm(ctx, "$sequence", Sequence);
	intp.Perform(R"Unilang(
		$defl! first ((&x .)) x;
		$defl! rest ((#ignore .x)) x;
		$defl! apply (&appv &arg .&opt)
			eval (cons () (cons (unwrap appv) arg))
				($if (null? opt) (() make-environment)
					(($lambda ((&e .&eopt))
						$if (null? eopt) e
							(raise-invalid-syntax-error
								"Syntax error in applying form.")) opt));
		$defl! list* (&head .&tail)
			$if (null? tail) head (cons head (apply list* tail));
		$defv! $defw! (&f &formals &ef .&body) d
			eval (list $set! d f wrap (list* $vau formals ef body)) d;
		$defv! $cond clauses d
			$if (null? clauses) #inert
				(apply ($lambda ((&test .&body) .&clauses)
					$if (eval test d) (eval body d)
						(apply (wrap $cond) clauses d)) clauses);
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
				(apply (wrap $or?) (rest x) d)) (eval (first x) d));
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
		$defl! append (.&ls) foldr1 list-concat () ls;
		$defv! $let (&bindings .&body) d
			eval (list* () (list* $lambda (map1 first bindings)
				(list body)) (map1 ($lambda (x) list (rest x)) bindings)) d;
		$defv! $let/d (&bindings &ef .&body) d
			eval (list* () (list wrap (list* $vau (map1 first bindings)
				ef (list body))) (map1 ($lambda (x) list (rest x)) bindings)) d;
		$defv! $let* (&bindings .&body) d
			eval ($if (null? bindings) (list* $let bindings body)
				(list $let (list (first bindings))
				(list* $let* (rest bindings) body))) d;
		$defv! $letrec (&bindings .&body) d
			eval (list $let () $sequence (list $def! (map1 first bindings)
				(list* () list (map1 rest bindings))) body) d;
		$defv! $bindings/p->environment (&parents .&bindings) d $sequence
			($def! res apply make-environment (map1 ($lambda (x) eval x d)
				parents))
			(eval (list $set! res (map1 first bindings)
				(list* () list (map1 rest bindings))) d)
			res;
		$defv! $bindings->environment (.&bindings) d
			eval (list* $bindings/p->environment
				(list (() make-standard-environment)) bindings) d;
		$defv! $provide! (&symbols .&body) d
			$sequence (eval (list $def! symbols (list $let () $sequence
				(list ($vau (e) d $set! e res (lock-environment d))
				(() get-current-environment)) body
				(list* () list symbols))) d) res;
		$defv! $provide/d! (&symbols &ef .body) d
			$sequence (eval (list $def! symbols (list $let/d () ef $sequence
				(list ($vau (e) d $set! e res (lock-environment d))
				(() get-current-environment)) body
				(list* () list symbols))) d) res;
		$defv! $import! (&e .&symbols) d
			eval (list $set! d symbols (list* () list symbols)) (eval e d);
	)Unilang");
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
	RegisterStrict(ctx, "load", [&](TermNode& term, Context& c){
		RetainN(term);
		c.SetupFront([&]{
			term = intp.ReadFrom(*intp.OpenUnique(std::move(
				Unilang::ResolveRegular<string>(*std::next(term.begin())))),
				c);
			return ReduceOnce(term, c);
		});
		return ReductionStatus::Clean;
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
	RegisterUnary<Strict, const int>(ctx, "sys.exit", [&](int status){
		std::exit(status);
	});
	// NOTE: FFI and external libraries support.
	InitializeFFI(intp);
	intp.SaveGround();
}

#define APP_NAME "Unilang demo"
#define APP_VER "0.5.23"
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

