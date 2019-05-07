# coding=utf-8
################################################################################
#                                                                              #
# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.
#                                                                              #
################################################################################
# MAKE SURE YOU SAVE THIS FILE AS UTF-8!!!

"""
This is a longer sctipt for testing various unicode characters that someone
might want. There are a bunch of chars that are more typical, "normal"
characters, and then there's a decent number of emoji.
"""

import sys
import time # time.sleep is in seconds
from common import *

# Run this file with:
#   python test-unicode.py
if __name__ == '__main__':
    clear_all()
    print('Here\'s A bunch of chars that should work:')
    write(u'tick: ✔ ')
    write(u'cross: ✖ ')
    print(u'star: ★ ')
    write(u'square: ▇ ')
    write(u'squareSmall: ◻ ')
    print(u'squareSmallFilled: ◼ ')
    print(u'play: ▶ ')
    write(u'circle: ◯ ')
    write(u'circleFilled: ◉ ')
    write(u'circleDotted: ◌ ')
    print(u'circleDouble: ◎ ')
    write(u'circleCircle: ⓞ ')
    write(u'circleCross: ⓧ ')
    write(u'circlePipe: Ⓘ ')
    write(u'circleQuestionMark: ?⃝ ')
    print(u'bullet: ● ')
    write(u'dot: ․ ')
    write(u'line: ─ ')
    print(u'ellipsis: … ')
    print(u'pointer: ❯ ')
    print(u'pointerSmall: › ')
    write(u'info: ℹ ')
    write(u'warning: ⚠ ')
    print(u'hamburger: ☰ ')
    print(u'smiley: ㋡ ')
    print(u'mustache: ෴ ')
    print(u'heart: ♥ ')
    write(u'arrowUp: ↑ ')
    write(u'arrowDown: ↓ ')
    write(u'arrowLeft: ← ')
    print(u'arrowRight: → ')
    write(u'radioOn: ◉ ')
    print(u'radioOff: ◯ ')
    write(u'checkboxOn: ☒ ')
    print(u'checkboxOff: ☐ ')
    write(u'oneHalf: ½ ')
    write(u'oneThird: ⅓ ')
    write(u'oneQuarter: ¼ ')
    print(u'oneFifth: ⅕ ')
    write(u'oneSixth: ⅙ ')
    write(u'oneSeventh: ⅐ ')
    write(u'oneEighth: ⅛ ')
    print(u'oneNinth: ⅑ ')
    write(u'oneTenth: ⅒ ')
    write(u'twoThirds: ⅔ ')
    write(u'twoFifths: ⅖ ')
    print(u'threeQuarters: ¾ ')
    write(u'threeFifths: ⅗ ')
    write(u'threeEighths: ⅜ ')
    write(u'fourFifths: ⅘ ')
    print(u'fiveSixths: ⅚ ')
    write(u'fiveEighths: ⅝ ')
    print(u'sevenEighths: ⅞ ')

    print('Emoji:')
    write(u'beer: 🍺 ')
    print(u'burrito: 🌯 ')
    write(u'Red Heart: ❤ ')
    print(u'Fire: 🔥 ')
    write(u'Face With Tears of Joy: 😂 ')
    print(u'Smiling Face With Heart-Eyes: 😍 ')
    write(u'Thinking Face: 🤔 ')
    print(u'Smiling Face With Smiling Eyes: 😊 ')
    print(u'Smiling Face With Hearts: 🥰 ')
    write(u'Thumbs Up: 👍 ')
    write(u'Heavy Check Mark: ✔ ')
    write('\n')

