#!/usr/bin/env python

import os
import sys


def Cmd(cmd):
    print(cmd)
    sys.stdout.flush()
    os.system(cmd)

def Main():
    scriptFile = os.environ["temp"] + "\jlink.jls"

    fdScript = open(scriptFile, "w")
    fdScript.truncate()


    if len(sys.argv) == 2 and sys.argv[1] == "-noreset":
        fdScript.write(
"""si 1
speed 1000
device RP2040_M0_0
connect
erase 101F6000 10200000 noreset
exit
""")
    else:
        fdScript.write(
"""si 1
speed 1000
device RP2040_M0_0
connect
erase 101F6000 10200000
exit
""")

    fdScript.close()

    Cmd(f"type {scriptFile}")
    print("\n")
    sys.stdout.flush()
    Cmd(f"jlink -CommandFile {scriptFile} -nogui 1")


Main()


