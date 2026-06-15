# Copyright © 2023 CCP ehf.

"""Import blue for test code

This package import is only necessary for blue's python test suite because those cannot run through
exefile in interpreter mode (which loads blue for you).
"""
import os
import sys

flavor = os.environ.get("BUILDFLAVOR", "release")
# need to back up sys.argv because importing blue ends up overriding it
orig_argv = sys.argv

if flavor == 'release':
    import blue as mod
elif flavor == 'debug':
    import blue_debug as mod
elif flavor == 'trinitydev':
    import blue_trinitydev as mod
elif flavor == 'internal':
    import blue_internal as mod
else:
    raise RuntimeError("Unknown build flavor: {}".format(flavor))

sys.modules["blue"] = mod
sys.argv = orig_argv
