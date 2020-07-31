#!/usr/bin/env sh

set -e

OUT=/tmp/out
ERR=/tmp/err

touch "$OUT"
touch "$ERR"

run()
{
	echo > "$ERR"
	echo "$1" | ./unilang 1> "$OUT" 2> "$ERR"
}

# NOTE: Test cases should print no errors.
test()
{
	echo "Running case:" "$1"
	run "$1"
	if [ -s "$ERR" ]; then
		echo "FAIL."
		echo "Error:"
		cat "$ERR"
	else
		echo "PASS."
	fi
}

# Sanity.
test 'display'

# Documented examples.
test 'display "Hello, world!"'
test 'display (display "Hello, world!")'
test '(display "Hello, world!")'
test '() newline'
test '() display "Hello, world!"'
test 'display "Hello, world!";'
test '$sequence display "Hello, world!"'
test 'display "Hello, "; display "world!"'
test '$sequence (display "Hello, ") (display "world!")'
test '$def! x "hello"'
test 'list "hello" "world"'
test 'cons "x" ()'
test 'list "x"'
test 'cons "x" (cons "y" ())'
test 'list "x" "y"'

