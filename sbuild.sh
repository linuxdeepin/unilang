#!/usr/bin/env bash
# Requires: YSLib sysroot >= b904, having bin in $PATH.
# Usage: sbuild.sh CONF [SHBOPT_BASE ...].
# See https://frankhb.github.io/YSLib-book/Tools/Scripts.zh-CN.html.

set -e
Unilang_BaseDir="$(cd "$(dirname "${BASH_SOURCE[0]}")"; pwd)"

[[ "$1" != '' ]] || (echo \
	"ERROR: The configuration name should not be empty." >& 2; exit 1)

. "$Unilang_BaseDir/detect-llvm.sh"

CXXFLAGS_Qt="$(pkg-config --cflags Qt5Widgets Qt5Quick)"
LIBS_Qt="$(pkg-config --libs Qt5Widgets Qt5Quick)"

case $(uname) in
*MINGW64*)
# XXX: Workaround for MinGW64 G++ + ld with ASLR enabled by default. Clang++
#	+ ld or G++ + lld do need the flag, but leaving it here as-is.
#	See https://www.msys2.org/news/#2021-01-31-aslr-enabled-by-default,
#	https://github.com/msys2/MINGW-packages/issues/6986,
#	https://github.com/msys2/MINGW-packages/issues/7023,
#	and https://sourceware.org/bugzilla/show_bug.cgi?id=26659.
	LDFLAGS_LOWBASE_=-Wl,--default-image-base-low
	;;
esac
case $(uname) in
*MSYS* | *MINGW*)
	LIBS_EXTRA="$LIBS_Qt $LIBS_EXTRA"
	;;
*)
	LIBS_EXTRA="$LIBS_Qt $LIBS_EXTRA -ldl"
esac
mkdir -p "$Unilang_BaseDir/build"
(cd "$Unilang_BaseDir/build" && SHBuild_NoAdjustSubsystem=true \
	SHBuild_CXXFLAGS="$CXXFLAGS_EXTRA" SHBuild_LDFLAGS="$LDFLAGS_LOWBASE_" \
	SHBuild_LIBS="$LIBS_EXTRA" SHBuild-BuildPkg.sh "$@" \
	-xn,unilang "$Unilang_BaseDir/src" -I\""$Unilang_BaseDir/include"\" \
	"$CXXFLAGS_Qt")

echo "Done."

