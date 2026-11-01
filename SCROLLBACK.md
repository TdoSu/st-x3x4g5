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

## Planned commits

- Record architecture and rollback boundaries.
- Export and test terminal snapshots without enabling the new shortcut.
- Add the isolated vis browsing mode and its checks.
- Add the launcher, feature switch, lifecycle management, and integration checks.

Each repository is committed independently. The final integration requires both
updated executables and vis's installed Lua support files.
