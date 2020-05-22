---
author: Mike Griese @zadjii-msft
created on: 2020-05-22
last updated: 2020-05-22
issue id: 1203
---
# Cursor Text Foreground Color

## Abstract

In the original conhost, the text cursor was originally drawn by inverting the
color of the cell it was over, including the color of the text under the cursor.
This made it easy for the text to stand out against the cursor, because the text
would be inverted just like the text background color.

However, with the Windows Terminal and the new DX renderer we use, it's much
_much_ harder to invert the colors of an arbitrary region of the window.

The user can currently configure the color of the cursor in the Terminal, but
the cursor is always drawn _on top_ of the cell it's on. This is because the
Terminal shares the same rendering stack as the vintage console did, where the
console needed to invert the cell as the _last_ step in the renderer. This means
that the cell under the text cursor is always totally obscured when the cursor
is in "filledBox" mode.

This spec outlines the addition of a new setting `cursorTextColor`, that would
allow the text under the cursor to be drawn in another color, so that the text
will always be visible.

## Solution Design

We're going to draw the cursor in two phases - a pre-paint and a post-paint. All
the other renderers will just return `S_FALSE` from the cursor pre-paint phase
automatically - we absolutely don't need to implement it for them.

We'll introduce `cursorTextColor` as a setting that accepts the following
values:
* `"#rrggbb"` (a color): paint the character the cursor is on in the given color
* `"textForeground"`: paint the character the cursor is on _on top of the
  cursor_ in the text foreground color.
* `"textBackground"`: paint the character the cursor is on _on top of the
  cursor_ in the text _background_ color. (This is like what gVim does, see
  [this
  comment](https://github.com/microsoft/terminal/issues/1203#issuecomment-618754090)).
* `null`: Paint the cursor _on top of the character_ always.

`null` is effectively the behavior we have now. I'm proposing we move the
default _for all profiles_ to `textForeground`.

Currently, the cursor is drawn once in the renderer, after the text is sent to
the render engine to be drawn. I propose we add an optional cursor pre-paint
step to the renderer & engine, to give the renderer a chance to draw the cursor
_before_ the text is drawn to the screen.

We're going to draw the cursor in two phases - a pre-paint and a post-paint. All
the other renderers will just return `S_FALSE` from the cursor pre-paint phase
automatically - we absolutely don't need to implement it for them.

For the DX renderer (the Terminal render engine), we'll modify our behavior.

* `null`: This is the current behavior. Cursor is drawn in post-paint, on top of
  the cell.
* `"textForeground"`: Cursor is drawn in pre-paint, underneath the text from the
  cell.
* `"#rrggbb"`: Cursor is drawn in pre-paint, underneath the text from the cell.
  The renderer will return `S_FALSE` from the cursor pre-paint phase, indicating
  we should manually break the text run where the cursor is. When we draw the
  run where the cursor is, we'll draw it using the the provided color
  (`#rrggbb`) instead.
* `"textBackground"`: This is the same as the `#rrggbb` case, but with the cell
  the cursor is on being drawn in the current background color for that cell.

In a table form, these cases look like the following. Assume that `cursorColor` is set to `#rrggbb`:

cursorShape | cursorTextColor | Break Ligatures? | Pre-draw cursor | character color | post-draw cursor | Result | Notes
-- | -- | -- | -- | -- | -- | -- | --
Filled Box |   |   |   |   |   |   |
Filled Box | `null` | FALSE | N/A | cell FG | #rrggbb | Solid box of #rrggbb | Current behavior
Filled Box | `#r2g2b2` | TRUE | #rrggbb | #r2g2b2 | N/A | Box of #rrggbb with character in #r2g2b2 on top |
Filled Box | `textForeground` | FALSE | #rrggbb | cell FG | N/A | Box of #rrggbb with character in (text FG) on top | Proposed Default
Filled Box | `textBackground` | TRUE | #rrggbb | cell BG | N/A | Box of #rrggbb with character in (text BG) on top |
Vintage |   |   |   |   |   |   |   |
Vintage | `null` | FALSE | N/A | cell FG | #rrggbb | ▃ of #rrggbb on top of char in (text FG) | Current behavior
Vintage | `#r2g2b2` | TRUE | #rrggbb | #r2g2b2 | N/A | ▃ of #rrggbb with character in #r2g2b2 on top of ▃ |
Vintage | `textForeground` | FALSE | #rrggbb | cell FG | N/A | ▃ of #rrggbb with character in (text FG) on top of ▃ | Proposed Default
Vintage | `textBackground` | TRUE | #rrggbb | cell BG | N/A | ▃ of #rrggbb with character in (text BG) on top of ▃ |
Vertical Bar |   |   |   |   |   |   |   |
Vertical Bar | `null` | FALSE | N/A | cell FG | #rrggbb | ┃ of #rrggbb on top of char in (text FG) | Current behavior
Vertical Bar | `#r2g2b2` | TRUE | #rrggbb | #r2g2b2 | N/A | ┃ of #rrggbb with character in #r2g2b2 on top of ┃ |
Vertical Bar | `textForeground` | FALSE | #rrggbb | cell FG | N/A | ┃ of #rrggbb with character in (text FG) on top of ┃ | Proposed Default
Vertical Bar | `textBackground` | TRUE | #rrggbb | cell BG | N/A | ┃ of #rrggbb with character in (text BG) on top of ┃ |

I omitted `underscore`, `emptyBox` because they're just the same as Vertical Bar
cases.

### Implementation plan


I believe this work can be broken into 3 PRs:
1. The first would simply move the Terminal to use `"cursorTextColor":
   "textForeground"` by default. This won't introduce the setting or anything -
   merely change the default behavior silently so the character appears on top
   of the cursor.
2. The second would actually introduce the `cursorTextColor` setting, accepting
   only `null` or `textForeground`. This will let users opt-in to the old
   behavior.
3. the third would introduce the `#rrggbb` and `textBackground` settings to the
   `cursorTextColor` property. This is left as a separate PR because these
   involve manually breaking runs of characters on the cell where the cursor is,
   which will require some extra plumbing.

I'd want to get both 1&2 done in the course of a single release. Ideally all 3
would be done in the course of a single release, but if only the first two are
done, then at least users can opt-out of the new behavior.

## Capabilities

### Accessibility

This should make it _easier_ for users to actually tell what the character under the cursor is.

### Security
N/A

### Reliability
N/A

### Compatibility

Renderers other than the DX renderer will all auto-return `S_FALSE` from the
cursor pre-paint phase, leaving their behavior unchanged.

### Performance, Power, and Efficiency
N/A

## Potential Issues



## Future considerations

* This whole scenario feels a lot like how `selectionForeground` ([#3580]) should work.

## Resources

Feature Request: Show character under cursor when cursorShape is set to filledBox [#1203]


<!-- Footnotes -->
[#1203]: https://github.com/microsoft/terminal/issues/1203
[#3580]: https://github.com/microsoft/terminal/issues/3580

