#!/bin/sh
set -eu

make st
taskdir=$(mktemp -d "${TMPDIR:-/tmp}/st-recursion-test.XXXXXXXX")
trap 'rm -rf "$taskdir"' EXIT HUP INT TERM

# These checks deliberately have no X display. An accidental st invocation
# must fail before XOpenDisplay and before any window or PTY can be created.
reject_terminal() {
	if env DISPLAY= ST_SCROLLBACK_PARENT=1 ST_SCROLLBACK_LAUNCHER=1 \
		./st "$@" >"$taskdir/output" 2>&1; then
		echo 'FAIL: recursive terminal invocation accepted' >&2
		exit 1
	fi
	grep -q 'invalid or recursive scrollback terminal invocation' "$taskdir/output"
}

# A mistaken launcher path naming st, a wrapper dropping internal arguments,
# and a viewer invocation without an embedding parent.
reject_terminal snapshot 1 80x24 monospace ./st
reject_terminal -e /bin/sh
reject_terminal --scrollback-window -e /bin/sh

if env DISPLAY= ST_SCROLLBACK_LAUNCHER=1 ./st-scrollback-viewer \
	>"$taskdir/output" 2>&1; then
	echo 'FAIL: recursive launcher invocation accepted' >&2
	exit 1
fi
grep -q 'recursive launcher invocation refused' "$taskdir/output"
echo 'scrollback recursion: launcher/terminal guards reject before X11 startup'
