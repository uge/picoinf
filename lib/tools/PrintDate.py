# https://docs.python.org/3/library/datetime.html#strftime-strptime-behavior

from datetime import datetime
import os
import sys

now = datetime.now()
versionStr = now.strftime("%Y-%m-%d_%H:%M:%S")

print(versionStr, end="")