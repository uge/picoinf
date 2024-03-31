#!/usr/bin/env python3

import math
import os
import re
import sys




# https://gcc.gnu.org/onlinedocs/gnat_ugn/Static-Stack-Usage-Analysis.html
# https://stackoverflow.com/questions/6387614/how-to-determine-maximum-stack-usage-in-embedded-system-with-gcc

# for use in recursively reading/sorting all the .su (stack usage) files generated
# from adding add_definitions(-fstack-usage) to the build.




def Commas(val):
    return "{:,}".format(val)



FUNCTION_DATA_LIST = []



def Summarize():
    FUNCTION_DATA_LIST.sort(key=lambda x: x["bytes"])

    for funcData in FUNCTION_DATA_LIST:
        print(f"{funcData['bytes']} : {funcData['func']}")





def OnFunction(file, func, bytes, type):
    FUNCTION_DATA_LIST.append({
        "file": file,
        "func": func,
        "bytes": int(bytes),
        "type": type,
    })




def ProcessSuFile(file):
    # print(f"Processing {file}")

    lineList = []
    with open(file) as f:
        lineList = f.read().splitlines()
    
    for line in lineList:
        linePartList = line.split()

        type  = linePartList[-1]
        bytes = linePartList[-2]
        func  = " ".join(linePartList[0:-2])

        # print(f"line: {line}")
        # print(f"type: {type}")
        # print(f"bytes: {bytes}")
        # print(f"func: {func}")
        # print("")

        OnFunction(file, func, bytes, type)


def ProcessSuFilesUnder(dir):
    rootDir = os.path.abspath(dir)

    print(f"{dir} becomes {rootDir}")

    suFileList = []

    r = re.compile(".*.su$")
    for root, subdirList, fileList in os.walk(rootDir):
        # print(f"{root}")
        suFileListLocal = list(filter(r.match, fileList))

        for suFile in suFileListLocal:
            suFileFqn = os.path.join(root, suFile)
            suFileList.append(suFileFqn)
            # print(f"  Found {suFileFqn}")

    print(f"Found {len(suFileList)} files")
    # print(suFileList)

    for suFile in suFileList:
        ProcessSuFile(suFile)


def Main():
    if len(sys.argv) < 2:
        print("Usage: " +
              os.path.basename(sys.argv[0]) +
              " <dir>")
        sys.exit(-1)

    dir = sys.argv[1]

    ProcessSuFilesUnder(dir)
    Summarize()


Main()


