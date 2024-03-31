#!/usr/bin/env python3

import math
import os
import re
import signal
import sys


# call this with python3 -u

# Meant to read from stdin and monitor the output of the malloc/free PICO_DEBUG_MALLOC output.
# This is the pico_malloc.c file.
# I have tweaked that system to output free() calls as well.
#
# Read from stdin and monitor the addresses alloc'd and free'd and report outstanding
# usage.

def Commas(val):
    return "{:,}".format(val)

def _ctrl_c_handler(signal, frame):
    print('^C handler')
    sys.exit(0)

signal.signal(signal.SIGINT, handler=_ctrl_c_handler)


# malloc 80 2002AA50->2002AAA0
# malloc 80 2002AAA8->2002AAF8
# malloc 80 2002AB00->2002AB50
# free 2002AA50
# free 2002AB00

addr__size = dict()
totalAllocd = 0

def OnLine(line):
    global addr__size
    global totalAllocd

    # get rid of possible shell character
    line = line.lstrip(">").strip()

    linePartList = line.split()

    p1 = linePartList[0]

    if p1 == "malloc":
        print(f"MALLOC: \"{line}\"")
        size = int(linePartList[1])
        addr = linePartList[2].split('-')[0]

        totalAllocd += size
        print(f"malloc'd {size} bytes starting at addr {addr}")
        print(f"total alloc'd now {totalAllocd} bytes")

        if addr not in addr__size:
            addr__size[addr] = size
        else:
            print(f"malloc WARN -- addr {addr} already in cache, maybe got missed, replacing")
            addr__size[addr] = size
        
        print()
    elif p1 == "free":
        print(f"FREE: \"{line}\"")
        addr = linePartList[1]

        print(f"free'd addr {addr}")

        if addr in addr__size:
            size = addr__size[addr]
            del addr__size[addr]

            totalAllocd -= size

            print(f"  this frees {size} bytes, now {totalAllocd} bytes allocated")
        else:
            print(f"free WARN -- addr {addr} not in cache")
        
        print()
    elif p1 == "realloc":
        print(f"REALLOC: \"{line}\"")

        print()
    elif p1 == "realloc":
        print(f"CALLOC: \"{line}\"")

        print()
    else:
        pass




def Read(inFile):
    if inFile == "-":
        fd = sys.stdin
    else:
        fd = open(inFile, "r")

    line = fd.readline()
    while line:
        line = line.strip()
        # print(f"{line}")
        if line != "":
            OnLine(line)
        line = fd.readline()


def Main():
    if len(sys.argv) < 2:
        print("Usage: " +
              os.path.basename(sys.argv[0]) +
              " <inFile>")
        sys.exit(-1)

    inFile = sys.argv[1]

    Read(inFile)

Main()


