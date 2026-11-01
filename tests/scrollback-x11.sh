#!/bin/sh
set -eu

# Builds of both repositories (vis with Lua) are required. Uses a test-only
# xclip provider which actually owns and serves X11 CLIPBOARD after vis exits.
# Run from st-x3x4g5. Override VIS_DIR for a different vis checkout.
VIS_DIR=${VIS_DIR:-../vis}
VIS_DIR=$(cd "$VIS_DIR" && pwd)
taskdir=$(mktemp -d "${TMPDIR:-/tmp}/st-viewer-test.XXXXXXXX")
trap 'rm -rf "$taskdir"' EXIT HUP INT TERM
mkdir "$taskdir/bin" "$taskdir/snapshots"
case $(uname -s) in
	OpenBSD) feature_flags='-D_XOPEN_SOURCE=700 -D_BSD_SOURCE' ;;
	*) feature_flags='-D_XOPEN_SOURCE=700 -D_DEFAULT_SOURCE' ;;
esac
make all
${CC:-cc} $feature_flags -I/usr/X11R6/include -std=c99 -Wall -Wextra \
	tests/scrollback-x11.c -o "$taskdir/scrollback-x11" -L/usr/X11R6/lib -lX11
ln -s "$taskdir/scrollback-x11" "$taskdir/bin/xclip"
cat > "$taskdir/session.ksh" <<'KSH'
cd "${2%/*}" || exit 1
export SCROLLBACK_TEST_ENV=unchanged
before="$$:$PWD:$SCROLLBACK_TEST_ENV"
i=0
while [ "$i" -lt 60 ]; do
	printf 'HISTORY-%04d\n' "$i"
	i=$((i + 1))
done
printf '  CURRENT-SCREEN 你好\n'
"$1" --foreground "$2" "$3" || exit 1
[ "$before" = "$$:$PWD:$SCROLLBACK_TEST_ENV" ] || exit 1
printf 'unchanged\n' > "$2.state"
KSH
export VIS_PATH="$VIS_DIR/lua"
export PATH="$taskdir/bin:$(pwd):$VIS_DIR:$PATH"
"$taskdir/scrollback-x11" "$taskdir/scrollback-x11" "$taskdir/session.ksh" "$taskdir/snapshots"
