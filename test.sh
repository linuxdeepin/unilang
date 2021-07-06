#!/usr/bin/env bash
# shellcheck disable=2016

set -e

OUT=/tmp/out
ERR=/tmp/err

touch "$OUT"
touch "$ERR"

: "${UNILANG:=./unilang}"

if [[ -d "$UNILANG" || ! -x "$UNILANG" ]]; then
	echo "ERROR: Wrong '$UNILANG' found as the interpreter."
	exit 1
fi

call_intp()
{
	set +e
	echo > "$ERR"
#	echo "$1" | "$UNILANG" 1> "$OUT" 2> "$ERR"
	"$UNILANG" -e "$1" 1> "$OUT" 2> "$ERR"
	set -e
}

# NOTE: Test cases should print no errors.
run_case()
{
	echo "Running case:" "$1"
	if ! (call_intp "$1") || [ -s "$ERR" ]; then
		echo "FAIL."
		echo "Error:"
		cat "$ERR"
	else
		echo "PASS."
	fi
}

if [[ "$PTC" != '' ]]; then
# NOTE: Test cases should print no errors.
	echo "The following case are expected to be non-terminating."
	echo "However, the maximum memory consumption is expected constant."
	echo "Please exit manually by SIGINT."
	run_case '$defl! f (n) $if #t (f n); f 1'
fi

# Sanity.
run_case 'display'

# Documented examples.
run_case 'display "Hello, world!"'
run_case 'display (display "Hello, world!")'
run_case '(display "Hello, world!")'
run_case '() newline'
run_case '() display "Hello, world!"'
run_case 'display "Hello, world!";'
run_case '$sequence display "Hello, world!"'
run_case 'display "Hello, "; display "world!"'
run_case '$sequence (display "Hello, ") (display "world!")'
run_case '$def! x "hello"'
run_case 'list "hello" "world"'
run_case 'cons "x" ()'
run_case 'list "x"'
run_case 'cons "x" (cons "y" ())'
run_case 'list "x" "y"'
run_case '$lambda (x) display x'
run_case '$lambda (x) x'
run_case '($lambda (x) display x)'
run_case '($lambda (x) (display x))'
run_case '$lambda (x y) $sequence (display x) (display y)'
run_case '$def! id $lambda (x) x;'
run_case 'display (($lambda ((x y)) x) (list "hello" "world"))'
run_case '$def! car $lambda ((x .)) x; $def! cdr $lambda ((#ignore .x)) x;'
run_case 'eval (list display "Hello, world!") (() get-current-environment)'
run_case '$def! (x y) list "hello" "world"; display x; display y;'
run_case '$def! id $lambda (x) x;'
run_case '$defl! id (x) x;'
run_case \
'$def! x (); display "x is "; display ($if (null? x) "empty" "not empty");'
# NOTE: Test case on parent environment search.
run_case "\$def! e make-environment (() get-current-environment); \
eval ((unwrap (\$lambda (x) x)) e) e"
# NOTE: Test case on std.strings.
run_case '$import! std.strings string-empty?; display (string-empty? "")'
run_case '$import! std.strings string-empty?; display (string-empty? "x")'
run_case '$import! std.strings ++; display (eqv? (++ "a" "bc" "123") "abc123")'

