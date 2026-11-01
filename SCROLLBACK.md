# Alt-q scrollback viewer

## Architecture review

The current terminal already has a per-pane history ring (`hist`, `histi`,
`histlen`) and a built-in vi copy mode. There is no externalpipe exporter.
Alt-q currently enters copy mode, so its existing binding will dispatch through
one configuration switch: `scrollbackvis = 1` selects vis, `0` keeps copy mode.

The minimal implementation has three parts:

1. st exports the focused pane's history, oldest first, followed by its active
   screen, to a private temporary file. Export ignores the current scroll offset,
   skips wide-character dummy cells, joins soft wraps, and preserves cell spaces.
   An alternate screen is exported after the existing history with a hard line
   boundary. Neither screen nor history is changed.
2. `st-scrollback-viewer` supervises a separate st process running vis on a new
   PTY, embedded as an X11 child of the original st through its existing `-w`
   support. It covers the original client area without creating a top-level
   window, and follows parent resizes.
   st owns a lifetime pipe and tracks one launcher per original window; repeated
   activation is ignored until that launcher exits. The launcher watches pipe
   EOF, cleans the snapshot, closes its viewer, and restores parent focus when
   possible. Both st and launcher remove the snapshot on normal failure paths.
3. vis provides `--scrollback` with a dedicated Lua configuration, Normal mode,
   navigation/search/selection/yank actions, and a restricted command prompt.
   Editing actions are disabled at the key-action boundary; Text and editing
   engines remain unchanged. Yanks go to X11 CLIPBOARD using vis-clipboard.

The viewer uses the original client window as its X11 parent, with origin (0, 0),
the same font and the current client dimensions. This uses the existing embedding
mechanism without changing ownership of the original shell's PTY. No terminal
multiplexer or window-manager modification is needed. The snapshot remains static.

## Commit boundaries

- Record architecture and rollback boundaries.
- Export and test terminal snapshots without enabling the new shortcut.
- Add the isolated vis browsing mode and its checks.
- Add the launcher, feature switch, lifecycle management, and integration checks.

Each repository is committed independently. The final integration requires both
updated executables and vis's installed Lua support files.

## Build and use

In the companion vis checkout:

```
./configure --enable-lua
make
make install
```

Then in this checkout:

```
make
make install
```

Choose the same installation prefix (configure's `--prefix` for vis, `PREFIX`
for st), and put its `bin` directory on PATH. st's install target installs the
small launcher alongside st. `make st` also builds the launcher. If a bare
launcher name is absent from PATH, st tries beside its resolved executable.
Linux uses `/proc/self/exe`; other systems resolve the invocation path or PATH.
Explicit launcher
paths and existing PATH entries keep their usual precedence.
Install `xclip` or `xsel` for X11 CLIPBOARD; the
launcher selects X11 even when WAYLAND_DISPLAY is present. No shell configuration
is needed. For an uninstalled checkout, prepend both checkout directories to
PATH and set `VIS_PATH` to vis's absolute `lua` directory (needed on OpenBSD,
where vis cannot discover it through `/proc/self/exe`).

`st: scrollback launcher ...: No such file or directory` means the launcher
executable is missing; the snapshot itself is automatically created by mkstemp.
Build/install the launcher as above. The separate vis checkout must also be
built, and its executable must precede an older system vis on PATH, since older
versions do not recognize `--scrollback`.

`scrollbackvis` in `config.def.h` is a global configuration variable:

- `1` (default): Alt-q opens vis. `gg`/`G`, `/`/`?`, `n`/`N`, page motions,
  `v`/`V`, and the usual yank motions are available. `y` and `yy` copy to X11
  CLIPBOARD; `:q` or `:q!` closes the viewer.
- `0`: Alt-q enters the original built-in vi copy mode with its old bindings.

