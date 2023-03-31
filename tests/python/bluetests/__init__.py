"""Placeholder package that grabs the tests for blue from
the blue source trees,
so the tests and project can be run as a normal python package.

If you are testing an existing C++ project,
just add test_*.py files to the test directory for the C++ project.

If you are adding a new C++ project,
you should add a new file to this package's python test directory.

See `tests/runtests.py` to run the tests from the commandline.
"""
try:
    import blue
except ImportError:
    import blue_debug
    import sys
    sys.modules["blue"] = blue_debug
