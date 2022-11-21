#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2022 UnionTech Software Technology Co.,Ltd.
# Requires: valgrind.
# Test script for running the interpreter in release mode using valgrind.
# The default tool is callgrind.

: "${CONF:="release"}"
: "${OUTPUT:="build/.$CONF/callgrind.out"}"
: "${LOGOPT:="--log-file=build/.$CONF/callgrind.log"}"
: "${TOOLOPT:="--tool=callgrind --callgrind-out-file=$OUTPUT --dump-instr=yes \
--collect-jumps=yes"}"
: "${UNILANG:="build/.$CONF/unilang.exe"}"

echo 'Ready to run the command for Unilang interpreter "'"$UNILANG"'".'
echo 'Using valgrind options: '$LOGOPT $TOOLOPT'.'
echo 'Using interpreter options: '"$@"'.'

# XXX: Value of several variables may contain whitespaces.
# shellcheck disable=2086
valgrind $LOGOPT $TOOLOPT --fullpath-after= "$UNILANG" "$@"

echo 'Output file is placed in "'"$OUTPUT"'".'
echo 'Done.'

