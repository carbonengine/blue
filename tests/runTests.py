# Copyright © 2014 CCP ehf.

from test_blackpersistence import *
from test_blue import *
from test_blueexposure import *
from test_bluelist import *
from test_callbacks import *
from test_copier import *
from test_dictpersistence import *
from test_filestream import *
from test_functions import *
from test_marshal import *
from test_md5 import *
from test_memstream import *
from test_motherlode import *
from test_objectrecycler import *
from test_optionalargs import *
from test_paths import *
from test_percentile_accumulator import *
from test_properties import *
from test_remotefilecache import *
from test_resfile import *
from test_resman import *
from test_standaloneblueexposure import *
from test_structurelist import *
from test_sysinfo import *
from test_yamlpersistence import *


if __name__ == "__main__":
    import scheduler
    import blue
    tasklet = scheduler.tasklet(unittest.main)
    while tasklet.alive:
        blue.os.Pump()
