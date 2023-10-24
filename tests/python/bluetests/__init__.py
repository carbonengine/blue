"""Import blue for test code
This package import is only necessary for blue's python test suite because those cannot run through
exefile in interpreter mode (which loads blue for you).

We import the appropriate module flavor, which automagically
patches that module flavor into sys.modules as "blue", thus making it possible to "import blue"
in the test code.
"""
import os
flavor = os.environ.get("BUILDFLAVOR", "release")

if flavor == 'release':
    import blue
elif flavor == 'debug':
    import blue_debug
elif flavor == 'trinitydev':
    import blue_trinitydev
elif flavor == 'internal':
    import blue_internal
else:
    raise RuntimeError("Unknown build flavor: {}".format(flavor))
