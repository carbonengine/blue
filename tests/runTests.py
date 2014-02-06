import sys
import os
import platform

if sys.platform.startswith("linux"):
    osName = "Linux64" if platform.architecture()[0] == "64bit" else "Linux32"
    defaultPlatformToolset = "clang++"
elif sys.platform.startswith("darwin"):
    osName = "MacOsXx86_64" if platform.architecture()[0] == "64bit" else "MacOsXi386"
    defaultPlatformToolset = "clang4.2"
else:
    osName = "Win32" if platform.architecture()[0] == "32bit" else "x64"
    defaultPlatformToolset = "v100"

try:
    platformToolset = os.environ["PlatformToolset"]
except KeyError:
    platformToolset = defaultPlatformToolset

binPath = "../../../autobuild/blue/python_27/%s/%s/" % (osName, platformToolset)

print "Binaries from", binPath
sys.path.append(binPath)

import unittest
from test_blue import *
from test_blackpersistence import *
from test_blueexposure import *
from test_copier import *
#from test_dictpersistence import *
from test_objectrecycler import *
from test_structurelist import *
from test_yamlpersistence import *
from test_paths import *
from test_resfile import *
from test_memstream import *
from test_resman import *
from test_motherlode import *

if __name__ == "__main__":
    unittest.main()