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

In a table form, these cases look like the following:


cursorShape | cursorTextColor | cursorColor | Break Ligatures? | Pre-draw cursor | character color | post-draw cursor | Result | Notes
-- | -- | -- | -- | -- | -- | -- | -- | --
Filled Box |   | #rrggbb |   |   |   |   |   |
Filled Box | `null` | #rrggbb | FALSE | N/A | cell FG | #rrggbb | Solid box of #rrggbb | Current behavior
Filled Box | `#r2g2b2` | #rrggbb | TRUE | #rrggbb | #r2g2b2 | N/A | Box of #rrggbb with character in #r2g2b2 on top |
Filled Box | `textForeground` | #rrggbb | FALSE | #rrggbb | cell FG | N/A | Box of #rrggbb with character in (text FG) on top | Proposed Default
Filled Box | `textBackground` | #rrggbb | TRUE | #rrggbb | cell BG | N/A | Box of #rrggbb with character in (text BG) on top |
Vintage |   |   |   |   |   |   |   |
Vintage | `null` | #rrggbb | FALSE | N/A | cell FG | #rrggbb | ▃ of #rrggbb on top of char in (text FG) | Current behavior
Vintage | `#r2g2b2` | #rrggbb | TRUE | #rrggbb | #r2g2b2 | N/A | ▃ of #rrggbb with character in #r2g2b2 on top of ▃ |
Vintage | `textForeground` | #rrggbb | FALSE | #rrggbb | cell FG | N/A | ▃ of #rrggbb with character in (text FG) on top of ▃ | Proposed Default
Vintage | `textBackground` | #rrggbb | TRUE | #rrggbb | cell BG | N/A | ▃ of #rrggbb with character in (text BG) on top of ▃ |
Vertical Bar |   |   |   |   |   |   |   |
Vertical Bar | `null` | #rrggbb | FALSE | N/A | cell FG | #rrggbb | ┃ of #rrggbb on top of char in (text FG) | Current behavior
Vertical Bar | `#r2g2b2` | #rrggbb | TRUE | #rrggbb | #r2g2b2 | N/A | ┃ of #rrggbb with character in #r2g2b2 on top of ┃ |
Vertical Bar | `textForeground` | #rrggbb | FALSE | #rrggbb | cell FG | N/A | ┃ of #rrggbb with character in (text FG) on top of ┃ | Proposed Default
Vertical Bar | `textBackground` | #rrggbb | TRUE | #rrggbb | cell BG | N/A | ┃ of #rrggbb with character in (text BG) on top of ┃ |

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

[comment]: # Be sure to add links to references, resources, footnotes, etc.




