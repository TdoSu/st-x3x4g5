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
2. `st-scrollback-viewer` supervises a separate st running vis on a new PTY.
   st owns a lifetime pipe and tracks one launcher per original window; repeated
   activation is ignored until that launcher exits. The launcher watches pipe
   EOF, cleans the snapshot, closes its viewer, and restores parent focus when
   possible. Both st and launcher remove the snapshot on normal failure paths.
3. vis provides `--scrollback` with a dedicated Lua configuration, Normal mode,
   navigation/search/selection/yank actions, and a restricted command prompt.
   Editing actions are disabled at the key-action boundary; Text and editing
   engines remain unchanged. Yanks go to X11 CLIPBOARD using vis-clipboard.

The viewer requests the original window's location, size, font and transient
relationship using X11. Window managers may override placement and focus. Exact
same-window switching would require substantially changing terminal ownership;
V1 deliberately uses a separate window and does not touch the original PTY.

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
small launcher alongside st. Install `xclip` or `xsel` for X11 CLIPBOARD; the
launcher selects X11 even when WAYLAND_DISPLAY is present. No shell configuration
is needed. For an uninstalled checkout, prepend both checkout directories to
PATH and set `VIS_PATH` to vis's absolute `lua` directory (needed on OpenBSD,
where vis cannot discover it through `/proc/self/exe`).

`scrollbackvis` in `config.def.h` is a global configuration variable:

- `1` (default): Alt-q opens vis. `gg`/`G`, `/`/`?`, `n`/`N`, page motions,
  `v`/`V`, and the usual yank motions are available. `y` and `yy` copy to X11
  CLIPBOARD; `:q` or `:q!` closes the viewer.
- `0`: Alt-q enters the original built-in vi copy mode with its old bindings.

If `config.h` already exists, synchronize the new configuration variables and
Alt-q binding there, or regenerate it after preserving local customizations.
There is only one Alt-q binding. Scrollback itself remains enabled in both modes;
the switch selects the browsing interface. The temporary viewer ignores Alt-q
to prevent recursive activation.

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

Window placement, transient hints and focus use standard X11/EWMH requests.
The viewer requests the original dimensions and font. Negative desktop positions
are clamped to zero because st's geometry syntax interprets minus as an offset
from the far screen edge. A window manager can override placement/focus; there
is no dwm dependency. V1 is a separate window with a static snapshot.

## Verification

From this checkout:

```
sh tests/scrollback-snapshot.sh
sh tests/wide-overwrite.sh
sh tests/split-state.sh
sh tests/scrollback-x11.sh
```

The X11 test requires a working DISPLAY, ksh, a C compiler and X11 headers, and a
built sibling vis checkout (override `VIS_DIR` if necessary). It runs two st
instances, one with an alternate-screen foreground program, and checks history,
current content, search/Visual copying, paste from another visible X11 client
after `:q`, duplicate suppression, focus, unchanged shell PID/cwd/environment,
unchanged foreground input, repeated activation, restrictive permissions, and
cleanup on failure/cancellation/launcher crash/original exit. Its test-only
xclip replacement serves actual X11 selections and avoids requiring an installed
clipboard provider just to run the test.

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
