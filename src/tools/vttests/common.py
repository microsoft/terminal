################################################################################
#                                                                              #
# Copyright (c) Microsoft Corporation.
# Licensed under the MIT license.
#                                                                              #
################################################################################

import sys, os
import time # time.sleep is in seconds

if os.name == 'nt':
    import ctypes
    hOut = ctypes.windll.kernel32.GetStdHandle(-11)
    out_modes = ctypes.c_uint32()
    ENABLE_VT_PROCESSING = ctypes.c_uint32(0x0004)
    ctypes.windll.kernel32.GetConsoleMode(hOut, ctypes.byref(out_modes))
    out_modes = ctypes.c_uint32(out_modes.value | 0x0004)
    ctypes.windll.kernel32.SetConsoleMode(hOut, out_modes)
    setcp_result = ctypes.windll.kernel32.SetConsoleOutputCP(65001)
    if not setcp_result:
        gle = ctypes.windll.kernel32.GetLastError()
        print('SetConsoleOutputCP failed with error {}'.format(gle))
        exit(0)
    setcp_result = ctypes.windll.kernel32.SetConsoleCP(65001)
    if not setcp_result:
        gle = ctypes.windll.kernel32.GetLastError()
        print('SetConsoleCP failed with error {}'.format(gle))
        exit(0)
    import codecs
    codecs.register(lambda name: codecs.lookup('utf-8') if name == 'cp65001' else None)
    sys.stdout = codecs.getwriter('utf8')(sys.stdout)
    sys.stderr = codecs.getwriter('utf8')(sys.stderr)

def write(s):
    sys.stdout.write(s)

def esc(seq):
    write('\x1b{}'.format(seq))

def csi(seq):
    sys.stdout.write('\x1b[{}'.format(seq))

def osc(seq):
    sys.stdout.write('\x1b]{}\x07'.format(seq))

def cup(r=0, c=0):
    csi('H') if (r==0 and c==0) else csi('{};{}H'.format(r, c))

def cupxy(x=0, y=0):
    cup(y+1, x+1)

def margins(top=0, bottom=0):
    csi('{};{}r'.format(top, bottom))

def clear_all():
    cupxy(0,0)
    csi('2J')

def sgr(code=0):
    csi('{}m'.format(code))

def sgr_n(seq=[]):
    csi('{}m'.format(';'.join(str(code) for code in seq)))

def tbc():
    """
    Clear all tabstops from the terminal.
    Tab will take the cursor to the last column, then the first column.
    """
    csi('3g')

def ht():
    write('\t')

def cbt():
    csi('Z')

def hts(column=-1):
    if column > 0:
        csi(';{}H'.format(column))
    esc('H')

def alt_buffer():
    csi('?1049h')

def main_buffer():
    csi('?1049l')

def flush(timeout=0):
    sys.stdout.flush()
    time.sleep(timeout)

def set_color(index, r, g, b):
    osc('4;{};rgb:{:02X}/{:02X}/{:02X}'.format(index, r, g, b))

if __name__ == '__main__':
    clear_all()
    print('This is the VT Test template.')
