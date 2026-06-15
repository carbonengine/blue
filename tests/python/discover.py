# Copyright © 2023 CCP ehf.

import sys
import unittest


sys.modules['blue'] = sys


class MockTasklet:
    def __init__(*args, **kwargs):
        pass

class MockTaskletExt:
    def __init__(*args, **kwargs):
        pass

class MockTaskletExit:
    def __init__(*args, **kwargs):
        pass
sys.modules['_scheduler'] = sys
# Load release scheduler during discover
import _scheduler as mod
mod.TaskletExt = MockTaskletExt
mod.TaskletExit = MockTaskletExit
mod.tasklet = MockTasklet
sys.modules['scheduler'] = mod


def print_suite(suite):
    if hasattr(suite, '_exception'):
        print(suite._exception)
        sys.exit(1)
    else:
        if not hasattr(suite, '__iter__'):
            print(suite.id())
        else:
            [print_suite(suite) for suite in suite]


if __name__ == '__main__':
    suite = unittest.defaultTestLoader.discover('.')
    print_suite(suite)
