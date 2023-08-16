#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2021-2023 UnionTech Software Technology Co.,Ltd.
# Requires: LLVM 7.0 installed in some known location.
# Usage: ./detect-llvm.sh, or sourced.
# shellcheck disable=2016

set -e

# XXX: Currently we stick to LLVM 7.

if test -z "$UNILANG_NO_LLVM"; then

test_llvm7_prefix()
{
	for pfx in "$USE_LLVM_PREFIX" '/usr/lib/llvm-7' '/opt/llvm70' \
		'/opt/llvm70' '/usr/local' '/usr'; do
		if test -x "$pfx/bin/llvm-config"; then
			if [[ $("$pfx/bin/llvm-config" --version) =~ 7\..+ ]]; then
				LLVM_PREFIX="$("$pfx/bin/llvm-config" --prefix)"
				LLVM_BINDIR="$("$pfx/bin/llvm-config" --bindir)"
				return
			fi
		fi
	done
}

test_llvm7_prefix
if test -d "$LLVM_PREFIX"; then
	echo 'Found LLVM 7 prefix:' "$LLVM_PREFIX"
else
	echo 'ERROR: Failed to find LLVM 7.'
	echo 'Please specify the installation prefix in $USE_LLVM_PREFIX.'
	echo 'Alternatively, consider disable LLVM by "UNILANG_NO_LLVM".'
	exit 1
fi
echo 'LLVM version:' "$("$LLVM_BINDIR/llvm-config" --version)"

CXXFLAGS_EXTRA="$("$LLVM_BINDIR/llvm-config" --cxxflags | sed s/-DNDEBUG//g) \
-fexceptions -frtti -Wp,-U_FORTIFY_SOURCE -U_FORTIFY_SOURCE -Wno-date-time"
if echo "$CXX" --version | grep -q clang; then
	echo 'Found $CXX: '"$CXX"', unsupported compiler options are removed.'
	CXXFLAGS_EXTRA="$(echo "$CXXFLAGS_EXTRA" \
| sed -E 's/-Wno-(class-memaccess|maybe-uninitialized)//g')"
fi
# NOTE: Loading local symbol in LLVM IR requires '--export-dynamic'.
LIBS_EXTRA="$("$LLVM_BINDIR/llvm-config" --ldflags) \
$("$LLVM_BINDIR/llvm-config" --libs) -lffi -Wl,--export-dynamic"

else

echo 'LLVM support is disabled.'
CXXFLAGS_EXTRA='-DUNILANG_NO_LLVM=1'
# XXX: Some other dependencies are still needed.
# shellcheck disable=2034
LIBS_EXTRA='-lffi'

fi

