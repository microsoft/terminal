---
author: Mike Griese @zadjii-msft
created on: 2020-05-22
last updated: 2020-06-08
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

### Note

This spec is being written a bit retroactively. [#6337] was merged before this
spec was approved, which added support for a two-phased attempt at rendering the
cursor in DX during the rendering of each run of text. This spec now mostly
reflects new settings, and how they should appear to the user.

## Solution Design

Note that these changes only apply to the DX renderer. All other renderers will
be unaffected by this change.

We're drawing the cursor in two passes. The first pass happens before the text
is painted to the rendering target (below the character), and the second pass
happens afterwards (above the character). - a pre-paint and a post-paint.

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

`null` is effectively the behavior we had prior to [#6337]. [#6337] changed the
default behavior _for all profiles_ to `textForeground`.

For the DX renderer (the Terminal render engine), we'll modify our behavior.

* `"textForeground"`: For the `filledBox` cursor, the cursor is drawn in
  pre-paint, underneath the text from the cell. FOr all other cursors, the
  cursor is drawn on top of the character.
* `null`: Cursor is drawn in post-paint, on top of the cell, for all cursor
  chapes.
* `"#rrggbb"`: Cursor is drawn in pre-paint, underneath the text from the cell.
  The render engine will also need to indicate to the renderer that the renderer
  should manually break the text run where the cursor is. When we draw the run
  where the cursor is, we'll draw it using the the provided color (`#rrggbb`)
  instead.
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
Vintage | `#r2g2b2` | TRUE | N/A | #r2g2b2 | #rrggbb | ▃ of #rrggbb on top of character in #r2g2b2 on top of ▃ |
Vintage | `textForeground` | FALSE | N/A | cell FG | #rrggbb | ▃ of #rrggbb on top of character in (text FG) on top of ▃ | Proposed Default
Vintage | `textBackground` | TRUE | N/A | cell BG | #rrggbb | ▃ of #rrggbb on top of character in (text BG) on top of ▃ |
Vertical Bar |   |   |   |   |   |   |   |
Vertical Bar | `null` | FALSE | N/A | cell FG | #rrggbb | ┃ of #rrggbb on top of char in (text FG) | Current behavior
Vertical Bar | `#r2g2b2` | TRUE | N/A | #r2g2b2 | #rrggbb | ┃ of #rrggbb on top of character in #r2g2b2 on top of ┃ |
Vertical Bar | `textForeground` | FALSE | N/A | cell FG | #rrggbb | ┃ of #rrggbb on top of character in (text FG) on top of ┃ | Proposed Default
Vertical Bar | `textBackground` | TRUE | N/A | cell BG | #rrggbb | ┃ of #rrggbb on top of character in (text BG) on top of ┃ |

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


## Capabilities

### Accessibility

This should make it _easier_ for users to actually tell what the character under the cursor is.

### Security
N/A

### Reliability
N/A

### Compatibility

The initial PR doesn't do anything to change the behavior of other render
engines, so I don't expect any compatibility issues with them.

Once the initial PR is complete, until the second PR is finished, the user can use

```json
{ "cursorShape": "vintage", "cursorHeight": 100 }
```

to replicate the old (pre-[#6337]) behavior.

### Performance, Power, and Efficiency
N/A

## Potential Issues



## Future considerations

* This whole scenario feels a lot like how `selectionForeground` ([#3580]) should work.
* One could also imagine adding a `textForeground` value to `cursorColor`. This
  would automatically draw the cursor in the _same_ color as the text in the
  cell the cursor is on. This is a bit trickier, because we'd want to draw that
  color in the pre-paint phase, but the renderer won't know what color the cell
  under the cursor is until after the pre-paint, when the text is drawn. Maybe
  the renderer could know "ah, this text is where I thought the cursor should
  have been, I'll just draw that real quick now".
  - Adding `textBackground` to `cursorColor` seems silly - the use case would be
    "I want the cursor to be totally invisible", or "I want the cursor to be
    invisible, but change the color of the character it's on", which is weird.

## Resources

Feature Request: Show character under cursor when cursorShape is set to filledBox [#1203]


<!-- Footnotes -->
[#1203]: https://github.com/microsoft/terminal/issues/1203
[#3580]: https://github.com/microsoft/terminal/issues/3580
[#6337]: https://github.com/microsoft/terminal/pull/6337
