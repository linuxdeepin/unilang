#!/usr/bin/env bash
# SPDX-FileCopyrightText: 2020-2022 UnionTech Software Technology Co.,Ltd.

set -e
Unilang_BaseDir="$(cd "$(dirname "${BASH_SOURCE[0]}")"; pwd)"
YSLib_BaseDir="$Unilang_BaseDir/3rdparty/YSLib"

: "${CXX:=g++}"
: "${CXXFLAGS=-std=c++11 -Wall -Wextra -g}"

. "$Unilang_BaseDir/detect-llvm.sh"

CXXFLAGS_Qt="$(pkg-config --cflags Qt5Widgets Qt5Quick)"
LIBS_Qt="$(pkg-config --libs Qt5Widgets Qt5Quick)"

echo 'Building ...'

case $(uname) in
*MSYS* | *MINGW*)
	EXTRA_OPT="-I$YSLib_BaseDir/YFramework/Win32/include \
$YSLib_BaseDir/YFramework/source/YCLib/Host.cpp \
$YSLib_BaseDir/YFramework/Win32/source/YCLib/MinGW32.cpp \
$YSLib_BaseDir/YFramework/Win32/source/YCLib/NLS.cpp \
$YSLib_BaseDir/YFramework/Win32/source/YCLib/Registry.cpp \
$YSLib_BaseDir/YFramework/Win32/source/YCLib/Consoles.cpp"
	;;
*)
	EXTRA_OPT="-fPIC -pthread \
$YSLib_BaseDir/YFramework/source/CHRLib/chrmap.cpp -ldl"
esac

# shellcheck disable=2086
"$CXX" $CXXFLAGS -ounilang $Unilang_BaseDir/src/*.cpp \
$CXXFLAGS_EXTRA \
-Iinclude -I$YSLib_BaseDir/YBase/include \
"$YSLib_BaseDir/YBase/source/ystdex/any.cpp" \
"$YSLib_BaseDir/YBase/source/ystdex/cassert.cpp" \
"$YSLib_BaseDir/YBase/source/ystdex/concurrency.cpp" \
"$YSLib_BaseDir/YBase/source/ystdex/cstdio.cpp" \
"$YSLib_BaseDir/YBase/source/ystdex/exception.cpp" \
"$YSLib_BaseDir/YBase/source/ystdex/memory_resource.cpp" \
"$YSLib_BaseDir/YBase/source/ystdex/node_base.cpp" \
"$YSLib_BaseDir/YBase/source/ystdex/tree.cpp" \
-I$YSLib_BaseDir/YFramework/include \
"$YSLib_BaseDir/YFramework/source/CHRLib/CharacterProcessing.cpp" \
"$YSLib_BaseDir/YFramework/source/CHRLib/MappingEx.cpp" \
"$YSLib_BaseDir/YFramework/source/YCLib/Debug.cpp" \
"$YSLib_BaseDir/YFramework/source/YCLib/FileIO.cpp" \
"$YSLib_BaseDir/YFramework/source/YCLib/FileSystem.cpp" \
"$YSLib_BaseDir/YFramework/source/YCLib/MemoryMapping.cpp" \
"$YSLib_BaseDir/YFramework/source/YCLib/NativeAPI.cpp" \
"$YSLib_BaseDir/YFramework/source/YCLib/YCommon.cpp" \
"$YSLib_BaseDir/YFramework/source/YSLib/Core/YCoreUtilities.cpp" \
"$YSLib_BaseDir/YFramework/source/YSLib/Core/YException.cpp" \
"$YSLib_BaseDir/YFramework/source/YSLib/Core/YObject.cpp" \
"$YSLib_BaseDir/YFramework/source/YSLib/Service/File.cpp" \
"$YSLib_BaseDir/YFramework/source/YSLib/Service/TextFile.cpp" \
$CXXFLAGS_Qt $LIBS_Qt $EXTRA_OPT $LIBS_EXTRA

echo 'Done.'

