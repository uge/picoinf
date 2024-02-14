#!/usr/bin/env python3

import os
import sys
import math

# https://wiki.osdev.org/ELF
# https://www.embeddedrelated.com/showarticle/900.php
#
# TEXT = code
# RODATA = strings
# DATA = initialized data
# BSS  = zero-initialised data 
#
#    ,'''''''''''''''`.0x00000000
#    |                |
#    |     DATA       |
#    |________________|
#    |                |
#    |     BSS        |
#    |________________|
#    |       |        |
#    |       |        |
#    |       |        |
#    |       V        |
#    |     HEAP       |
#    |                |
#    |                |
#    |                |
#    |     STACK      |
#    |       ^        |
#    |       |        |
#    |       |        |
#    |       |        |RAM_MAX_ADDRESS
#    '`''''''''''''''''
#
# The executable code file has to contain the initialized values to be loaded in ram.
#
# Layout looks like this in bin/hex files.
#
#    ,'''''''''''''''''''''|FILE_START
#    |_/Interrupt\Vectors\_|
#    | \_/ \_/ \_/ \_/ \_/ |
#    |_/REST OF/CODE \_/ \_|
#    | \INSTRUCTIONS_/ \_/ |
#    .......................
#    |'    ':..:'    ':..:'|
#    |. INITIAL:VALUES:'':.|
#    |':OF VARIABLES:IN   '|
#    |.:DATA SEGMENT:.    .|
#    L_____________________|FILE_END

# flash:
# .flash_begin
# .flash_end      0x1002d108

# ram:
# .data          0x20001b00	// is this offset from 0x200...?  should I start from there?
# .bss            0x20002220
# rodata?
# .heap           0x2002be24

def ProcessFile(file):
    sectionList = [
        ".debug_info",
        ".text",
        ".rodata",
        ".data",
        ".bss",
    ]

    section__data = dict()
    for section in sectionList:
        section__data[section] = {
            "addr": 0,
            "size": 0,
        }

    startProcessing = False

    lineList = []
    with open(file) as f:
        lineList = f.read().splitlines()
    
    for line in lineList:
        if line == "Linker script and memory map":
            startProcessing = True
        elif startProcessing:
            linePartList = line.split()

            if len(linePartList) >= 3:
                section   = linePartList[0]
                addrInHex = linePartList[1]
                sizeInHex = linePartList[2]

                if section in section__data:
                    if section__data[section]["addr"] == 0 and section__data[section]["size"] == 0:
                        # print(line)
                        section__data[section]["addr"] = int(addrInHex, 0)
                        section__data[section]["size"] = int(sizeInHex, 0)


    for section in sectionList:
        addr = section__data[section]["addr"]
        size = section__data[section]["size"]
        # print(f"{section}: {addr} {size}")

    ram = section__data[".data"]["size"] + \
          section__data[".bss"]["size"]

    rom = section__data[".text"]["size"] + \
          section__data[".data"]["size"] + \
          section__data[".rodata"]["size"]

    RPI_PICO_FLASH_CAPACITY = 0x200000
    RPI_PICO_RAM_CAPACITY   = 0x040000

    pctRom = math.ceil(100.0 * rom / RPI_PICO_FLASH_CAPACITY)
    pctRam = math.ceil(100.0 * ram / RPI_PICO_RAM_CAPACITY)

    def Commas(val):
        return "{:,}".format(val)
    
    print("ROM: %9s / %9s = %2s %%" % (Commas(rom), Commas(RPI_PICO_FLASH_CAPACITY), pctRom))
    print("RAM: %9s / %9s = %2s %%" % (Commas(ram), Commas(RPI_PICO_RAM_CAPACITY), pctRam))
    print("")




def Main():
    if len(sys.argv) < 2:
        print("Usage: " +
              os.path.basename(sys.argv[0]) +
              " <inFileElfMap>")
        sys.exit(-1)

    inFile = sys.argv[1]

    ProcessFile(inFile)


Main()


