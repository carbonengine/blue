"""Import blue for test code

This package import is only necessary for blue's python test suite because those cannot run through
exefile in interpreter mode (which loads blue for you).
"""
import importlib
import sys
for flavour in ('', '_internal', '_trinitydev', '_debug'):
    try:
        mod = importlib.import_module(f"blue{flavour}")
        sys.modules["blue"] = mod
        break
    except ImportError:
        pass
else:
    raise ImportError("Could not import any flavour of blue")
