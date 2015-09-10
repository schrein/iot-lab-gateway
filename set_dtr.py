#! /usr/bin/env python
# -*- coding:utf-8 -*-

import os
import sys
import signal
import termios
import fcntl
import struct

DTR_ON = termios.TIOCMBIS
TIOCM_DTR_STR = struct.pack('I', termios.TIOCM_DTR)


def set_dtr(tty):
    flags = os.O_WRONLY | os.O_NOCTTY | os.O_NONBLOCK

    flags = os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK  # TODO

    ttyfd = os.open(tty, flags)

    fcntl.ioctl(ttyfd, DTR_ON, TIOCM_DTR_STR)
    while 1:
        ret = os.read(ttyfd, 100)
        print os.fstat(ttyfd)

        print '%r' % ret

    return 0


def main():
    try:
        tty = sys.argv[1]
    except IndexError:
        print >> sys.stderr, "Usage: %s <tty_path>" % sys.argv[0]
        exit(1)
    ret = set_dtr(tty)
    exit(ret)


if __name__ == '__main__':
    main()
