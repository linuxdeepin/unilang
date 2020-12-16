#!/usr/bin/env bash
# Requires: wget, 7z.

set -e
Unilang_BaseDir="$(cd "$(dirname "${BASH_SOURCE[0]}")"; pwd)"
YSLib_BaseDir="$Unilang_BaseDir/3rdparty/YSLib"

case $(uname) in
*MSYS* | *MINGW*)
	exit
	# Unsupported yet.
esac

LIB="$YSLib_BaseDir/YFramework/Linux/lib"

if [[ -d "$LIB" && -r "$LIB/libFreeImage.a" && -r "$LIB/libFreeImaged.a" ]]; \
then
	echo 'Archive files are detected. Skip.'
else
	echo 'Archive files are not ready.'

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

	URL_Lib_Archive_D=\
'https://osdn.net/frs/redir.php?m=tuna&f=yslib%2F73798%2FExternal-0.9.7z'
	URL_Lib_Archive=\
'https://osdn.net/frs/redir.php?m=tuna&f=yslib%2F73798%2FExternal-0.9-b902.7z'

	# XXX: Currently p7zip does not support '-si' for 7z archives.
	wget -O /tmp/External-0.9.7z "$URL_Lib_Archive_D"
	7za x -y -bsp0 -bso0 /tmp/External-0.9.7z -o"$LIB"
	wget -O /tmp/External-0.9-b902.7z "$URL_Lib_Archive"
	7za x -y -bsp0 -bso0 /tmp/External-0.9-b902.7z -o"$LIB"

	echo 'Done.'
fi

