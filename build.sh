#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2020-2023 UnionTech Software Technology Co.,Ltd.

set -e
Unilang_BaseDir="$(cd "$(dirname "${BASH_SOURCE[0]}")"; pwd)"
YSLib_BaseDir="$Unilang_BaseDir/3rdparty/YSLib"
: "${Unilang_Output=unilang}"
: "${Unilang_BuildDir="$(dirname "$Unilang_Output")"}"

echo 'Configuring ...'

echo "Output path is \"$Unilang_Output\"."
echo "Build directory is \"$Unilang_BuildDir\"."

: "${CXX:=g++}"
: "${CXXFLAGS=-std=c++11 -Wall -Wextra -g}"

. "$Unilang_BaseDir/detect-llvm.sh"

CXXFLAGS_Qt="$(pkg-config --cflags Qt5Widgets Qt5Quick)"
LIBS_Qt="$(pkg-config --libs Qt5Widgets Qt5Quick)"
# NOTE: These are known redundant or false positives.
if echo "$CXX" --version | grep -q clang; then
	CXXFLAGS_WKRD_='-Wno-ignored-attributes -Wno-mismatched-tags'
else
	CXXFLAGS_WKRD_=\
'-Wno-dangling-reference -Wno-ignored-attributes -Wno-uninitialized'
fi

INCLUDES=(-Iinclude "-I$YSLib_BaseDir/YBase/include" \
"-I$YSLib_BaseDir/YFramework/include")

SRCS=("$YSLib_BaseDir/YBase/source/ystdex/any.cpp" \
"$YSLib_BaseDir/YBase/source/ystdex/cassert.cpp" \
"$YSLib_BaseDir/YBase/source/ystdex/concurrency.cpp" \
"$YSLib_BaseDir/YBase/source/ystdex/cstdio.cpp" \
"$YSLib_BaseDir/YBase/source/ystdex/exception.cpp" \
"$YSLib_BaseDir/YBase/source/ystdex/memory_resource.cpp" \
"$YSLib_BaseDir/YBase/source/ystdex/node_base.cpp" \
"$YSLib_BaseDir/YBase/source/ystdex/tree.cpp" \
"$YSLib_BaseDir/YBase/source/ystdex/hash_table.cpp" \
"$YSLib_BaseDir/YFramework/source/CHRLib/CharacterProcessing.cpp" \
"$YSLib_BaseDir/YFramework/source/CHRLib/MappingEx.cpp" \
"$YSLib_BaseDir/YFramework/source/CHRLib/chrmap.cpp" \
"$YSLib_BaseDir/YFramework/source/YCLib/Debug.cpp" \
"$YSLib_BaseDir/YFramework/source/YCLib/FileIO.cpp" \
"$YSLib_BaseDir/YFramework/source/YCLib/FileSystem.cpp" \
"$YSLib_BaseDir/YFramework/source/YCLib/MemoryMapping.cpp" \
"$YSLib_BaseDir/YFramework/source/YCLib/NativeAPI.cpp" \
"$YSLib_BaseDir/YFramework/source/YCLib/YCommon.cpp" \
"$YSLib_BaseDir/YFramework/source/YSLib/Core/YCoreUtilities.cpp" \
"$YSLib_BaseDir/YFramework/source/YSLib/Core/YException.cpp" \
"$YSLib_BaseDir/YFramework/source/YSLib/Core/YObject.cpp" \
"$YSLib_BaseDir/YFramework/source/YSLib/Service/FileSystem.cpp" \
"$YSLib_BaseDir/YFramework/source/YSLib/Service/File.cpp" \
"$YSLib_BaseDir/YFramework/source/YSLib/Service/TextFile.cpp")

case $(uname) in
*MSYS* | *MINGW*)
	INCLUDES=("${INCLUDES[@]}" "-I$YSLib_BaseDir/YFramework/Win32/include")
	SRCS=("${SRCS[@]}" "$YSLib_BaseDir/YFramework/source/YCLib/Host.cpp" \
"$YSLib_BaseDir/YFramework/Win32/source/YCLib/MinGW32.cpp" \
"$YSLib_BaseDir/YFramework/Win32/source/YCLib/NLS.cpp" \
"$YSLib_BaseDir/YFramework/Win32/source/YCLib/Registry.cpp" \
"$YSLib_BaseDir/YFramework/Win32/source/YCLib/Consoles.cpp"
"$YSLib_BaseDir/YFramework/source/YSLib/Core/YConsole.cpp")
	CXXFLAGS_EXTRA="$CXXFLAGS_EXTRA -mthreads"
	LIB_EXTRA="$LIB_EXTRA -mthreads"
	;;
*)
	CXXFLAGS_EXTRA="$CXXFLAGS_EXTRA -fPIC -pthread"
	LIB_EXTRA="$LIB_EXTRA -pthread -ldl"
esac

SRCS=("$Unilang_BaseDir/src"/*.cpp "${SRCS[@]}")

echo 'Configuring Done.'

echo 'Building ...'

mkdir -p "$Unilang_BuildDir"
mkdir -p "$(dirname "$Unilang_Output")"

Call_()
{
	# XXX: %Verbose is external.
	# shellcheck disable=2154
	if test -n "$Verbose"; then
		# shellcheck disable=2086
		echo "${@@Q}"
	fi
	"$@"
}

# XXX: Assume GNU parallel, not parallel from moreutils, if '--version' is
#	supported.
# XXX: %NoParallel is external.
# shellcheck disable=2154
if test -z "$NoParallel" && hash parallel 2> /dev/null && hash grep 2> \
	/dev/null && parallel --will-cite --version > /dev/null 2> /dev/null; \
	then
	echo 'Using parallel.'
	if test -n "$Verbose"; then
		Verbose_t_=-t
	fi

	# XXX: Value of several variables may contain whitespaces.
	# shellcheck disable=2086
	parallel --will-cite -q $Verbose_t_ "$CXX" -c {} \
-o"$Unilang_BuildDir/{#}.o" $CXXFLAGS $CXXFLAGS_EXTRA $CXXFLAGS_WKRD_ \
$CXXFLAGS_Qt "${INCLUDES[@]}" ::: "${SRCS[@]}"

	OFILES_=("$Unilang_BuildDir"/*.o)

	# shellcheck disable=2086
	Call_ "$CXX" -o"$Unilang_Output" "${OFILES_[@]}" $LDFLAGS $LIBS $LIBS_Qt \
$LIBS_EXTRA
else
	echo 'Not using parallel.'

	# shellcheck disable=2086
	Call_ "$CXX" -o"$Unilang_Output" $CXXFLAGS $CXXFLAGS_EXTRA $CXXFLAGS_WKRD_ \
$CXXFLAGS_Qt $LDFLAGS $LIBS_Qt "${INCLUDES[@]}" "${SRCS[@]}" $LIBS_EXTRA
fi

echo 'Done.'

