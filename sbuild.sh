#!/usr/bin/env bash
# Requires: YSLib sysroot >= b904, having bin in $PATH.
# Usage: sbuild.sh CONF [SHBOPT_BASE ...].
# See https://frankhb.github.io/YSLib-book/Tools/Scripts.zh-CN.html.

set -e
Unilang_BaseDir="$(cd "$(dirname "${BASH_SOURCE[0]}")"; pwd)"

[[ "$1" != '' ]] || (echo \
	"ERROR: The configuration name should not be empty." >& 2; exit 1)

CXXFLAGS_Qt="$(pkg-config --cflags Qt5Widgets)"
LIBS_Qt="$(pkg-config --libs Qt5Widgets)"

case $(uname) in
*MSYS* | *MINGW*)
	SHBuild_LIBS="$LIBS_Qt -lffi"
	;;
*)
	SHBuild_LIBS="$LIBS_Qt -lffi -ldl"
esac
mkdir -p "$Unilang_BaseDir/build"
(cd "$Unilang_BaseDir/build" \
	&& SHBuild_NoAdjustSubsystem=true SHBuild_LIBS="$SHBuild_LIBS" \
	SHBuild-BuildPkg.sh "$@" -xn,unilang "$Unilang_BaseDir/src" \
	-I\""$Unilang_BaseDir/include\"" "$CXXFLAGS_Qt")

echo "Done."

