# Copyright © 2023 CCP ehf.

import io
import sys
import unittest


class TestOutputStreams(unittest.TestCase):
    """
    We used to overwrite stdout and stderr with a custom class,
    but we don't do that anymore. These tests check that we are
    using the expected type to represent stdout and stderr.
    """
    def tearDown(self):
        sys.stdout = sys.__stdout__
        sys.stderr = sys.__stderr__

    def test_stdout_is_textiowrapper_instance(self):
        if sys.stdout is not None:
            # Just in case this gets run from a process that has no shell.
            self.assertIsInstance(sys.stdout, io.TextIOWrapper)

    def test_stderr_is_textiowrapper_instance(self):
        if sys.stderr is not None:
            # Just in case this gets run from a process that has no shell.
            self.assertIsInstance(sys.stderr, io.TextIOWrapper)

    def test_stdout_can_be_set_to_none(self):
        sys.stdout = None
        self.assertIsNone(sys.stdout)

    def test_stderr_can_be_set_to_none(self):
        sys.stderr = None
        self.assertIsNone(sys.stderr)
