---
author: Mike Griese @zadjii-msft
created on: 2020-05-22
last updated: 2020-05-22
issue id: 1203
---


# Cursor Text Foreground Color

## Abstract

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