<!-- Footnotes -->
[#3580]: https://github.com/microsoft/terminal/issues/3580


<details>

<summary>
Horrible first attempt at a solution
</summary>


```
// "the cursor background color"
"cursorColor": "#rrggbb" | "invert" | "textForeground"

// "the color to use to draw the character the cursor is over"
"cursorTextColor": null | "#rrggbb" | "textForeground" | "textBackground"
````

Scenarios:
* **A** I've got a filled box cursor chosen in my settings
  - **A.1** and I want it to display the character _on top_ of the cursor with whatever color the character usually would be
  - **A.2** and I want it to display the character _on top_ of the cursor, but with the "background color" of the character, not the FG color
    - this is like what gVim does, see [this comment](https://github.com/microsoft/terminal/issues/1203#issuecomment-618754090)
  - **A.3** I want the cursor to _invert_ the contents of the cell it's over, like conhost did
  - **A.4** I want the character the cursor is on to be drawn on top of the cursor with some color `#rrggbb`.
  - **A.5** I don't care, and I want the cursor drawn on top of the character. I'm a madman.

* **B** I've usually got a (not filled box cursor), but sometimes (an application) switches the cursor to be a filled box.
  - **B.1** I want my cursor to be come color `#rrggbb`. When it's a filled box, I want the character drawn on top of the filled box, in whatever color it's usually drawn in.
  - **B.2** I want my cursor to be come color `#rrggbb`. I want the character underneath the cursor to be drawn in another color `#r2g2b2`. When it's a filled box, I want the character drawn on top of the filled box, in`#r2g2b2`.
  - **B.2**
  - **B.2**


## Naive attempt 1:
* We'll add a second pass to re-draw the character in the new attributes after drawing the cursor box.
  - This will result in ligatures being drawn, then the cursor on top of them, then the _half_ the ligature being re-drawn, and that's bonkers bad.


cursorTextColor |  | cursorColor|  |
-- | -- | -- | --
  | `#rrggbb` | `invert `| `textForeground`
`null` | Cursor is drawn on top of the   character in #rrggbb. The character is not re-drawn. | Cursor is drawn on top of the   character in (1-(current cell BG)). The character is re-drawn in (1 -   (current cell FG) ) | Cursor is drawn on top of the   character in (current cell FG). The character is not re-drawn.
`#r2g2b2` | Cursor is drawn on top of the   character in #rrggbb. Character is re-drawn in #r2g2b2 | Cursor is drawn on top of the   character in (1-(current cell BG)). The character is re-drawn in (1 -   (#r2g2b2) ) | Cursor is drawn on top of the   character in (current cell FG). The character is re-drawn in (#r2g2b2)
`textForeground` | Cursor is drawn on top of the   character in #rrggbb.  The character is   re-drawn in (current cell FG) | Cursor is drawn on top of the   character in (1-(current cell BG)).    The character is re-drawn in (1-(current cell FG)) | Cursor is drawn on top of the   character in (current cell FG).  The   character is re-drawn in (current cell FG)
`textBackground` |   |   |

This is as far as I got in that chart before I realized it was bad.

</details>

## Better solution idea:

We could try drawing the cursor _first_.

We're going to draw the cursor in two phases - a pre-paint and a post-paint. All the other renderers will just return `S_FALSE` from the cursor pre-paint phase automatically - we absolutely don't need to implement it for them.

We'll introduce `cursorTextColor` as a setting that accepts the following values:
* `"#rrggbb"` (a color): paint the character the cursor is on in the given color
* `"textForeground"`: paint the character the cursor is on _on top of the cursor_ in the text foreground color.
* `"textBackground"`: paint the character the cursor is on _on top of the cursor_ in the text _background_ color. (This is like what gVim does, see [this comment](https://github.com/microsoft/terminal/issues/1203#issuecomment-618754090)).
* `null`: Paint the cursor _on top of the character_ always.

`null` is effectively the behavior we have now. I'm proposing we move the default _for all profiles_ to `textForeground`.

If the renderer doesn't return `S_FALSE` from painting the cursor, then it wants us to break that run at the cursor too.
Then, when we encounter the cursor cell, we'll paint the char under the cursor specially.
* We'll paint it with the cell's BG color if the `cursorTextColor` is `textBackground`
* We'll paint it with the cell's FG color if the `cursorTextColor` is `textForeground` or null
* We'll paint it with the given color if the `cursorTextColor` is `#r2g2b2`


If the cursor is any shape other than filledBox:


cursorShape | cursorTextColor | cursorColor | Break Ligatures? | Pre-draw cursor | character color | post-draw cursor | Result | Notes
-- | -- | -- | -- | -- | -- | -- | -- | --
Filled Box |   | #rrggbb |   |   |   |   |   |
Filled Box | `null` | #rrggbb | FALSE | N/A | cell FG | #rrggbb | Solid box of #rrggbb | Current behavior
Filled Box | `#r2g2b2` | #rrggbb | TRUE | #rrggbb | #r2g2b2 | N/A | Box of #rrggbb with character in #r2g2b2 on top |
Filled Box | `textForeground` | #rrggbb | FALSE | #rrggbb | cell FG | N/A | Box of #rrggbb with character in (text FG) on top | Proposed Default
Filled Box | `textBackground` | #rrggbb | TRUE | #rrggbb | cell BG | N/A | Box of #rrggbb with character in (text BG) on top |
Vintage |   |   |   |   |   |   |   |
Vintage | `null` | #rrggbb | FALSE | N/A | cell FG | #rrggbb | ▃ of #rrggbb on top of char in (text FG) | Current behavior
Vintage | `#r2g2b2` | #rrggbb | TRUE | #rrggbb | #r2g2b2 | N/A | ▃ of #rrggbb with character in #r2g2b2 on top of ▃ |
Vintage | `textForeground` | #rrggbb | FALSE | #rrggbb | cell FG | N/A | ▃ of #rrggbb with character in (text FG) on top of ▃ | Proposed Default
Vintage | `textBackground` | #rrggbb | TRUE | #rrggbb | cell BG | N/A | ▃ of #rrggbb with character in (text BG) on top of ▃ |
Vertical Bar |   |   |   |   |   |   |   |
Vertical Bar | `null` | #rrggbb | FALSE | N/A | cell FG | #rrggbb | ┃ of #rrggbb on top of char in (text FG) | Current behavior
Vertical Bar | `#r2g2b2` | #rrggbb | TRUE | #rrggbb | #r2g2b2 | N/A | ┃ of #rrggbb with character in #r2g2b2 on top of ┃ |
Vertical Bar | `textForeground` | #rrggbb | FALSE | #rrggbb | cell FG | N/A | ┃ of #rrggbb with character in (text FG) on top of ┃ | Proposed Default
Vertical Bar | `textBackground` | #rrggbb | TRUE | #rrggbb | cell BG | N/A | ┃ of #rrggbb with character in (text BG) on top of ┃ |

