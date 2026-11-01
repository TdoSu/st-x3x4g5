#!/bin/sh
set -eu

tmpdir=$(mktemp -d "${TMPDIR:-/tmp}/st-native-separators.XXXXXXXX")
trap 'rm -rf "$tmpdir"' EXIT HUP INT TERM

case $(uname -s) in
	OpenBSD) feature_flags='-D_XOPEN_SOURCE=600 -D_BSD_SOURCE' ;;
	*)       feature_flags='-D_XOPEN_SOURCE=600 -D_DEFAULT_SOURCE' ;;
esac

${CC:-cc} \
	-DDEBUG_WIDE $feature_flags \
	-I. -std=c99 -Wall -Wextra \
	tests/native-separators.c st.c -o "$tmpdir/native-separators" -lutil

"$tmpdir/native-separators"
