#!/usr/bin/env python3

import os
import sys


def Cmd(cmd):
    os.system(cmd)

def Main():
    tmpDir = os.environ.get("temp")
    scriptFile = "%s\\jlink.jls" % (tmpDir)

    fdScript = open(scriptFile, "w")
    fdScript.truncate()
    fdScript.write(
"""si 1
speed 2000
device RP2040_M0_0
connect
erase
reset
go
exit
""")
    fdScript.close()

    Cmd("JLink.exe -CommandFile %s -NoGui 1 > nul 2>&1" % (scriptFile))


Main()


