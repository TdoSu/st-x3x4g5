# Native application dividers

This st advertises `ST_NATIVE_SEPARATORS=1` to its children. The matching vis
uses that capability when `TERM` is `st` or starts with `st-`, outside tmux and
screen. Start a new st session after rebuilding both programs. Older terminals
and multiplexers retain vis's Unicode divider fallback.

vis reserves blank cells for divider rows/columns and submits their geometry.
st paints each divider as one Xft rectangle through the same drawing function
as its own split bar: palette colour 4, centred, thickness `max(1, cell_height/12)`.
This works with `boxdraw=0` and does not depend on font glyphs or the vis theme.
The protocol does not write characters or change the terminal cursor, selection,
scrollback or PTY geometry.

The private interface uses the existing OSC 777 namespace. Each command ends
with BEL or ST:

```
OSC 777 ; st-separators ; begin BEL
OSC 777 ; st-separators ; add ; h|v ; x ; y ; length BEL
OSC 777 ; st-separators ; end BEL
OSC 777 ; st-separators ; clear BEL
```

Coordinates are zero-based terminal cells relative to the application's st
pane. `h` spans `length` columns; `v` spans `length` rows. All coordinates must
be in bounds and length must be positive and fit within that pane. Up to 256
dividers can be submitted per frame. Invalid or overflowing batches are ignored
as a whole, preserving the last committed frame. An empty committed batch or
`clear` removes the old dividers; an unfinished batch is never drawn.

Each st PTY pane keeps independent main/alternate-screen divider lists. Lines
are repainted after text and the cursor, clipped when the terminal shrinks, and
repositioned automatically when font metrics or st pane offsets change. Leaving
the alternate screen clears its dividers; terminal reset clears both lists.
vis resubmits geometry after drawing, clears it before handing the terminal to
another command or exiting, and restores its frame on return.

Validation:

```
sh tests/native-separators.sh
# From the vis checkout, on an isolated DISPLAY:
sh test/st-ui-x11.sh
```

The parser test checks atomic replacement, invalid/overflow input, text
isolation, main/alternate screens, resizing and reset. The X11 test compares
actual rectangle pixels, including theme independence, layout changes and exit
cleanup. Set `ST_UI_FALLBACK=1` to test the Unicode fallback. A separate st build
with `boxdraw=0` also passed the native pixel test with a 30px Maple Mono font.
