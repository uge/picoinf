import os
import sys


def Cmd(cmd):
    os.system(cmd)

def Main():
    if len(sys.argv) < 2:
        print("Usage: " +
              os.path.basename(sys.argv[0]) +
              " <inFileElf>")
        sys.exit(-1)

    inFileElf      = sys.argv[1]

    tmpDir = os.environ.get("temp")
    scriptFile = "%s\\jlink.jls" % (tmpDir)

    fdScript = open(scriptFile, "w")
    fdScript.truncate()
    fdScript.write(
"""si 1
speed 2000
device RP2040_M0_0
connect
loadfile %s
reset
go
exit
""" % (inFileElf))
    fdScript.close()

    Cmd("JLink.exe -CommandFile %s -NoGui 1 > nul 2>&1" % (scriptFile))


Main()


