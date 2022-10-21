#!/usr/bin/env bash
# Requires: wget, 7z.

set -e

Unilang_BaseDir="$(cd "$(dirname "${BASH_SOURCE[0]}")"; pwd)"
YSLib_BaseDir="$Unilang_BaseDir/3rdparty/YSLib"

case $(uname) in
*MSYS* | *MINGW*)
	echo 'Windows platforms are not supported by this installer yet.'
	exit 1
esac

# NOTE: This works around the missing execution bit in scripts cloned from YSLib
#	repository (originally in Mercurial which cannot set the bit portably).

find "$YSLib_BaseDir" -type f -name "*.sh" -exec chmod +x {} \;

# NOTE: Prepare archives for YSLib build.

LIB="$YSLib_BaseDir/YFramework/Linux/lib"

if [[ -d "$LIB" && -r "$LIB/libFreeImage.a" && -r "$LIB/libFreeImaged.a" ]]; \
then
	echo 'Archive files for YSLib are detected. Skip.'
else
	echo 'Archive files for YSLib are not ready. Try to download them...'

	if ! hash "wget" 2> /dev/null; then
		echo "Missing tool: wget. Install wget first."
		exit 1
	fi
	if ! hash "7z" 2> /dev/null; then
		echo "Missing tool: 7za. Install p7zip first."
		exit 1
	fi

	echo 'Getting archive files online ...'

	mkdir -p "$LIB"

	# XXX: Currently OSDN does not always successfully select the optimal
	#	mirror. Use hard-coded TUNA mirror to get better performance.
	URL_Lib_Archive=\
'https://osdn.net/frs/redir.php?m=tuna&f=yslib%2F73798%2FExternal-0.9-b916.7z'

	# XXX: Currently p7zip does not support '-si' for 7z archives.
	wget -O /tmp/External-0.9-b916.7z "$URL_Lib_Archive"
	7za x -y -bsp0 -bso0 /tmp/External-0.9-b916.7z -o"$LIB"

	echo 'Archive files prepared.'
fi

# NOTE: Patch files.

PATCHED_SIG="$Unilang_BaseDir/3rdparty/.patched"
if [[ -f "$PATCHED_SIG" ]]; then
	echo 'Patched source found. Skipped patching.'
else
	echo 'Ready to patch files.'

	if cd "$YSLib_BaseDir" && ! [[ -d .git ]] \
|| (git show --summary | grep 'build 904 ' > /dev/null); then
		# NOTE: Workaround for a bug with GCC < 9.2.
		#	See https://stackoverflow.com/a/58376059/2307646.
		sed -i 's/p_impl{}/p_impl/' \
"$YSLib_BaseDir/YFramework/include/YCLib/HostedGUI.h"
	fi
	# NOTE: Old GCC does not support LTO well. Disable it globally here.
	sed -i 's/-flto=jobserver//g' \
"$YSLib_BaseDir/Tools/Scripts/SHBuild-YSLib-common.txt"
	sed -i 's/-flto=auto//g' \
"$YSLib_BaseDir/Tools/Scripts/SHBuild-YSLib-common.txt"
	sed -i 's/-flto//g' \
"$YSLib_BaseDir/Tools/Scripts/SHBuild-YSLib-common.txt"
	# NOTE: Use debug library to work around the bogus LTO information in the
	#	release library.
	cp "$YSLib_BaseDir/YFramework/Linux/lib/libFreeImaged.a" \
"$YSLib_BaseDir/YFramework/Linux/lib/libFreeImage.a"
	
	touch "$PATCHED_SIG"
	echo 'Patched.'
fi

# NOTE: Build.

: "${SHBuild_BuildOpt:="-xj,$(nproc)"}"
: "${SHBuild_SysRoot:="$YSLib_BaseDir/sysroot"}"

echo "Use option: SHBuild_BuildOpt=$SHBuild_BuildOpt"
echo "Use option: SHBuild_SysRoot=$SHBuild_SysRoot"

SHBuild_BuildOpt="$SHBuild_BuildOpt" SHBuild_SysRoot="$SHBuild_SysRoot" \
	SHBuild_UseDebug=true SHBuild_UseRelease=true \
	SHBuild_NoDev=true "$YSLib_BaseDir/Tools/install-sysroot.sh"

echo 'To make the build environment work, ensure environment variables are' \
	'exported as following:'
echo "export PATH=$SHBuild_SysRoot/usr/bin:\$PATH"
echo "export LD_LIBRARY_PATH=$SHBuild_SysRoot/usr/lib:\$LD_LIBRARY_PATH"

echo 'Done.'

