# coding=utf-8
################################################################################
#                                                                              #
# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.
#                                                                              #
################################################################################
# MAKE SURE YOU SAVE THIS FILE AS UTF-8!!!

"""
This is a dead simple script for testing some emoji in the console.
It's written in python, so the same script can easily be used in both a linux
terminal and the console without a lot of work.
This script has both some "simple" emoji - burrito, cheese, etc. and some more
complex ones - WOMAN COOK is actually two emoji with a zero width joiner.
"""
import sys
import time # time.sleep is in seconds
from common import *

# Run this file with:
#   python burrito.py
if __name__ == '__main__':
    clear_all()
    print('We are going to make a burrito:')
    print(u"\U0001F32F") # Burrito
    write('Here we have some components of a burrito:')
    # POULTRY LEG, CHEESE WEDGE, HOT PEPPER, TOMATO
    print(u"\U0001F357\U0001F9C0\U0001F336\U0001F345")
    # woman cook and man cook emoji
    print(u"👩‍🍳 packing them up ‍👨‍🍳")
    sgr('92')
    print(u'✔ Complete!')
    sgr('0')
    write('\n')
    print(u'🌯🌯🌯🌯🌯🌯🌯🌯🌯🌯🌯')