If `config.h` already exists, synchronize the new configuration variables and
Alt-q binding there, or regenerate it after preserving local customizations.
There is only one Alt-q binding. Scrollback itself remains enabled in both modes;
the switch selects the browsing interface. The temporary viewer ignores Alt-q
to prevent recursive activation. The whole launch chain is marked before the
first exec; st and the launcher also reject nested invocations. A viewer st
requires the internal `--scrollback-window` flag and a matching embedding parent,
so a mistaken launcher path naming st cannot open an ordinary shell window.
Rebuild/install st and its launcher together when updating.

The snapshot contains all terminal-cell spaces (including trailing blanks and
blank rows), UTF-8 glyphs once each, and newlines at hard line boundaries. Soft
wrap markers join physical rows, including history-to-main-screen boundaries.
History and an active alternate screen have a hard boundary. Terminal control
sequences and text already discarded from the bounded ring cannot be recovered.

The snapshot is created with mkstemp, becomes mode 0400 after writing, and is
removed on normal exit, cancellation and startup failure. The launcher's lifetime
pipe closes when the original st exits; it terminates the viewer. st also reaps
the launcher and terminates its separate process group if the launcher crashes.
Clipboard providers intentionally retain selection ownership after vis exits,
until another application replaces the selection.

The original top-level window stays in place. The launcher verifies that its
child terminal actually embeds in that window and cancels startup if it does not.
It follows parent ConfigureNotify events and restores input focus on exit using
standard X11/EWMH requests. Focus may still be constrained by the desktop, but
viewer placement does not depend on a window manager. There is no dwm dependency.

## Verification

From this checkout:

```
sh tests/scrollback-snapshot.sh
sh tests/scrollback-recursion.sh
sh tests/wide-overwrite.sh
sh tests/split-state.sh
sh tests/scrollback-x11.sh
```

The snapshot test also verifies automatic temporary-file creation/cleanup,
launcher lookup beside st when PATH misses it, PATH precedence, and explicit
launcher paths. The X11 script does not add the st checkout to PATH, exercising
the sibling lookup with the real viewer. The recursion script rejects mistaken
launcher/terminal invocations before connecting to X11 or creating any window.

The X11 test requires a working DISPLAY, ksh, a C compiler and X11 headers, and a
built sibling vis checkout (override `VIS_DIR` if necessary). It runs two st
instances, one with an alternate-screen foreground program, and checks history,
same-window embedding, parent resizing, an unchanged top-level window count,
Alt-q bursts in both parent and child, resolution of a deliberately wrong argv[0],
current content, search/Visual copying, paste from another visible X11 client
after `:q`, duplicate suppression, focus, unchanged shell PID/cwd/environment,
unchanged foreground input, repeated activation, restrictive permissions, and
cleanup on failure/cancellation/launcher crash/original exit. Its test-only
xclip replacement serves actual X11 selections and avoids requiring an installed
clipboard provider just to run the test.

The embedded-viewer scenarios were also run successfully on a temporary nested
rootful Xwayland server, independently of the main desktop's window placement
and focus rules.

From the vis checkout, `python3 test/scrollback.py` checks the native PTY input
path, dedicated configuration, navigation/search/yanking, rejected editing,
rejected save/shell commands, and an unchanged snapshot after `:q`.

For manual acceptance, launch st with ksh, produce more than one screen of output,
press Alt-q, search for old output, select text with `v` and yank with `y`, then
`:q` and paste into another X11 application. Repeat with a full-screen program.
Use `scrollbackvis = 0` and rebuild to verify the original copy interface.
With that build, set `SCROLLBACK_TEST_LEGACY=1` when running the X11 script to
check the old copy selection/motions and verify they do not reach the PTY.

Validated in the implementation environment: both repository builds, snapshot
tests, existing wide-character/split-state tests, vis's PTY viewer tests, its core
suite and 65 editing cases, the X11 scenarios above, and a separate build with
`scrollbackvis = 0`. The sam suite was skipped because sam was unavailable.
OpenBSD portability follows the existing build conventions but was not executed
on an OpenBSD host.
