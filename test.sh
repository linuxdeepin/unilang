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
run_case 'load "test.txt"'

